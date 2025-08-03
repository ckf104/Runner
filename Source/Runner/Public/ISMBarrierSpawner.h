// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Components/InstancedStaticMeshComponent.h"
#include "Containers/Map.h"
#include "CoreMinimal.h"
#include "BarrierSpawner.h"
#include "Math/MathFwd.h"
#include "ISMBarrierSpawner.generated.h"

/**
 * 
 */
UCLASS()
class RUNNER_API AISMBarrierSpawner : public ABarrierSpawner
{
	GENERATED_BODY()
	
public:
	AISMBarrierSpawner();

	void SpawnBarriers(TArrayView<RandomPoint> Positions, FInt32Point Tile, AWorldGenerator* WorldGenerator) override;
	void RemoveTile(FInt32Point Tile) override;
	int32 GetBarrierCountAnyThread(double RandomValue) const override;

	const TArray<int32>* GetInstanceInTile(FInt32Point Tile) const { return TileInstanceIndices.Find(Tile); };

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Barrier Spawner")
	UInstancedStaticMeshComponent* ISMComponent;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Barrier Spawner")
	int32 MinBarrierCount = 30; // 最小障碍物数量

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Barrier Spawner")
	int32 MaxBarrierCount = 60; // 最大障碍物数量

protected:
	// 每个 tile 上的静态网格体实例编号
	TMap<FInt32Point, TArray<int32>> TileInstanceIndices;

	// 存储了可复用的静态网格体实例
	TArray<int32> ReplaceInstanceIndices;

};
