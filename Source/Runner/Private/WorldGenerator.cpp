// Fill out your copyright notice in the Description page of Project Settings.

#include "WorldGenerator.h"
#include "Algo/MinElement.h"
#include "Async/Async.h"
#include "Async/TaskGraphInterfaces.h"
#include "BarrierSpawner.h"
#include "Containers/Array.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "HAL/Platform.h"
#include "Kismet/GameplayStatics.h"
#include "KismetProceduralMeshLibrary.h"
#include "Math/Color.h"
#include "Math/MathFwd.h"
#include "ProceduralMeshComponent.h"
#include "Templates/Tuple.h"
#include <cstdint>
#include <functional>
#include <random>

// 从 Async.cpp 中 copy 过来的
class FPCGAsyncGraphTask : public FAsyncGraphTaskBase
{
public:
	/** Creates and initializes a new instance. */
	FPCGAsyncGraphTask(ENamedThreads::Type InDesiredThread, TUniqueFunction<void()>&& InFunction)
			: DesiredThread(InDesiredThread)
			, Function(MoveTemp(InFunction))
	{
	}

	/** Performs the actual task. */
	void DoTask(ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
	{
		Function();
	}

	/** Returns the name of the thread that this task should run on. */
	ENamedThreads::Type GetDesiredThread()
	{
		return DesiredThread;
	}

private:
	/** The thread to execute the function on. */
	ENamedThreads::Type DesiredThread;

	/** The function to execute on the Task Graph. */
	TUniqueFunction<void()> Function;
};

// 整体的结构是这样：最小的单元是一个 Cell，它由两个三角形组成，XCellNumber * YCellNumber 个 Cell
// 定义组成了一个 Tile。Tile 是 ProeduralMeshComp 管理的基本单位。但我发现当生成的顶点坐标距离 ProceduralMeshComp
// 很远时（250000 cm）时，会出现浮点精度问题，导致材质被拉伸。因此定义一个更上层的结构 Region，Region 由 Tile 组成，每
// 个 ProceduralMeshComp 管理一个 Region。Region 的大小是 RegionSize
// 世界中最多同时存在 4 个 Region，使用 LRU 算法来对 Region 进行替换

// Sets default values
AWorldGenerator::AWorldGenerator()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
	// ProceduralMeshComp = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("ProceduralMeshComp"));
	// RootComponent = ProceduralMeshComp;
	// ProceduralMeshComp->bUseAsyncCooking = true;

	PerlinFreq = { 0.1f, 0.2f, 0.3f };
	PerlinAmplitude = { 1.0f, 0.5f, 0.25f };
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("RootComponent"));

	for (int32 i = 0; i < MaxThreadCount; ++i)
	{
		TilesInBuilding[i] = FInt32Point(INT32_MAX, INT32_MAX); // Initialize to an invalid tile
	}
}

// Called when the game starts or when spawned
void AWorldGenerator::BeginPlay()
{
	InitDataBuffer();
	// SpawnBarrierSpawners();

	Super::BeginPlay();
}

void AWorldGenerator::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);
	for (int i = 0; i < MaxThreadCount; ++i)
	{
		if (AsyncTaskRef[i].IsValid())
		{
			AsyncTaskRef[i]->Wait();
			AsyncTaskRef[i]->Release();
			AsyncTaskRef[i] = nullptr;
		}
	}
}

// Called every frame
void AWorldGenerator::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	GenerateNewTiles();

	if (bDebugMode)
	{
		DebugPrint();
	}
}

