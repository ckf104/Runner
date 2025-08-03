// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BPBarrierSpawner.h"
#include "Math/MathFwd.h"

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

	// 一个轨道 spawn 的最大金币数量
	UPROPERTY(EditAnywhere, Category = "Barrier Spawner")
	int32 MaxCoinNumber = 30;

	UPROPERTY(EditAnywhere, Category = "Barrier Spawner")
	FVector CoinStartOffset = FVector(500, 0, 100); // 金币的起始偏移量

	void SpawnBarriers(TArrayView<RandomPoint> Positions, FInt32Point Tile, AWorldGenerator* WorldGenerator) override;
	bool DeferSpawnBarriers(TArrayView<RandomPoint> Positions, FInt32Point Tile, AWorldGenerator* WorldGenerator) override;

protected:
	class AISMBridgeSpawner* BridgeSpawner;

	void BeginPlay() override;

	void GenerateGoldTrace(FVector WorldStartPos, double StartYaw, double StartZVelocity, FInt32Point Tile, class AWorldGenerator* WorldGenerator);
	FVector2D GetCurrentGroundDirection(double time);

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
