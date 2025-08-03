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
#include <functional>

// 整体的结构是这样：最小的单元是一个 Cell，它由两个三角形组成，XCellNumber * YCellNumber 个 Cell
// 定义组成了一个 Tile。Tile 是 ProeduralMeshComp 管理的基本单位。但我发现当生成的顶点坐标距离 ProceduralMeshComp
// 很远时（250000 cm）时，会出现浮点精度问题，导致材质被拉伸。因此定义一个更上层的结构 Region，Region 由 Tile 组成，每
// 个 ProceduralMeshComp 管理一个 Region。Region 的大小是 RegionSize
// 世界中最多同时存在 4 个 Region，使用 LRU 算法来对 Region 进行替换

// Sets default values
AWorldGenerator::AWorldGenerator()
		: PointGenerator(0.0, 1.0)
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
	// ProceduralMeshComp = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("ProceduralMeshComp"));
	// RootComponent = ProceduralMeshComp;
	// ProceduralMeshComp->bUseAsyncCooking = true;

	PerlinFreq = { 0.1f, 0.2f, 0.3f };
	PerlinAmplitude = { 1.0f, 0.5f, 0.25f };
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("RootComponent"));
}

// Called when the game starts or when spawned
void AWorldGenerator::BeginPlay()
{
	InitDataBuffer();
	// SpawnBarrierSpawners();

	Super::BeginPlay();
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
	// Initialize the TileMap with empty tiles
	VerticesBuffer.SetNumUninitialized((XCellNumber + 1) * (YCellNumber + 1));
	TrianglesBuffer.SetNumUninitialized(XCellNumber * YCellNumber * 6);
	NormalsBuffer.SetNumUninitialized((XCellNumber + 1) * (YCellNumber + 1));
	UV0Buffer.SetNumUninitialized((XCellNumber + 1) * (YCellNumber + 1));
	TangentsBuffer.SetNumUninitialized((XCellNumber + 1) * (YCellNumber + 1));
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
	return Tile;
}

bool AWorldGenerator::IsNeccessrayTile(FInt32Point Tile) const
{
	auto PlayerTile = GetPlayerTile();
	if (Tile.X >= PlayerTile.X - 1 && Tile.X <= PlayerTile.X + 1 && Tile.Y >= PlayerTile.Y - 1 && Tile.Y <= PlayerTile.Y + 1)
	{
		return true; // Tile is within the player's vicinity
	}
	return false;
}

bool AWorldGenerator::CanRemoveTile(FInt32Point Tile) const
{
	auto PlayerTile = GetPlayerTile();
	if (Tile.X >= PlayerTile.X - 2 && Tile.X <= PlayerTile.X + 2 && Tile.Y >= PlayerTile.Y - 2 && Tile.Y <= PlayerTile.Y + 2)
	{
		return false; // Tile is still within the player's vicinity
	}
	return true;
}

FVector2D AWorldGenerator::GetUVFromPos(FVector Position) const
{
	double X = Position.X / (TextureSize.X);
	double Y = Position.Y / (TextureSize.Y);
	X = FMath::Fmod(X + UVOffset.X, MaxTextureCoords);
	Y = FMath::Fmod(Y + UVOffset.Y, MaxTextureCoords);
	return FVector2D(X, Y);
}

double AWorldGenerator::GetHeightFromPerlin(FVector2D Pos, FInt32Point CellPos) const
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