void AWorldGenerator::DebugPrint() const
{
	auto* Character = UGameplayStatics::GetPlayerCharacter(this, 0);
	if (!Character)
	{
		return; // No player character found
	}
	auto Pos = Character->GetActorLocation();

	auto Tile = GetTileFromHorizontalPos(FVector2D(Pos.X, Pos.Y));
	auto TileXSize = CellSize * XCellNumber;
	auto TileYSize = CellSize * YCellNumber;

	auto TileStartX = Tile.X * TileXSize;
	auto TileStartY = Tile.Y * TileYSize;
	auto UVX = (Pos.X - TileStartX) / TileXSize;
	auto UVY = (Pos.Y - TileStartY) / TileYSize;
	FVector2D UV = FVector2D(UVX, UVY);

	FVector2D BarycentricCoords;
	auto TriangleIndex = GetTriangleFromUV(UV, BarycentricCoords);

	UProceduralMeshComponent* PMC = nullptr;
	int32 PMCIndex = -1;
	Tie(PMC, PMCIndex) = GetPMCFromHorizontalPos(FVector2D(Pos.X, Pos.Y));

	auto MeshSection = TileMap[PMCIndex].Find(Tile);
	if (MeshSection == INDEX_NONE)
	{
		return; // Tile not found
	}

	auto* MeshInfo = PMC->GetProcMeshSection(MeshSection);
	if (!MeshInfo)
	{
		return; // Mesh section not found
	}
	auto V1 = MeshInfo->ProcIndexBuffer[TriangleIndex * 3 + 0];
	auto V2 = MeshInfo->ProcIndexBuffer[TriangleIndex * 3 + 1];
	auto V3 = MeshInfo->ProcIndexBuffer[TriangleIndex * 3 + 2];

	auto Vertex1 = MeshInfo->ProcVertexBuffer[V1].Position + PMC->GetComponentLocation();
	auto Vertex2 = MeshInfo->ProcVertexBuffer[V2].Position + PMC->GetComponentLocation();
	auto Vertex3 = MeshInfo->ProcVertexBuffer[V3].Position + PMC->GetComponentLocation();

	auto UV1 = MeshInfo->ProcVertexBuffer[V1].UV0;
	auto UV2 = MeshInfo->ProcVertexBuffer[V2].UV0;
	auto UV3 = MeshInfo->ProcVertexBuffer[V3].UV0;

	UE_LOG(LogTemp, Log, TEXT("Player Position: %s, V1 Pos: %s, V1 UV: %s, V2 Pos: %s, V2 UV: %s, V3 Pos: %s, V3 UV: %s"),
			*Pos.ToString(), *Vertex1.ToString(), *UV1.ToString(), *Vertex2.ToString(), *UV2.ToString(), *Vertex3.ToString(), *UV3.ToString());
}

TPair<UProceduralMeshComponent*, int32> AWorldGenerator::GetPMCFromHorizontalPos(FVector2D Pos) const
{
	FInt32Point Region = GetRegionFromHorizontalPos(Pos);
	int32 ReplacableIndex = -1;
	for (int32 i = 0; i < MaxRegionCount; ++i)
	{
		UProceduralMeshComponent* PMC = ProceduralMeshComp[i];
		if (!PMC)
		{
			ReplacableIndex = i;
		}
		else if (GetRegionFromPMC(PMC) == Region)
		{
			VersionNumber[i] = ++CurrentVersionIndex; // Update version number
			return { PMC, i };												// Return the existing PMC for this region
		}
	}
	if (ReplacableIndex != -1)
	{
		auto* NonConstThis = const_cast<AWorldGenerator*>(this);
		auto* PMC = NewObject<UProceduralMeshComponent>(NonConstThis, UProceduralMeshComponent::StaticClass(), NAME_None);
		UE_LOG(LogTemp, Warning, TEXT("Creating new PMC for region: %s at index %d, input position: %s"), *Region.ToString(), ReplacableIndex, *Pos.ToString());
		PMC->bUseAsyncCooking = true; // 重中之重！！
		PMC->RegisterComponent();
		PMC->SetWorldLocation(FVector(Region.X * RegionSize, Region.Y * RegionSize, 0.0f));
		PMC->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepWorldTransform);
		ProceduralMeshComp[ReplacableIndex] = PMC;							// Replace the PMC at the found index
		VersionNumber[ReplacableIndex] = ++CurrentVersionIndex; // Update version number
		return { PMC, ReplacableIndex };
	}
	else
	{
		int64 MinVersion = INT64_MAX;
		for (int32 i = 0; i < MaxRegionCount; ++i)
		{
			if (VersionNumber[i] < MinVersion)
			{
				MinVersion = VersionNumber[i];
				ReplacableIndex = i;
			}
		}
		UProceduralMeshComponent* PMC = ProceduralMeshComp[ReplacableIndex];
		auto OldRegion = GetRegionFromPMC(PMC);
		UE_LOG(LogTemp, Warning, TEXT("Replacing PMC for region: %s with new region: %s at index %d, input position: %s"),
				*OldRegion.ToString(), *Region.ToString(), ReplacableIndex, *Pos.ToString());
		PMC->ClearAllMeshSections();
		PMC->SetWorldLocation(FVector(Region.X * RegionSize, Region.Y * RegionSize, 0.0f));

		for (auto Tile : TileMap[ReplacableIndex])
		{
			for (ABarrierSpawner* Spawner : BarrierSpawners)
			{
				Spawner->RemoveTile(Tile);
			}
		}
		TileMap[ReplacableIndex].Empty(); // Clear the tile map for this PMC

		VersionNumber[ReplacableIndex] = ++CurrentVersionIndex;
		return { PMC, ReplacableIndex };
	}
}

