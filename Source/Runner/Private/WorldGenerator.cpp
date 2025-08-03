// Fill out your copyright notice in the Description page of Project Settings.

#include "WorldGenerator.h"
#include "Algo/MinElement.h"
#include "Async/Async.h"
#include "Async/TaskGraphInterfaces.h"
#include "BarrierSpawner.h"
#include "Containers/AllowShrinking.h"
#include "Containers/Array.h"
#include "Containers/ArrayView.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Character.h"
#include "HAL/Platform.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMaterialLibrary.h"
#include "KismetProceduralMeshLibrary.h"
#include "KismetTraceUtils.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Math/Color.h"
#include "Math/MathFwd.h"
#include "Misc/AssertionMacros.h"
#include "MissileComponent.h"
#include "ProceduralMeshComponent.h"
#include "Templates/Tuple.h"
#include "UObject/ObjectPtr.h"
#include <algorithm>
#include <cstdint>
#include <functional>
#include <random>

DEFINE_LOG_CATEGORY_STATIC(LogWorldGenerator, Log, All);

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

	MissileComponent = CreateDefaultSubobject<UMissileComponent>(TEXT("MissileComponent"));
}

// Called when the game starts or when spawned
void AWorldGenerator::BeginPlay()
{
	InitDataBuffer();
	SortBarrierSpawners();
	// SpawnBarrierSpawners();

	// Set EvilPos to minimum double
	// EvilPos = TNumericLimits<double>::Lowest();
	// float MatTextureSize, MaxMatTextureCoords, MatTileSize;
	// TileMaterial->GetScalarParameterValue(FHashedMaterialParameterInfo("TextureSize"), MatTextureSize);
	// TileMaterial->GetScalarParameterValue(FHashedMaterialParameterInfo("MaxTexCoords"), MaxMatTextureCoords);
	// TileMaterial->GetScalarParameterValue(FHashedMaterialParameterInfo("TileSize"), MatTileSize);
	// ensure(MatTileSize == float(CellSize) * XCellNumber);
	// ensure(MatTextureSize == TextureSize.X);
	// ensure(MaxMatTextureCoords == MaxTextureCoords);

	Super::BeginPlay();
}

// 按组号对 Barrier Spawner 进行排序
void AWorldGenerator::SortBarrierSpawners()
{
	BarrierSpawners.Sort([](const TObjectPtr<ABarrierSpawner>& A, const TObjectPtr<ABarrierSpawner>& B) {
		if (A->bDeferSpawn)
		{
			return false; // Defer spawn barriers should be at the end
		}
		else if (B->bDeferSpawn)
		{
			return true; // Defer spawn barriers should be at the end
		}
		if (A->BarrierGroup < 0)
		{
			return false; // Group -1 barriers should be at the end
		}
		else if (B->BarrierGroup < 0)
		{
			return true; // Group -1 barriers should be at the end
		}
		return A->BarrierGroup < B->BarrierGroup;
	});
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

	UpdateEvilPos(DeltaTime);

	if (bDebugMode)
	{
		DebugPrint();
	}
}

