// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BPBarrierSpawner.h"
#include "LaserSpawner.generated.h"

/**
 *
 */
UCLASS()
class RUNNER_API ALaserSpawner : public ABPBarrierSpawner
{
	GENERATED_BODY()

public:

	void BeginPlay() override;
	void SpawnBarriers(TArrayView<RandomPoint> Positions, FInt32Point Tile, AWorldGenerator* WorldGenerator) override;

	UPROPERTY(EditAnywhere, Category = "Laser")
	int32 OneLineLaserNumber = 2; // 每行激光的数量

	UPROPERTY(EditAnywhere, Category = "Laser")
	float LaserDistance = 200.0f;

	UPROPERTY(EditAnywhere, Category = "Laser")
	float ProbabilityForBaseLaser = 0.8f;

	UPROPERTY(EditAnywhere)
	TSubclassOf<class AActor> AnotherLaserClass;

};