// void AWorldGenerator::SpawnBarrierSpawners()
// {
// 	for (auto SpawnerClass : BarrierSpawnerClasses)
// 	{
// 		if (SpawnerClass)
// 		{
// 			auto* Spawner = GetWorld()->SpawnActor<ABarrierSpawner>(SpawnerClass);
// 			if (Spawner)
// 			{
// 				BarrierSpawners.Add(Spawner);
// 			}
// 		}
// 	}
// }

void AWorldGenerator::InitDataBuffer()
{
	for (int32 i = 0; i < MaxThreadCount; ++i)
	{
		TaskDataBuffers[i].VerticesBuffer.SetNumUninitialized((XCellNumber + 1) * (YCellNumber + 1));
		TaskDataBuffers[i].NormalsBuffer.SetNumUninitialized((XCellNumber + 1) * (YCellNumber + 1));
		TaskDataBuffers[i].UV0Buffer.SetNumUninitialized((XCellNumber + 1) * (YCellNumber + 1));
		TaskDataBuffers[i].TangentsBuffer.SetNumUninitialized((XCellNumber + 1) * (YCellNumber + 1));
	}

	TrianglesBuffer.SetNumUninitialized(XCellNumber * YCellNumber * 6);
	for (int32 Y = 0; Y < YCellNumber; ++Y)
	{
		for (int32 X = 0; X < XCellNumber; ++X)
		{
			int32 Index = (Y * XCellNumber + X) * 6;
			// 第一个 13
			//       2
			TrianglesBuffer[Index] = (Y * (XCellNumber + 1)) + X;
			TrianglesBuffer[Index + 1] = ((Y + 1) * (XCellNumber + 1)) + X;
			TrianglesBuffer[Index + 2] = (Y * (XCellNumber + 1)) + X + 1;

			// 第二个  2
			//       31
			TrianglesBuffer[Index + 3] = ((Y + 1) * (XCellNumber + 1)) + X + 1;
			TrianglesBuffer[Index + 4] = (Y * (XCellNumber + 1)) + X + 1;
			TrianglesBuffer[Index + 5] = ((Y + 1) * (XCellNumber + 1)) + X;
		}
	}
}

int32 AWorldGenerator::FindReplaceableSection(int32 PMCIndex)
{
	for (int32 idx = 0; idx < TileMap[PMCIndex].Num(); ++idx)
	{
		if (CanRemoveTile(TileMap[PMCIndex][idx]))
		{
			return idx; // Found a valid section to replace
		}
	}
	return -1; // No valid section found
}

FInt32Point AWorldGenerator::GetPlayerTile() const
{
	auto* Character = UGameplayStatics::GetPlayerCharacter(this, 0);
	if (!Character)
	{
		return FInt32Point::ZeroValue; // No player character found
	}
	auto PlayerLocation = Character->GetActorLocation();
	auto Tile = GetTileFromHorizontalPos(FVector2D(PlayerLocation.X, PlayerLocation.Y));
	if (bOneLineMode)
	{
		Tile.Y = 0; // 在一行模式下，Y 坐标固定为 0
	}
	return Tile;
}

bool AWorldGenerator::IsNeccessrayTile(FInt32Point Tile) const
{
	auto PlayerTile = GetPlayerTile();
	if (bOneLineMode)
	{
		// 在一行模式下，只检查 X 坐标
		return Tile.X >= PlayerTile.X - 1 && Tile.X <= PlayerTile.X + 3;
	}
	if (Tile.X >= PlayerTile.X - 1 && Tile.X <= PlayerTile.X + 1 && Tile.Y >= PlayerTile.Y - 1 && Tile.Y <= PlayerTile.Y + 1)
	{
		return true; // Tile is within the player's vicinity
	}
	return false;
}