void AWorldGenerator::GenerateOneTile(int32 InX, int32 InY)
{
	double XOffset = (double)InX * CellSize * XCellNumber;
	double YOffset = (double)InY * CellSize * YCellNumber;

	auto TestPos = FVector2D(XOffset + (double)CellSize * XCellNumber / 2.0, YOffset + (double)CellSize * YCellNumber / 2.0);
	UProceduralMeshComponent* PMC = nullptr;
	int32 PMCIndex = -1;
	Tie(PMC, PMCIndex) = GetPMCFromHorizontalPos(TestPos);

	for (int32 Y = 0; Y <= YCellNumber; ++Y)
	{
		for (int32 X = 0; X <= XCellNumber; ++X)
		{
			FVector VertexPosition(double(X) * CellSize + XOffset, double(Y) * CellSize + YOffset, 0.0);
			VertexPosition.Z = GetHeightFromPerlin(FVector2D(VertexPosition.X, VertexPosition.Y), FInt32Point(InX * XCellNumber + X, InY * YCellNumber + Y));
			VerticesBuffer[Y * (XCellNumber + 1) + X] = VertexPosition - PMC->GetComponentLocation();
			UV0Buffer[Y * (XCellNumber + 1) + X] = GetUVFromPos(VertexPosition);
		}
	}
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


	UKismetProceduralMeshLibrary::CalculateTangentsForMesh(VerticesBuffer, TrianglesBuffer, UV0Buffer, NormalsBuffer, TangentsBuffer);
	auto SectionIdx = FindReplaceableSection(PMCIndex);
	// See https://forums.unrealengine.com/t/procedural-mesh-does-not-update-collision-after-modifying-vertices/68174/2
	// TODO: https://github.com/TriAxis-Games/RealtimeMeshComponent
	// https://github.com/TriAxis-Games/RuntimeMeshComponent-Examples
	if (SectionIdx == -1)
	{
		// Create a new section
		PMC->CreateMeshSection(TileMap[PMCIndex].Num(), VerticesBuffer, TrianglesBuffer, NormalsBuffer, UV0Buffer, TArray<FColor>(), TangentsBuffer, true);
		PMC->SetMaterial(TileMap[PMCIndex].Num(), TileMaterial);
		SectionIdx = TileMap[PMCIndex].Add(FInt32Point(InX, InY));
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
			BarrierSpawner->RemoveTile(OldTile, this);
		}
		TileMap[PMCIndex][SectionIdx] = FInt32Point(InX, InY);
	}
	auto NewTile = TileMap[PMCIndex][SectionIdx];
	auto RandomPoints = GenerateRandomPoints(NewTile);
	if (BarrierSpawners.Num() > 0)
	{
		// Spawn barriers using the first available barrier spawner
		BarrierSpawners[0]->SpawnBarriers(RandomPoints, NewTile, this);
	}
}

void AWorldGenerator::GenerateNewTiles()
{
	// Get the player location
	auto* Character = UGameplayStatics::GetPlayerCharacter(this, 0);
	if (!Character)
	{
		return;
	}
	FVector PlayerLocation = Character->GetActorLocation();
	int32 TileXSize = CellSize * XCellNumber;
	int32 TileYSize = CellSize * YCellNumber;
	FInt32Point PlayerTile(FMath::FloorToInt(PlayerLocation.X / TileXSize), FMath::FloorToInt(PlayerLocation.Y / TileYSize));

	// Generate new tiles around the player
	for (int32 Y = PlayerTile.Y - 1; Y <= PlayerTile.Y + 1; ++Y)
	{
		for (int32 X = PlayerTile.X - 1; X <= PlayerTile.X + 1; ++X)
		{
			FInt32Point Tile(X, Y);
			if (IsNeccessrayTile(Tile) && !IsValidTile(Tile))
			{
				GenerateOneTile(Tile.X, Tile.Y);
			}
		}
	}
}

int32 AWorldGenerator::GetBarrierCountForTile(FInt32Point Tile) const
{
	int64 Seed = ((int64)Tile.X << 32) | (int64)Tile.Y;
	std::hash<int64> Int64Hash;
	auto HashValue = Int64Hash(Seed) % (MaxBarrierCount - MinBarrierCount + 1) + MinBarrierCount;
	return (int32)HashValue;
}

TArray<RandomPoint> AWorldGenerator::GenerateRandomPoints(FInt32Point Tile)
{
	auto BarrierCount = GetBarrierCountForTile(Tile);
	int64 Seed = ((int64)Tile.X << 32) | (int64)Tile.Y;
	RandomEngine.seed(Seed);
	TArray<RandomPoint> RandomPoints;
	RandomPoints.SetNumUninitialized(BarrierCount);
	for (int32 i = 0; i < BarrierCount; ++i)
	{
		auto& Point = RandomPoints[i];
		Point.Position.X = PointGenerator(RandomEngine);
		Point.Position.Y = PointGenerator(RandomEngine);
		Point.Size = 1.0f;
	}
	return RandomPoints;
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