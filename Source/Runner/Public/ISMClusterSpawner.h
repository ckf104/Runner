// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BarrierSpawner.h"
#include "UObject/NameTypes.h"
#include "UObject/UnrealType.h"

#include "ISMClusterSpawner.generated.h"
/**
 *
 */
UCLASS()
class RUNNER_API AISMClusterSpawner : public ABarrierSpawner
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Cluster Spawner")
	TArray<TObjectPtr<class UInstancedStaticMeshComponent>> ISMComponents;

	UPROPERTY(EditAnywhere, Category = "Cluster Spawner")
	int32 MinMeshCountInCluster = 1; // 每个 cluster 中最少的 mesh 数量

	UPROPERTY(EditAnywhere, Category = "Cluster Spawner")
	int32 MaxMeshCountInCluster = 5; // 每个 cluster 中最多的 mesh 数量

	UPROPERTY(EditAnywhere, Category = "Cluster Spawner")
	float MinDistanceWithinCluster = 50.0f; // 每个 cluster 中 mesh 之间的距离

	UPROPERTY(EditAnywhere, Category = "Cluster Spawner")
	float HalfClusterExtent = 100.0f; // 每个 cluster 的方格大小

	UPROPERTY(EditAnywhere, Category = "Cluster Spawner")
	int32 TryTimeBeforeGiveUp = 10; // 在放置 mesh 时，尝试的次数

	AISMClusterSpawner();

	void SpawnBarriers(TArrayView<RandomPoint> Positions, FInt32Point Tile, AWorldGenerator* WorldGenerator) override;
	void RemoveTile(FInt32Point Tile) override;

#if WITH_EDITOR
	void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

protected:
	void BeginPlay() override;

	// 每个 tile 上的静态网格体实例编号
	TMap<FInt32Point, TArray<FInt32Point>> TileInstanceIndices;

	// 各个 ISM 中可替换的实例
	TArray<TArray<int32>> ReplaceInstanceIndices;
};