bool AWorldGenerator::CanRemoveTile(FInt32Point Tile) const
{
	auto PlayerTile = GetPlayerTile();
	if (bOneLineMode)
	{
		// 在一行模式下，只检查 X 坐标
		return Tile.X < PlayerTile.X - 1 || Tile.X > PlayerTile.X + 3;
	}
	if (Tile.X >= PlayerTile.X - 2 && Tile.X <= PlayerTile.X + 2 && Tile.Y >= PlayerTile.Y - 2 && Tile.Y <= PlayerTile.Y + 2)
	{
		return false; // Tile is still within the player's vicinity
	}
	return true;
}

FVector2D AWorldGenerator::GetUVFromPosAnyThread(FVector Position) const
{
	double X = Position.X / (TextureSize.X);
	double Y = Position.Y / (TextureSize.Y);
	// X = FMath::Fmod(X + UVOffset.X, MaxTextureCoords);
	// Y = FMath::Fmod(Y + UVOffset.Y, MaxTextureCoords);

	X = FMath::Fmod(X, MaxTextureCoords);
	Y = FMath::Fmod(Y, MaxTextureCoords);

	return FVector2D(X, Y);
}

double AWorldGenerator::GetHeightFromPerlinAnyThread(FVector2D Pos, FInt32Point CellPos) const
{
	if (PerlinAmplitude.Num() != PerlinFreq.Num())
	{
		// UE_LOG(LogTemp, Warning, TEXT("PerlinFreq and PerlinAmplitude arrays must have the same length!"));
		return 0.0;
	}
	double PerlinXOffset = 1 / FMath::Sqrt(2.0);
	double PerlinYOffset = 1 / FMath::Sqrt(3.0);
	auto Offset = FVector2D(FMath::Frac(CellPos.X * PerlinXOffset), FMath::Frac(CellPos.Y * PerlinYOffset));
	auto NewPos = FVector2D(Pos.X + Offset.X, Pos.Y + Offset.Y);
	double Height = 0.0;
	for (int32 i = 0; i < PerlinFreq.Num(); ++i)
	{
		Height += FMath::PerlinNoise2D(NewPos * PerlinFreq[i]) * PerlinAmplitude[i];
	}
	return Height;
}

void AWorldGenerator::CreateMeshFromTileData()
{
	for (int32 i = 0; i < MaxThreadCount; ++i)
	{
		if (BufferStateGameThreadOnly[i] == EBufferState::Completed)
		{
			auto& TaskData = TaskDataBuffers[i];
			auto& VerticesBuffer = TaskData.VerticesBuffer;
			auto& NormalsBuffer = TaskData.NormalsBuffer;
			auto& UV0Buffer = TaskData.UV0Buffer;
			auto& TangentsBuffer = TaskData.TangentsBuffer;
			auto& RandomPoints = TaskData.RandomPoints;

			auto Tile = TilesInBuilding[i];
			auto TestPos = FVector2D(Tile.X * CellSize * XCellNumber + (CellSize * XCellNumber / 2.0),
					Tile.Y * CellSize * YCellNumber + (CellSize * YCellNumber / 2.0));
			UProceduralMeshComponent* PMC = nullptr;
			int32 PMCIndex = -1;
			Tie(PMC, PMCIndex) = GetPMCFromHorizontalPos(FVector2D(TestPos.X, TestPos.Y));

			auto SectionIdx = FindReplaceableSection(PMCIndex);
			// See https://forums.unrealengine.com/t/procedural-mesh-does-not-update-collision-after-modifying-vertices/68174/2
			// TODO: https://github.com/TriAxis-Games/RealtimeMeshComponent
			// https://github.com/TriAxis-Games/RuntimeMeshComponent-Examples
			if (SectionIdx == -1)
			{
				// Create a new section
				PMC->CreateMeshSection(TileMap[PMCIndex].Num(), VerticesBuffer, TrianglesBuffer, NormalsBuffer, UV0Buffer, TArray<FColor>(), TangentsBuffer, true);
				PMC->SetMaterial(TileMap[PMCIndex].Num(), TileMaterial);
				SectionIdx = TileMap[PMCIndex].Add(Tile);
			}
			else
			{
				// Update the existing section
				PMC->ClearMeshSection(SectionIdx);
				PMC->CreateMeshSection(SectionIdx, VerticesBuffer, TrianglesBuffer, NormalsBuffer, UV0Buffer, TArray<FColor>(), TangentsBuffer, true);
				PMC->SetMaterial(SectionIdx, TileMaterial);
				auto OldTile = TileMap[PMCIndex][SectionIdx];
				// 通知 BarrierSpawner 移除旧的 tile 上的障碍物
				for (ABarrierSpawner* BarrierSpawner : BarrierSpawners)
				{
					BarrierSpawner->RemoveTile(OldTile);
				}
				TileMap[PMCIndex][SectionIdx] = Tile;
			}
			auto NewTile = TileMap[PMCIndex][SectionIdx];
			if (BarrierSpawners.Num() > 0)
			{
				// Spawn barriers using the first available barrier spawner
				BarrierSpawners[0]->SpawnBarriers(RandomPoints, NewTile, this);
			}

			BufferStateGameThreadOnly[i] = EBufferState::Idle; // Reset the buffer state
			AsyncTaskRef[i]->Release();
			AsyncTaskRef[i] = nullptr;
			TilesInBuilding[i] = FInt32Point(INT32_MAX, INT32_MAX); // Reset the tile in building
		}
	}
}

