// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Containers/Map.h"
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HAL/Platform.h"
#include "Math/MathFwd.h"
#include "ProceduralMeshComponent.h"
#include <random>
#include "Templates/SubclassOf.h"
#include "WorldGenerator.generated.h"

struct RandomPoint
{
	FVector2D Position;
	double Size;
};

UCLASS()
class RUNNER_API AWorldGenerator : public AActor
{
	GENERATED_BODY()

public:
	static constexpr int32 MaxRegionCount = 4; // 最多同时存在的 Region 数量
	static constexpr int32 MaxThreadCount = 4; // 最大线程数

	// Sets default values for this actor's properties
	AWorldGenerator();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "World Generation")
	TObjectPtr<class UMaterialInterface> TileMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "World Generation")
	TArray<TObjectPtr<class ABarrierSpawner>> BarrierSpawners; // 存储生成的障碍物生成器实例

	// 一个方格的大小
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "World Generation")
	float CellSize = 200.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "World Generation")
	int32 XCellNumber = 50;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "World Generation")
	int32 YCellNumber = 50;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "World Generation")
	int32 MinBarrierCount = 30;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "World Generation")
	int32 MaxBarrierCount = 60;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "World Generation")
	double MaxTextureCoords = 2000.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "World Generation")
	double RegionSize = 40000.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "World Generation")
	FVector2D TextureSize = FVector2D(1024.0f, 1024.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "World Generation")
	TArray<float> PerlinFreq;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "World Generation")
	TArray<float> PerlinAmplitude;

	UPROPERTY(VisibleAnywhere, Category = "World Generation")
	mutable TObjectPtr<class UProceduralMeshComponent> ProceduralMeshComp[MaxRegionCount];

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;
	void InitDataBuffer();
	// void SpawnBarrierSpawners();

public:
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	FInt32Point GetPlayerTile() const;

	// 检查该 tile 是否在 player 周围（如果离 player 较远我们可以释放它）
	bool IsNeccessrayTile(FInt32Point Tile) const;
	// 检查该 tile 是否有效（是否在 TileMap 中）
	bool IsValidTile(FInt32Point Tile) const
	{
		for (int32 i = 0; i < MaxRegionCount; ++i)
		{
			if (TileMap[i].Contains(Tile))
			{
				return true; // Tile is found in one of the regions
			}
		}
		return false; // Tile is not found in any region
	}
	bool CanRemoveTile(FInt32Point Tile) const;

	// 寻找一个可以替换的 section, 如果没有找到则返回 -1
	int32 FindReplaceableSection(int32 PMCIndex);
	void GenerateOneTile(int32 X, int32 Y);
	// 根据当前玩家的位置生成新的 tiles
	void GenerateNewTiles();

	FVector2D GetUVFromPos(FVector Position) const;
	double GetHeightFromPerlin(FVector2D Pos, FInt32Point CellPos) const;

	TArray<RandomPoint> GenerateRandomPoints(FInt32Point Tile);
	int32 GetBarrierCountForTile(FInt32Point Tile) const;

	FInt32Point GetRegionFromHorizontalPos(FVector2D Pos) const
	{
		return FInt32Point(FMath::FloorToInt(Pos.X / RegionSize), FMath::FloorToInt(Pos.Y / RegionSize));
	}
	// 获取对应位置的 PMC 和它在数组中的编号
	TPair<UProceduralMeshComponent*, int32> GetPMCFromHorizontalPos(FVector2D Pos) const;
	FInt32Point GetRegionFromPMC(UProceduralMeshComponent* PMC) const
	{
		auto TestPos = PMC->GetComponentLocation() + FVector(RegionSize / 2.0, RegionSize / 2.0, 0.0);
		return FInt32Point(FMath::FloorToInt(TestPos.X / RegionSize), FMath::FloorToInt(TestPos.Y / RegionSize));
	}
	FInt32Point GetTileFromHorizontalPos(FVector2D Pos) const;
	double GetHeightFromHorizontalPos(FVector2D Pos) const;
	FVector GetWorldPositionFromUV(FVector2D UV, FInt32Point Tile) const;
	int32 GetTriangleFromUV(FVector2D UV, FVector2D& BarycentricCoords) const;

private:
	mutable TArray<FInt32Point> TileMap[MaxRegionCount]; // 用于存储生成的方格位置

	struct TaskBuffer
	{
		TArray<FVector> VerticesBuffer;
		TArray<FVector> NormalsBuffer;
		TArray<FVector2D> UV0Buffer;
		TArray<FProcMeshTangent> TangentsBuffer;
	}; 
	TaskBuffer TaskDataBuffers[MaxThreadCount];

	TArray<FVector> VerticesBuffer;
	TArray<int32> TrianglesBuffer;
	TArray<FVector> NormalsBuffer;
	TArray<FVector2D> UV0Buffer;
	TArray<FProcMeshTangent> TangentsBuffer;

	std::mt19937_64 RandomEngine;													 // 随机数引擎
	std::uniform_real_distribution<double> PointGenerator; // 用于生成随机大小的分布

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug", meta = (AllowPrivateAccess = "true"))
	bool bDebugMode = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug", meta = (AllowPrivateAccess = "true"))
	FVector2D UVOffset = FVector2D(0.0f, 0.0f); // 用于调试 UV 偏移

	mutable int64 VersionNumber[MaxRegionCount]; // 对应每个 Region 的版本号
	mutable int64 CurrentVersionIndex = 0;

	void DebugPrint() const;

private:
	static void GenerateOneTileAsync(int32 BufferIndex);
};