void AWorldGenerator::UpdateEvilPos(float DeltaTime)
{
	if (bGameStart)
	{
		auto PlayerTile = GetPlayerTile();
		auto TileSizeX = CellSize * XCellNumber;

		auto MinPos = double(TileSizeX) * (PlayerTile.X - 1);
		auto NewEvilPos = EvilPos + (EvilChaseSpeed * DeltaTime);
		EvilPos = FMath::Max(MinPos, NewEvilPos);

		UKismetMaterialLibrary::SetScalarParameterValue(this, EvilChaseMaterialCollection, "EvilPos", float(EvilPos / TileSizeX));
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

	// UE_LOG(LogWorldGenerator, Log, TEXT("Player Position: %s, V1 Pos: %s, V1 UV: %s, V2 Pos: %s, V2 UV: %s, V3 Pos: %s, V3 UV: %s"),
	// 		*Pos.ToString(), *Vertex1.ToString(), *UV1.ToString(), *Vertex2.ToString(), *UV2.ToString(), *Vertex3.ToString(), *UV3.ToString());
}

TPair<UProceduralMeshComponent*, int32> AWorldGenerator::GetPMCFromHorizontalPos(FVector2D Pos) const
{
	FInt32Point Region = GetRegionFromHorizontalPos(Pos);
	int32 ReplacableIndex = -1;
	for (int32 i = 0; i < MaxRegionCount; ++i)
	{
		UProceduralMeshComponent* PMC = ProceduralMeshComp[i];
		if (!PMC || PMC->GetNumSections() == 0)
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
		UProceduralMeshComponent* PMC = ProceduralMeshComp[ReplacableIndex];
		if (!PMC)
		{
			PMC = NewObject<UProceduralMeshComponent>(NonConstThis, UProceduralMeshComponent::StaticClass(), NAME_None);
			// UE_LOG(LogWorldGenerator, Warning, TEXT("Creating new PMC for region: %s at index %d, input position: %s"), *Region.ToString(), ReplacableIndex, *Pos.ToString());
			PMC->bUseAsyncCooking = true; // 重中之重！！
			PMC->SetCollisionProfileName(TEXT("BlockAll"));
			PMC->RegisterComponent();
			PMC->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepWorldTransform);
			ProceduralMeshComp[ReplacableIndex] = PMC; // Replace the PMC at the found index
		}
		PMC->SetWorldLocation(FVector(Region.X * RegionSize, Region.Y * RegionSize, 0.0f));

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
		// UE_LOG(LogWorldGenerator, Warning, TEXT("Replacing PMC for region: %s with new region: %s at index %d, input position: %s"),
		// 		*OldRegion.ToString(), *Region.ToString(), ReplacableIndex, *Pos.ToString());
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

		// 删除 CachedSpawnData 中对应 tile 的数据
		for (auto It = CachedSpawnData.CreateIterator(); It; ++It)
		{
			if (It.Key().Z == ReplacableIndex)
			{
				// UE_LOG(LogWorldGenerator, Warning, TEXT("Spawner %d, tile %s removed from CachedSpawnData before used!"), It.Key().Z, *FIntVector(It.Key()).ToString());
				It.RemoveCurrent();
			}
		}

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
		TaskDataBuffers[i].BarriersCount.SetNumUninitialized(BarrierSpawners.Num());
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
	UV1Buffer.SetNumUninitialized((XCellNumber + 1) * (YCellNumber + 1));
	for (int32 Y = 0; Y <= YCellNumber; ++Y)
	{
		for (int32 X = 0; X <= XCellNumber; ++X)
		{
			int32 Index = (Y * (XCellNumber + 1)) + X;
			UV1Buffer[Index] = FVector2D(X / double(XCellNumber), Y / double(YCellNumber));
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
		// UE_LOG(LogWorldGenerator, Warning, TEXT("PerlinFreq and PerlinAmplitude arrays must have the same length!"));
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

FVector AWorldGenerator::GetNormalFromHorizontalPos(FVector2D Pos) const
{
	FInt32Point Tile;
	auto UV = GetUVandTileFromPos(Pos, Tile);

	UProceduralMeshComponent* PMC = nullptr;
	int32 PMCIndex = -1;
	Tie(PMC, PMCIndex) = GetPMCFromHorizontalPos(Pos);
	auto MeshSection = TileMap[PMCIndex].Find(Tile);
	if (MeshSection == INDEX_NONE)
	{
		UE_LOG(LogWorldGenerator, Warning, TEXT("Tile not found in TileMap for position: %s"), *Pos.ToString());
		return FVector::UpVector; // Tile not found
	}

	auto* MeshInfo = PMC->GetProcMeshSection(MeshSection);
	if (!MeshInfo)
	{
		UE_LOG(LogWorldGenerator, Warning, TEXT("Mesh section not found for PMC at index %d"), PMCIndex);
		return FVector::UpVector; // Mesh section not found
	}

	FVector2D BarycentricCoords;
	auto TriangleIndex = GetTriangleFromUV(UV, BarycentricCoords);

	auto V1 = MeshInfo->ProcIndexBuffer[TriangleIndex * 3 + 0];
	auto V2 = MeshInfo->ProcIndexBuffer[TriangleIndex * 3 + 1];
	auto V3 = MeshInfo->ProcIndexBuffer[TriangleIndex * 3 + 2];

	auto Normal1 = MeshInfo->ProcVertexBuffer[V1].Normal;
	auto Normal2 = MeshInfo->ProcVertexBuffer[V2].Normal;
	auto Normal3 = MeshInfo->ProcVertexBuffer[V3].Normal;

	auto InterpNormal = Normal1 * BarycentricCoords.X + Normal2 * BarycentricCoords.Y + Normal3 * (1.0 - BarycentricCoords.X - BarycentricCoords.Y);
	InterpNormal.Normalize();

	return InterpNormal;
}

FVector AWorldGenerator::GetNormalFromUVandTile(FVector2D UV, FInt32Point Tile) const
{
	auto TileXSize = CellSize * XCellNumber;
	auto TileYSize = CellSize * YCellNumber;
	auto Pos = FVector2D((Tile.X + UV.X) * TileXSize, (Tile.Y + UV.Y) * TileYSize);
	return GetNormalFromHorizontalPos(Pos);
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
			auto& BarriersCount = TaskData.BarriersCount;

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
				PMC->CreateMeshSection(TileMap[PMCIndex].Num(), VerticesBuffer, TrianglesBuffer, NormalsBuffer, UV0Buffer, UV1Buffer, TArray<FVector2D>(), TArray<FVector2D>(), TArray<FColor>(), TangentsBuffer, true);
				auto* DynamicMat = UMaterialInstanceDynamic::Create(TileMaterial, this, NAME_None);
				if (DynamicMat)
				{
					DynamicMat->SetScalarParameterValue("TileX", Tile.X);
				}
				PMC->SetMaterial(TileMap[PMCIndex].Num(), DynamicMat);
				SectionIdx = TileMap[PMCIndex].Add(Tile);
			}
			else
			{
				// Update the existing section
				auto* DynamicMat = Cast<UMaterialInstanceDynamic>(PMC->GetMaterial(SectionIdx));
				if (DynamicMat)
				{
					DynamicMat->SetScalarParameterValue("TileX", Tile.X);
				}
				PMC->ClearMeshSection(SectionIdx);
				PMC->CreateMeshSection(SectionIdx, VerticesBuffer, TrianglesBuffer, NormalsBuffer, UV0Buffer, UV1Buffer, TArray<FVector2D>(), TArray<FVector2D>(), TArray<FColor>(), TangentsBuffer, true);
				PMC->SetMaterial(SectionIdx, DynamicMat);
				auto OldTile = TileMap[PMCIndex][SectionIdx];
				// 通知 BarrierSpawner 移除旧的 tile 上的障碍物
				for (ABarrierSpawner* BarrierSpawner : BarrierSpawners)
				{
					BarrierSpawner->RemoveTile(OldTile);
				}
				TileMap[PMCIndex][SectionIdx] = Tile;
				// 删除 CachedSpawnData 中对应 tile 的数据
				auto RemoveNumber = CachedSpawnData.Remove(FIntVector(OldTile.X, OldTile.Y, PMCIndex));
				if (RemoveNumber > 0)
				{
					UE_LOG(LogWorldGenerator, Warning, TEXT("Spawner %d, tile %s removed from CachedSpawnData!"), PMCIndex, *FIntVector(OldTile.X, OldTile.Y, PMCIndex).ToString());
				}
			}

			// Spawn barriers
			auto NewTile = TileMap[PMCIndex][SectionIdx];
			int32 StartIdx = 0;
			for (int32 Idx = 0, TotalBarrier = BarrierSpawners.Num(); Idx < TotalBarrier; ++Idx)
			{
				int32 BarCount = BarriersCount[Idx];
				if (BarCount > 0)
				{
					TArrayView<RandomPoint> RandomPointsView(&RandomPoints[StartIdx], BarCount);
					if (BarrierSpawners[Idx]->bDeferSpawn)
					{
						CachedSpawnData.Add(FIntVector(NewTile.X, NewTile.Y, Idx), TArray<RandomPoint>(RandomPointsView));
					}
					else
					{
						BarrierSpawners[Idx]->SpawnBarriers(RandomPointsView, NewTile, this);
					}
					StartIdx += BarCount;
				}
			}
			// 可视化采样点
			if (bDrawSamplingPoint && Tile.X == 50)
			{
				FColor Colors[4] = { FColor::Red, FColor::Green, FColor::Blue, FColor::Yellow };
				int32 ColorIdx = 0;
				auto CurrentGroupIndex = BarrierSpawners[0]->BarrierGroup;
				TArray<FVector> WorldPositions;
				TArray<FVector2D> UVPositions;
				TArray<FVector2D> SameGroupPos;
				StartIdx = 0;
				for (int32 Idx = 0, TotalBarrier = BarrierSpawners.Num(); Idx < TotalBarrier; ++Idx)
				{
					if (BarrierSpawners[Idx]->BarrierGroup != CurrentGroupIndex)
					{
						ColorIdx = (ColorIdx + 1) % 4; // Change color for next group
						if (bCheckPoissonSampling && CurrentGroupIndex == 0)
						{
							for (int32 m = 0; m < SameGroupPos.Num(); ++m)
							{
								for (int32 j = m + 1; j < SameGroupPos.Num(); ++j)
								{
									auto Dist = FVector2D::Distance(SameGroupPos[m], SameGroupPos[j]);
									if (Dist < 1200)
									{
										UE_LOG(LogWorldGenerator, Error, TEXT("Poisson sampling points too close: %s and %s, distance: %f, Tile %s"),
												*SameGroupPos[m].ToString(), *SameGroupPos[j].ToString(), Dist, *NewTile.ToString());
									}
								}
							}
							SameGroupPos.Empty(); // Clear for next group
						}
					}
					CurrentGroupIndex = BarrierSpawners[Idx]->BarrierGroup;
					FColor CurrentColor = Colors[ColorIdx];
					auto BarCount = BarriersCount[Idx];
					auto EndIdx = StartIdx + BarCount;
					for (; StartIdx < EndIdx; ++StartIdx)
					{
						auto& RandomPos = RandomPoints[StartIdx];
						SameGroupPos.Add(FVector2D(RandomPos.UVPos.X * CellSize * XCellNumber, RandomPos.UVPos.Y * CellSize * YCellNumber));
						auto WorldPos = GetVisualWorldPositionFromUV(RandomPos.UVPos, NewTile);
						DrawDebugBox(GetWorld(), WorldPos, FVector(100.0f), CurrentColor, true, 5.0f);
						WorldPositions.Add(WorldPos);
						UVPositions.Add(RandomPos.UVPos);
					}
					StartIdx = EndIdx; // Update StartIdx for the next barrier
				}
				UVPositions.Sort([](const FVector2D& A, const FVector2D& B) {
					return A.X < B.X;
				});
				WorldPositions.Sort([](const FVector& A, const FVector& B) {
					return A.X < B.X;
				});
				// for (auto WorldPos : WorldPositions)
				// {
				// 	// DrawDebugBox(GetWorld(), WorldPos, FVector(100.0f), FColor::Red, true, 5.0f);
				// }
			}
			// 释放 slot
			BufferStateGameThreadOnly[i] = EBufferState::Idle; // Reset the buffer state
			// if (AsyncTaskRef[i])
			{
				AsyncTaskRef[i]->Release();
				AsyncTaskRef[i] = nullptr;
			}
			TilesInBuilding[i] = FInt32Point(INT32_MAX, INT32_MAX); // Reset the tile in building
		}
	}
}

void AWorldGenerator::PMCClear(int32 ReplaceableIndex)
{
	if (ReplaceableIndex < 0 || ReplaceableIndex >= MaxRegionCount)
	{
		UE_LOG(LogWorldGenerator, Warning, TEXT("Invalid PMC index: %d"), ReplaceableIndex);
		return;
	}

	UProceduralMeshComponent* PMC = ProceduralMeshComp[ReplaceableIndex];
	auto OldRegion = GetRegionFromPMC(PMC);
	// UE_LOG(LogWorldGenerator, Warning, TEXT("Clear PMC for old region: %s"),
	// 		*OldRegion.ToString());
	PMC->ClearAllMeshSections();

	for (auto Tile : TileMap[ReplaceableIndex])
	{
		for (ABarrierSpawner* Spawner : BarrierSpawners)
		{
			Spawner->RemoveTile(Tile);
		}
	}
	TileMap[ReplaceableIndex].Empty(); // Clear the tile map for this PMC

	// 删除 CachedSpawnData 中对应 tile 的数据
	for (auto It = CachedSpawnData.CreateIterator(); It; ++It)
	{
		if (It.Key().Z == ReplaceableIndex)
		{
			UE_LOG(LogWorldGenerator, Warning, TEXT("Spawner %d, tile %s removed from CachedSpawnData before used!"), It.Key().Z, *FIntVector(It.Key()).ToString());
			It.RemoveCurrent();
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
		UE_LOG(LogWorldGenerator, Warning, TEXT("All buffers are busy, cannot generate new tile at %s"), *Tile.ToString());
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

	for (int32 i = 0; i < MaxRegionCount; ++i)
	{
		if (TileMap[i].Num() > 0)
		{
			auto bHasNeccessaryTiles = false;
			for (auto Tile : TileMap[i])
			{
				if (IsNeccessrayTile(Tile))
				{
					bHasNeccessaryTiles = true;
					break;
				}
			}
			if (!bHasNeccessaryTiles)
			{
				// 如果没有必要的 tile，则清除这个 PMC
				PMCClear(i);
			}
		}
	}

	for (auto It = CachedSpawnData.CreateIterator(); It; ++It)
	{
		auto Tile = FInt32Point(It.Key().X, It.Key().Y);
		int32 ReplacableIndex = It.Key().Z;
		auto bSuccess = BarrierSpawners[ReplacableIndex]->DeferSpawnBarriers(It.Value(), Tile, this);
		if (bSuccess)
		{
			UE_LOG(LogWorldGenerator, Warning, TEXT("Spawner %d, tile %s spawned barriers from CachedSpawnData!"), ReplacableIndex, *Tile.ToString());
			It.RemoveCurrent();
		}
	}
}

// int32 AWorldGenerator::GetBarrierCountForTileAnyThread(FInt32Point Tile) const
// {
// 	int64 Seed = ((int64)Tile.X << 32) | (int64)Tile.Y;
// 	std::hash<int64> Int64Hash;
// 	auto HashValue = Int64Hash(Seed) % (MaxBarrierCount - MinBarrierCount + 1) + MinBarrierCount;
// 	return (int32)HashValue;
// }

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

FVector AWorldGenerator::GetVisualWorldPositionFromUV(FVector2D UV, FInt32Point Tile) const
{
	auto TestPos = FVector2D((Tile.X + 0.5) * CellSize * XCellNumber, (Tile.Y + 0.5) * CellSize * YCellNumber);
	UProceduralMeshComponent* PMC = nullptr;
	int32 PMCIndex = -1;
	Tie(PMC, PMCIndex) = GetPMCFromHorizontalPos(TestPos);
	auto MeshSection = TileMap[PMCIndex].Find(Tile);
	if (MeshSection == INDEX_NONE)
	{
		UE_LOG(LogWorldGenerator, Warning, TEXT("Tile not found in TileMap for position: %s"), *TestPos.ToString());
		return FVector::ZeroVector; // Tile not found
	}

	auto* MeshInfo = PMC->GetProcMeshSection(MeshSection);
	if (!MeshInfo)
	{
		UE_LOG(LogWorldGenerator, Warning, TEXT("Mesh section not found for PMC at index %d"), PMCIndex);
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

FVector2D AWorldGenerator::GetUVandTileFromPos(FVector2D Pos, FInt32Point& Tile) const
{
	auto TileXSize = CellSize * XCellNumber;
	auto TileYSize = CellSize * YCellNumber;
	Tile = FInt32Point(FMath::FloorToInt(Pos.X / TileXSize), FMath::FloorToInt(Pos.Y / TileYSize));

	auto TileStartX = Tile.X * TileXSize;
	auto TileStartY = Tile.Y * TileYSize;
	auto UVX = (Pos.X - TileStartX) / TileXSize;
	auto UVY = (Pos.Y - TileStartY) / TileYSize;
	return FVector2D(UVX, UVY);
}

double AWorldGenerator::GetVisualHeightFromHorizontalPos(FVector2D Pos) const
{
	FInt32Point Tile;
	auto UV = GetUVandTileFromPos(Pos, Tile);

	auto WorldPos = GetVisualWorldPositionFromUV(UV, Tile);
	return WorldPos.Z; // 返回高度
}

double AWorldGenerator::GetHeightFromHorizontalPos(FVector2D Pos /*, UPrimitiveComponent*& HitComponent*/) const
{
	auto GroundHeight = GetVisualHeightFromHorizontalPos(Pos);
	auto GroundPos = FVector(Pos.X, Pos.Y, GroundHeight);
	FVector Start = GroundPos + FVector(0, 0, 500);
	FVector End = GroundPos + FVector(0, 0, -500); // 向下射线
	FHitResult OutHit;

	// See https://gamedev.stackexchange.com/questions/178811/set-the-linetracebychannel-to-use-my-custom-created-channel-c
	// and See DefaultEngine.ini
	GetWorld()->LineTraceSingleByChannel(OutHit, Start, End, ECC_GameTraceChannel1);
	// DrawDebugLineTraceSingle(GetWorld(), Start, End, EDrawDebugTrace::ForDuration, OutHit.IsValidBlockingHit(), OutHit, FLinearColor::Red, FLinearColor::Green, 1.0f);
	if (OutHit.IsValidBlockingHit())
	{
		auto* HitActor = OutHit.GetActor();
		// HitComponent = OutHit.GetComponent();
		if (HitActor && HitActor != this)
		{
			return OutHit.ImpactPoint.Z; // 返回碰撞点的高度
		}
	}
	return GroundPos.Z;
}

FVector2D AWorldGenerator::ClampToWorld(FVector2D Pos, double Radius) const
{
	auto YSize = double(CellSize) * YCellNumber;
	auto ClampedY = FMath::Clamp(Pos.Y, Radius, YSize - Radius);

	// TODO：截断 X 坐标？
	return FVector2D(Pos.X, ClampedY);
}

// FVector AWorldGenerator::TransformUVToWorldPos(const RandomPoint& Point, FInt32Point Tile) const
// {
// 	auto UVPos = Point.Transform.GetLocation();
// 	auto WorldPos = GetVisualWorldPositionFromUV(FVector2D(UVPos), Tile);
// 	Point.Transform.SetTranslation(WorldPos);
// }

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
	int64 Seed = ((int64)Tile.X << 32) | (int64)Tile.Y;
	TaskDataBuffers[BufferIndex].RandomEngine.seed(Seed);

	// GenerateUniformRandomPointsAsync(BufferIndex, RandomPoints);
	GeneratePoissonRandomPointsAsync(BufferIndex, RandomPoints);
	// UE_LOG(LogWorldGenerator, Warning, TEXT("Generated %d random points for tile %s in buffer %d"), RandomPoints.Num(), *Tile.ToString(), BufferIndex);
}

void AWorldGenerator::GenerateUniformRandomPointsAsync(int32 BufferIndex, TArray<RandomPoint>& RandomPoints)
{
	std::uniform_real_distribution<double> PointGenerator(0.0, 1.0);

	int32 Idx = 0;
	int32 TotalBarrierCount = 0;
	for (ABarrierSpawner* Spawner : BarrierSpawners)
	{
		int32 BarCount = Spawner->GetBarrierCountAnyThread(PointGenerator(TaskDataBuffers[BufferIndex].RandomEngine));
		TaskDataBuffers[BufferIndex].BarriersCount[Idx] = BarCount; // 记录每个 Spawner 的障碍物数量
		TotalBarrierCount += BarCount;
		++Idx;
	}
	RandomPoints.SetNumUninitialized(TotalBarrierCount, EAllowShrinking::No);

	for (int32 i = 0; i < TotalBarrierCount; ++i)
	{
		auto& Point = RandomPoints[i];
		auto UVPos = FVector2D::ZeroVector;
		UVPos.X = PointGenerator(TaskDataBuffers[BufferIndex].RandomEngine);
		UVPos.Y = PointGenerator(TaskDataBuffers[BufferIndex].RandomEngine);
		Point.UVPos = UVPos;
		auto Rotation = FRotator::ZeroRotator;
		Rotation.Yaw = PointGenerator(TaskDataBuffers[BufferIndex].RandomEngine);
		Rotation.Pitch = PointGenerator(TaskDataBuffers[BufferIndex].RandomEngine);
		Rotation.Roll = PointGenerator(TaskDataBuffers[BufferIndex].RandomEngine);

		Point.Rotation = Rotation;
		Point.Scale = FVector(1.0, 1.0, 1.0); // 设置默认缩放
	}
}

void AWorldGenerator::GeneratePoissonRandomPointsAsync(int32 BufferIndex, TArray<RandomPoint>& RandomPoints)
{
	RandomPoints.SetNumUninitialized(0, EAllowShrinking::No);

	auto XSize = CellSize * XCellNumber;
	auto YSize = CellSize * YCellNumber;

	auto& RandomEngine = TaskDataBuffers[BufferIndex].RandomEngine;
	std::uniform_real_distribution<double> UniformGenerator(0.0, 1.0);
	TArray<FVector2D> OutPoints;
	TInlineComponentArray<int32, 10> EachSpawnerCounts;
	EachSpawnerCounts.SetNumUninitialized(BarrierSpawners.Num(), EAllowShrinking::No);

	auto StartIndex = 0;
	while (StartIndex < BarrierSpawners.Num() && BarrierSpawners[StartIndex]->BarrierGroup >= 0)
	{
		int32 GroupIndex = BarrierSpawners[StartIndex]->BarrierGroup;
		ensure(GroupIndex < GroupDistanceFunc.Num()); // 确保分组索引在范围内

		// 计算每组中 Spawner 希望的障碍物数量
		int32 ExpectedBarrierCount = 0;
		int32 EndIndex = StartIndex;
		double PoissonDistance = 0;
		for (; EndIndex < BarrierSpawners.Num() && BarrierSpawners[EndIndex]->BarrierGroup == GroupIndex; ++EndIndex)
		{
			int32 BarCount = BarrierSpawners[EndIndex]->GetBarrierCountAnyThread(UniformGenerator(RandomEngine));
			PoissonDistance = FMath::Max(PoissonDistance, BarrierSpawners[EndIndex]->PoissonDistance);
			EachSpawnerCounts[EndIndex] = BarCount;
			ExpectedBarrierCount += BarCount;
		}
		ensure(PoissonDistance > 0.0);

		OutPoints.SetNum(0, EAllowShrinking::No);
		auto DistFunc = GetDistanceFunc(GroupDistanceFunc[GroupIndex]);
		auto SampleNumber = PoissonSampling(XSize, YSize, PoissonDistance, SampleCountBeforeReject, RandomEngine, DistFunc, OutPoints);
		ensure(SampleNumber == OutPoints.Num()); // 确保采样点数量与返回值一致

		if (bCheckPoissonSampling)
		{
			for (int32 i = 0; i < OutPoints.Num(); ++i)
			{
				for (int32 j = i + 1; j < OutPoints.Num(); ++j)
				{
					auto Dist = FVector2D::Distance(OutPoints[i], OutPoints[j]);
					if (Dist < PoissonDistance)
					{
						UE_LOG(LogWorldGenerator, Error, TEXT("Poisson sampling failed at index %d and %d with distance %f < %f"),
								i, j, Dist, PoissonDistance);
					}
				}
			}
		}

		// 根据比例计算每个 Spawner 实际的障碍物数量
		int32 RealTotalBarrierCount = 0;
		auto SampleScale = FMath::Min(float(SampleNumber) / ExpectedBarrierCount, 1.0f);

		for (int32 Idx = StartIndex; Idx < EndIndex; ++Idx)
		{
			auto EachSpawnerRealCount = FMath::FloorToInt(EachSpawnerCounts[Idx] * SampleScale);
			EachSpawnerRealCount = EachSpawnerRealCount == 0 ? 1 : EachSpawnerRealCount;
			TaskDataBuffers[BufferIndex].BarriersCount[Idx] = EachSpawnerRealCount; // 记录每个 Spawner 的障碍物数量
			RealTotalBarrierCount += EachSpawnerRealCount;
		}
		if (RealTotalBarrierCount > SampleNumber)
		{
			auto ExcessCount = RealTotalBarrierCount - SampleNumber;
			auto* MaxEle = std::max_element(TaskDataBuffers[BufferIndex].BarriersCount.GetData(),
					TaskDataBuffers[BufferIndex].BarriersCount.GetData() + TaskDataBuffers[BufferIndex].BarriersCount.Num());
			ensure(*MaxEle >= ExcessCount); // 确保最大值大于等于多余的数量
			*MaxEle -= ExcessCount;
			RealTotalBarrierCount = SampleNumber;
		}
		// UE_LOG(LogWorldGenerator, Log, TEXT("Group %d Generate RealTotalBarrierCount: %d"),
		// 		GroupIndex, RealTotalBarrierCount);

		// TArray 返回的迭代器与 std 需要的迭代器不匹配
		std::shuffle(OutPoints.GetData(), OutPoints.GetData() + SampleNumber, RandomEngine);
		auto OldPosCnt = RandomPoints.Num();
		RandomPoints.SetNum(OldPosCnt + RealTotalBarrierCount, EAllowShrinking::No);
		for (int32 i = OldPosCnt; i < OldPosCnt + RealTotalBarrierCount; ++i)
		{
			OutPoints[i - OldPosCnt].X /= XSize;
			OutPoints[i - OldPosCnt].Y /= YSize;

			RandomPoints[i].UVPos = OutPoints[i - OldPosCnt];
			auto Rotation = FRotator::ZeroRotator;
			Rotation.Yaw = UniformGenerator(RandomEngine);
			Rotation.Pitch = UniformGenerator(RandomEngine);
			Rotation.Roll = UniformGenerator(RandomEngine);

			RandomPoints[i].Rotation = Rotation;
			RandomPoints[i].Scale = FVector(1.0, 1.0, 1.0); // 设置默认缩放
		}

		if (bCheckPoissonSampling)
		{
			for (int32 i = OldPosCnt; i < OldPosCnt + RealTotalBarrierCount; ++i)
			{
				for (int32 j = i + 1; j < OldPosCnt + RealTotalBarrierCount; ++j)
				{
					auto Pos1 = FVector2D(RandomPoints[i].UVPos.X * XSize, RandomPoints[i].UVPos.Y * YSize);
					auto Pos2 = FVector2D(RandomPoints[j].UVPos.X * XSize, RandomPoints[j].UVPos.Y * YSize);
					// 检查采样点
					auto Dist = FVector2D::Distance(Pos1, Pos2);
					if (Dist < PoissonDistance)
					{
						UE_LOG(LogWorldGenerator, Error, TEXT("Poisson sampling points too close: %s and %s, distance: %f"),
								*Pos1.ToString(), *Pos2.ToString(), Dist);
					}
				}
			}
		}

		StartIndex = EndIndex; // 更新 StartIndex 到下一个分组的起始位置
	}

	for (int32 Idx = StartIndex; Idx < BarrierSpawners.Num(); ++Idx)
	{
		ensure(BarrierSpawners[Idx]->BarrierGroup < 0);
		auto BarCount = BarrierSpawners[Idx]->GetBarrierCountAnyThread(UniformGenerator(RandomEngine));
		TaskDataBuffers[BufferIndex].BarriersCount[Idx] = BarCount;
		auto OldPosCnt = RandomPoints.Num();
		RandomPoints.SetNum(OldPosCnt + BarCount, EAllowShrinking::No);
		for (int32 i = OldPosCnt; i < OldPosCnt + BarCount; ++i)
		{
			auto& Point = RandomPoints[i];
			auto UVPos = FVector2D::ZeroVector;
			UVPos.X = UniformGenerator(RandomEngine);
			UVPos.Y = UniformGenerator(RandomEngine);
			Point.UVPos = UVPos;
			auto Rotation = FRotator::ZeroRotator;
			Rotation.Yaw = UniformGenerator(RandomEngine);
			Rotation.Pitch = UniformGenerator(RandomEngine);
			Rotation.Roll = UniformGenerator(RandomEngine);

			Point.Rotation = Rotation;
			Point.Scale = FVector(1.0, 1.0, 1.0); // 设置默认缩放
		}
	}
}

template <class T>
int32 AWorldGenerator::PoissonSampling(double XSize, double YSize, double MinDistance, int32 SampleCountBeforeReject, std::mt19937_64& RandomEngine, T DistLambda, TArray<FVector2D>& OutPoints)
{
	auto StartSize = OutPoints.Num();

	// 计算每个点的网格大小
	double CellSize = MinDistance / FMath::Sqrt(2.0);
	int32 MaxGridX = FMath::CeilToInt(XSize / CellSize);
	int32 MaxGridY = FMath::CeilToInt(YSize / CellSize);

	// 创建网格
	TArray<TArray<FVector2D>> Grid;
	TArray<bool> GridOccupied;
	TArray<FVector2D> ActiveList;

	Grid.SetNum(MaxGridX);
	GridOccupied.SetNumZeroed(MaxGridX * MaxGridY);
	for (int32 X = 0; X < MaxGridX; ++X)
	{
		Grid[X].SetNumUninitialized(MaxGridY);
	}

	// 生成第一个点
	std::uniform_real_distribution<double> Generator(0.0, 1.0);
	FVector2D FirstPoint = FVector2D(Generator(RandomEngine) * XSize, Generator(RandomEngine) * YSize);
	OutPoints.Add(FirstPoint);
	ActiveList.Add(FirstPoint);
	auto FirstGridX = FMath::FloorToInt(FirstPoint.X / CellSize);
	auto FirstGridY = FMath::FloorToInt(FirstPoint.Y / CellSize);
	Grid[FirstGridX][FirstGridY] = FirstPoint;
	GridOccupied[FirstGridX * MaxGridY + FirstGridY] = true;

	auto IsValidPoint = [&GridOccupied, &Grid, DistLambda, XSize, YSize, MinDistance, MaxGridX, MaxGridY](FVector2D NewPos, int32 NewGridX, int32 NewGridY) -> bool {
		if (NewPos.X < 0 || NewPos.X >= XSize || NewPos.Y < 0 || NewPos.Y >= YSize)
		{
			return false; // 点在网格外
		}
		for (int32 X = FMath::Max(0, NewGridX - 2); X <= FMath::Min(MaxGridX - 1, NewGridX + 2); ++X)
		{
			for (int32 Y = FMath::Max(0, NewGridY - 2); Y <= FMath::Min(MaxGridY - 1, NewGridY + 2); ++Y)
			{
				if (GridOccupied[X * MaxGridY + Y] && DistLambda(NewPos, Grid[X][Y], MinDistance))
				{
					return false;
				}
			}
		}
		return true;
	};

	// 进行采样
	while (!ActiveList.IsEmpty())
	{
		auto Idx = FMath::Clamp(FMath::FloorToInt32(Generator(RandomEngine) * ActiveList.Num()), 0, ActiveList.Num() - 1);
		FVector2D ActivePoint = ActiveList[Idx];
		int32 i = 0;
		for (; i < SampleCountBeforeReject; ++i)
		{
			// auto Radius = FMath::FRand() * MinDistance + MinDistance;
			// auto Angle = FMath::FRand() * 2.0 * PI;
			auto Radius = Generator(RandomEngine) * MinDistance + MinDistance;
			auto Angle = Generator(RandomEngine) * 2.0 * PI;
			FVector2D NewPoint = ActivePoint + FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * Radius;
			auto NewGridX = FMath::FloorToInt(NewPoint.X / CellSize);
			auto NewGridY = FMath::FloorToInt(NewPoint.Y / CellSize);

			// 检查新点是否有效
			if (IsValidPoint(NewPoint, NewGridX, NewGridY))
			{
				OutPoints.Add(NewPoint);
				Grid[NewGridX][NewGridY] = NewPoint;
				GridOccupied[NewGridX * MaxGridY + NewGridY] = true;
				ActiveList.Add(NewPoint);
				break;
			}
		}
		if (i == SampleCountBeforeReject)
		{
			// 如果没有找到有效的点，则从活动列表中移除当前点
			ActiveList.RemoveAt(Idx);
		}
	}

	auto EndSize = OutPoints.Num();
	return EndSize - StartSize; // 返回生成的点数
}
void AWorldGenerator::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	if (!bDebugMode)
	{
		return;
	}

	InitDataBuffer();

	auto PosOffset = FVector2D(double(CellSize) * XCellNumber / 2, double(CellSize) * YCellNumber / 2);
	GenerateOneTileAsync(0, FInt32Point(0, 0), PosOffset);

	auto& Vertices = TaskDataBuffers[0].VerticesBuffer;
	if (DrawType == EDrawType::Gaussian)
	{
		GenerateGaussian2D(Vertices);
	}

	auto& TaskData = TaskDataBuffers[0];
	auto& VerticesBuffer = TaskData.VerticesBuffer;
	auto& NormalsBuffer = TaskData.NormalsBuffer;
	auto& UV0Buffer = TaskData.UV0Buffer;
	auto& TangentsBuffer = TaskData.TangentsBuffer;
	auto& RandomPoints = TaskData.RandomPoints;

	auto Tile = TilesInBuilding[0];
	UProceduralMeshComponent* PMC = nullptr;
	int32 PMCIndex = -1;
	Tie(PMC, PMCIndex) = GetPMCFromHorizontalPos(FVector2D(100.0, 100.0));

	auto SectionIdx = FindReplaceableSection(PMCIndex);

	// Create a new section
	PMC->CreateMeshSection(TileMap[PMCIndex].Num(), VerticesBuffer, TrianglesBuffer, NormalsBuffer, UV0Buffer, UV1Buffer, TArray<FVector2D>(), TArray<FVector2D>(), TArray<FColor>(), TangentsBuffer, true);
	PMC->SetMaterial(TileMap[PMCIndex].Num(), TileMaterial);
}

void AWorldGenerator::GenerateGaussian2D(TArray<FVector>& Vertices) const
{
	double MaxZ = 0;
	for (auto& Vertex : Vertices)
	{
		Vertex.Z = Gaussian2D(Vertex.X, Vertex.Y);
		MaxZ = FMath::Max(MaxZ, Vertex.Z);
	}
	// UE_LOG(LogWorldGenerator, Warning, TEXT("Max Z value in Gaussian distribution: %f"), MaxZ);
}

#if WITH_EDITOR
void AWorldGenerator::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (PropertyChangedEvent.Property && PropertyChangedEvent.Property->GetFName() == GET_MEMBER_NAME_CHECKED(AWorldGenerator, DrawType))
	{
		// Handle changes to the DrawType property
	}
	else if (PropertyChangedEvent.Property && PropertyChangedEvent.Property->GetFName() == GET_MEMBER_NAME_CHECKED(AWorldGenerator, bDebugMode))
	{
		// Handle changes to the bDebugMode property
		if (!bDebugMode)
		{
			for (int32 i = 0; i < MaxRegionCount; ++i)
			{
				if (!ProceduralMeshComp[i])
				{
					continue; // Skip if the PMC is not initialized
				}
				ProceduralMeshComp[i]->ClearAllMeshSections();
			}
		}
	}
}
#endif // WITH_EDITOR