bool AWorldGenerator::GenerateOneTile(FInt32Point Tile)
{
	for (int32 i = 0; i < MaxThreadCount; ++i)
	{
		if (TilesInBuilding[i] == Tile)
		{
			return true;
		}
	}

	int32 BufferIndex = 0;
	for (; BufferIndex < MaxThreadCount; ++BufferIndex)
	{
		if (BufferStateGameThreadOnly[BufferIndex] == EBufferState::Idle)
		{
			BufferStateGameThreadOnly[BufferIndex] = EBufferState::Busy;
			break;
		}
	}
	if (BufferIndex == MaxThreadCount)
	{
		UE_LOG(LogTemp, Warning, TEXT("All buffers are busy, cannot generate new tile at %s"), *Tile.ToString());
		return false; // All buffers are busy
	}

	double XOffset = (double)Tile.X * CellSize * XCellNumber;
	double YOffset = (double)Tile.Y * CellSize * YCellNumber;

	auto TestPos = FVector2D(XOffset + (double)CellSize * XCellNumber / 2.0, YOffset + (double)CellSize * YCellNumber / 2.0);
	UProceduralMeshComponent* PMC = nullptr;
	int32 PMCIndex = -1;
	Tie(PMC, PMCIndex) = GetPMCFromHorizontalPos(TestPos);

	auto PosOffset = FVector2D(PMC->GetComponentLocation());

	auto Lambda = [this, PosOffset, Tile, BufferIndex]() {
		// Generate the tile mesh data
		GenerateOneTileAsync(BufferIndex, Tile, PosOffset);
	};
	AsyncTaskRef[BufferIndex] = TGraphTask<FPCGAsyncGraphTask>::CreateTask().ConstructAndDispatchWhenReady(ENamedThreads::AnyThread, MoveTemp(Lambda));
	TilesInBuilding[BufferIndex] = Tile; // Store the tile for this buffer
	return true;
}

void AWorldGenerator::GenerateNewTiles()
{
	// Get the player location
	auto* Character = UGameplayStatics::GetPlayerCharacter(this, 0);
	if (!Character)
	{
		return;
	}

	auto PlayerTile = GetPlayerTile();

	CreateMeshFromTileData(); // Process any completed tile data
	// Generate new tiles around the player
	if (bOneLineMode)
	{
		// 在一行模式下，只生成玩家所在行的 tile
		for (int32 X = PlayerTile.X - 1; X <= PlayerTile.X + 3; ++X)
		{
			FInt32Point Tile(X, PlayerTile.Y);
			if (IsNeccessrayTile(Tile) && !IsValidTile(Tile))
			{
				if (!GenerateOneTile(Tile))
				{
					// 如果不能生成新的 tile，可能是因为所有的缓冲区都在忙碌中
					break;
				}
			}
		}
		return;
	}
	else
	{

		for (int32 Y = PlayerTile.Y - 1; Y <= PlayerTile.Y + 1; ++Y)
		{
			for (int32 X = PlayerTile.X - 1; X <= PlayerTile.X + 1; ++X)
			{
				FInt32Point Tile(X, Y);
				if (IsNeccessrayTile(Tile) && !IsValidTile(Tile))
				{
					if (!GenerateOneTile(Tile))
					{
						// 如果不能生成新的 tile，可能是因为所有的缓冲区都在忙碌中
						break;
					}
				}
			}
		}
	}
}

