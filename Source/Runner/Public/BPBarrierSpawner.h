// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Containers/Array.h"
#include "Containers/Map.h"
#include "CoreMinimal.h"
#include "BarrierSpawner.h"
#include "UObject/ObjectPtr.h"
#include "UObject/WeakObjectPtrTemplates.h"

#include "BPBarrierSpawner.generated.h"
/**
 * 
 */
UCLASS()
class RUNNER_API ABPBarrierSpawner : public ABarrierSpawner
{
	GENERATED_BODY()
	
public:
	void SpawnBarriers(TArrayView<RandomPoint> Positions, FInt32Point Tile, AWorldGenerator* WorldGenerator) override;
	void RemoveTile(FInt32Point Tile) override;	
	int32 GetBarrierCountAnyThread(double RandomValue) const override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Barrier Spawner")
	int32 MinBarrierCount = 2; // 最小障碍物数量

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Barrier Spawner")
	int32 MaxBarrierCount = 5; // 最大障碍物数量

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Barrier Spawner")
	TSubclassOf<AActor> BarrierClass; // 用于生成障碍物的类

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Barrier Spawner")
	int32 MaxCachedBarriers = 10; // 最大缓存障碍物数量

protected:
	TArray<TWeakObjectPtr<AActor>> CachedBarriers; // 存储缓存的障碍物实例

	// 保证生成的 Actor 仅由该 BarrierSpawner 管理，可以不使用 UPROPERTY
	TMultiMap<FInt32Point, TWeakObjectPtr<AActor>> SpawnedBarriers; // 存储生成的障碍物实例
};
