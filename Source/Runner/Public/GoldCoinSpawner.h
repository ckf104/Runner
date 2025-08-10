// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BPBarrierSpawner.h"
#include "Math/MathFwd.h"
#include "Templates/SubclassOf.h"

#include "GoldCoinSpawner.generated.h"

/**
 *
 */
UCLASS()
class RUNNER_API AGoldCoinSpawner : public ABPBarrierSpawner
{
	GENERATED_BODY()

public:
	AGoldCoinSpawner();

	// spawn 金币的间隔时间
	UPROPERTY(EditAnywhere, Category = "Barrier Spawner")
	float SpawnTimeInterval = 0.1f;

	UPROPERTY(EditAnywhere, Category = "Items")
	float ItemAppearProbability = 0.5f; // 道具出现的概率

	UPROPERTY(EditAnywhere, Category = "Items")
	int32 CoinNumberInCloud = 20; // 云中金币的数量

	// 一个轨道 spawn 的最大金币数量
	UPROPERTY(EditAnywhere, Category = "Barrier Spawner")
	int32 MaxCoinNumber = 30;

	UPROPERTY(EditAnywhere, Category = "Barrier Spawner")
	int32 ItemAppearNumber = 10; // 道具出现的位置

	UPROPERTY(EditAnywhere, Category = "Barrier Spawner")
	FVector CoinStartOffset = FVector(500, 0, 100); // 金币的起始偏移量

	UPROPERTY(EditAnywhere, Category = "Items")
	FVector CloudOffset = FVector(1000.0, 0.0, 2500.0);

	UPROPERTY(EditAnywhere, Category = "Items")
	FVector CoinOffsetInCloud = FVector(0, 0, 500.0f); // 云中金币的偏移量

	UPROPERTY(EditAnywhere, Category = "Items")
	FVector BoxExtent = FVector(1000.0, 100.0, 100.0);

	UPROPERTY(EditAnywhere, Category = "Items")
	TSubclassOf<class AActor> CloudClass;

	UPROPERTY(EditAnywhere, Category = "Items")
	TArray<TSubclassOf<class AActor>> ClassToRemoveWhenOverlap; // 需要移除的类列表

	UPROPERTY(EditAnywhere, Category = "Items")
	TArray<TSubclassOf<class AActor>> Itemclasses; // 金币的类列表

	UPROPERTY(EditAnywhere, Category = "Items")
	TArray<float> ItemProbabilities; // 每个 Item 的概率

	void SpawnDeferredBarriers(TArrayView<RandomPoint> Positions, FInt32Point Tile, FVector2D UVPos, AWorldGenerator* WorldGenerator);
	FVector2D PreSpawnBarriers(TArrayView<RandomPoint> Positions, FInt32Point Tile, AWorldGenerator* WorldGenerator) override;
	bool DeferSpawnBarriers(TArrayView<RandomPoint> Positions, FInt32Point Tile, FVector2D UVPos, AWorldGenerator* WorldGenerator) override;

protected:
	class AISMBridgeSpawner* BridgeSpawner;

	void BeginPlay() override;

	int32 GenerateGoldTrace(FVector WorldStartPos, double StartYaw, double StartZVelocity, FInt32Point Tile, class AWorldGenerator* WorldGenerator, FVector2D RandomSeed);
	FVector2D GetCurrentGroundDirection(double time);

	bool CanGenerateCloudTrace(AWorldGenerator* WorldGenerator, double TilePos) const;
	void GenerateCloudTrace(FVector ItemPos, FInt32Point Tile);

	void ClearOverlappingBarriers(FVector Pos);

	// 玩家的水平移动速度
	double MaxWalkingSpeed = 500.0;

	// 正常的下落速度
	double ZVelocityInAir = -300.0;
	double Gravity = 980.0;

	// 玩家按下 down 时的下落速度
	double MaxZVelocityInAir = -600.0;
	double ZVelocityDownSpeed = -1200.0;

	// 起飞时的 Z 速度
	double TakeoffSpeedScale = 1.0;
	double MaxStartZVelocityInAir = 1000.0;
};