int32 AWorldGenerator::GetBarrierCountForTileAnyThread(FInt32Point Tile) const
{
	int64 Seed = ((int64)Tile.X << 32) | (int64)Tile.Y;
	std::hash<int64> Int64Hash;
	auto HashValue = Int64Hash(Seed) % (MaxBarrierCount - MinBarrierCount + 1) + MinBarrierCount;
	return (int32)HashValue;
}

FInt32Point AWorldGenerator::GetTileFromHorizontalPos(FVector2D Pos) const
{
	auto TileXSize = CellSize * XCellNumber;
	auto TileYSize = CellSize * YCellNumber;
	return FInt32Point(FMath::FloorToInt(Pos.X / TileXSize), FMath::FloorToInt(Pos.Y / TileYSize));
}

// 一个 tile 中三角形的排布是 ----> X 方向， | Y 方向，从左上角开始，先从左往右编号递增，，再从上往下编号递增
// 在一个方格内，先是左上角的三角形，然后是右下角的三角形
// 第一个三角形 13
//            2
// 第二个三角形  2
//            31

// 返回三角形的索引以及三角形前两个顶点的重心坐标
int32 AWorldGenerator::GetTriangleFromUV(FVector2D UV, FVector2D& BarycentricCoords) const
{
	// UV 范围是 [0, 1]，需要转换为 [0, XCellNumber] 和 [0, YCellNumber]
	double X = UV.X * XCellNumber;
	double Y = UV.Y * YCellNumber;

	int32 TriangleX = FMath::Clamp(FMath::FloorToInt(X), 0, XCellNumber - 1);
	int32 TriangleY = FMath::Clamp(FMath::FloorToInt(Y), 0, YCellNumber - 1);
	// 判断是左上角的三角形还是右下角的三角形
	double CoordX = X - TriangleX;
	double CoordY = Y - TriangleY;
	bool bBottomTriangle = CoordX + CoordY > 1.0;

	// 对于等腰直角三角形可以通过面积来计算重心坐标
	double Bary2 = bBottomTriangle ? 1 - CoordY : CoordY;
	double Bary3 = bBottomTriangle ? 1 - CoordX : CoordX;
	BarycentricCoords = FVector2D(1 - Bary2 - Bary3, Bary2);

	return (TriangleY * XCellNumber + TriangleX) * 2 + bBottomTriangle;
}

FVector AWorldGenerator::GetWorldPositionFromUV(FVector2D UV, FInt32Point Tile) const
{
	auto TestPos = FVector2D((Tile.X + 0.5) * CellSize * XCellNumber, (Tile.Y + 0.5) * CellSize * YCellNumber);
	UProceduralMeshComponent* PMC = nullptr;
	int32 PMCIndex = -1;
	Tie(PMC, PMCIndex) = GetPMCFromHorizontalPos(TestPos);
	auto MeshSection = TileMap[PMCIndex].Find(Tile);
	if (MeshSection == INDEX_NONE)
	{
		return FVector::ZeroVector; // Tile not found
	}

	auto* MeshInfo = PMC->GetProcMeshSection(MeshSection);
	if (!MeshInfo)
	{
		return FVector::ZeroVector; // Mesh section not found
	}
	FVector2D BarycentricCoords;
	auto TriangleIndex = GetTriangleFromUV(UV, BarycentricCoords);
	auto V1 = MeshInfo->ProcIndexBuffer[TriangleIndex * 3 + 0];
	auto V2 = MeshInfo->ProcIndexBuffer[TriangleIndex * 3 + 1];
	auto V3 = MeshInfo->ProcIndexBuffer[TriangleIndex * 3 + 2];

	auto Vertex1 = MeshInfo->ProcVertexBuffer[V1].Position;
	auto Vertex2 = MeshInfo->ProcVertexBuffer[V2].Position;
	auto Vertex3 = MeshInfo->ProcVertexBuffer[V3].Position;

	auto WPos = BarycentricCoords.X * Vertex1 + BarycentricCoords.Y * Vertex2 + (1 - BarycentricCoords.X - BarycentricCoords.Y) * Vertex3;

	return WPos + PMC->GetComponentLocation(); // Add the component location to get the world position
}

double AWorldGenerator::GetHeightFromHorizontalPos(FVector2D Pos) const
{
	// 计算 UV 坐标
	auto Tile = GetTileFromHorizontalPos(Pos);
	auto TileXSize = CellSize * XCellNumber;
	auto TileYSize = CellSize * YCellNumber;

	auto TileStartX = Tile.X * TileXSize;
	auto TileStartY = Tile.Y * TileYSize;
	auto UVX = (Pos.X - TileStartX) / TileXSize;
	auto UVY = (Pos.Y - TileStartY) / TileYSize;
	FVector2D UV = FVector2D(UVX, UVY);

	auto WorldPos = GetWorldPositionFromUV(UV, Tile);
	return WorldPos.Z; // 返回高度
}

void AWorldGenerator::GenerateOneTileAsync(int32 BufferIndex, FInt32Point Tile, FVector2D PositionOffset)
{
	// 在这里执行异步生成逻辑
	auto& TaskData = TaskDataBuffers[BufferIndex];
	auto& VerticesBuffer = TaskData.VerticesBuffer;
	auto& UV0Buffer = TaskData.UV0Buffer;
	auto& NormalsBuffer = TaskData.NormalsBuffer;
	auto& TangentsBuffer = TaskData.TangentsBuffer;

	double XOffset = (double)Tile.X * CellSize * XCellNumber;
	double YOffset = (double)Tile.Y * CellSize * YCellNumber;

	for (int32 Y = 0; Y <= YCellNumber; ++Y)
	{
		for (int32 X = 0; X <= XCellNumber; ++X)
		{
			FVector VertexPosition(double(X) * CellSize + XOffset, double(Y) * CellSize + YOffset, 0.0);
			VertexPosition.Z = GetHeightFromPerlinAnyThread(FVector2D(VertexPosition.X, VertexPosition.Y), FInt32Point(Tile.X * XCellNumber + X, Tile.Y * YCellNumber + Y));
			VerticesBuffer[Y * (XCellNumber + 1) + X] = FVector(VertexPosition.X - PositionOffset.X, VertexPosition.Y - PositionOffset.Y, VertexPosition.Z);
			UV0Buffer[Y * (XCellNumber + 1) + X] = GetUVFromPosAnyThread(VertexPosition);
		}
	}

	UKismetProceduralMeshLibrary::CalculateTangentsForMesh(VerticesBuffer, TrianglesBuffer, UV0Buffer, NormalsBuffer, TangentsBuffer);
	GenerateRandomPointsAsync(BufferIndex, Tile, TaskDataBuffers[BufferIndex].RandomPoints);

	// Game 线程的回调
	AsyncTask(ENamedThreads::GameThread, TUniqueFunction<void()>([this, BufferIndex]() {
		BufferStateGameThreadOnly[BufferIndex] = EBufferState::Completed;
	}));
}

void AWorldGenerator::GenerateRandomPointsAsync(int32 BufferIndex, FInt32Point Tile, TArray<RandomPoint>& RandomPoints)
{
	auto BarrierCount = GetBarrierCountForTileAnyThread(Tile);
	int64 Seed = ((int64)Tile.X << 32) | (int64)Tile.Y;
	TaskDataBuffers[BufferIndex].RandomEngine.seed(Seed);
	RandomPoints.SetNumUninitialized(BarrierCount);

	std::uniform_real_distribution<double> PointGenerator(0.0, 1.0);
	for (int32 i = 0; i < BarrierCount; ++i)
	{
		auto& Point = RandomPoints[i];
		Point.Position.X = PointGenerator(TaskDataBuffers[BufferIndex].RandomEngine);
		Point.Position.Y = PointGenerator(TaskDataBuffers[BufferIndex].RandomEngine);
		Point.Size = 1.0f;
	}
}