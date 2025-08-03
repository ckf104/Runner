// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ISMBarrierSpawner.h"
#include "ISMBridgeSpawner.generated.h"

/**
 *
 */
UCLASS()
class RUNNER_API AISMBridgeSpawner : public AISMBarrierSpawner
{
	GENERATED_BODY()

public:
	AISMBridgeSpawner();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Barrier Spawner")
	float MinBridgeAngle = 10.0f; // 最小桥梁角度

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Barrier Spawner")
	float MaxBridgeAngle = 45.0f; // 最大桥梁角度

	void SpawnBarriers(TArrayView<RandomPoint> Positions, FInt32Point Tile, AWorldGenerator* WorldGenerator) override;

	bool BarrierHasCustomSlope() const override { return true; }
	double GetCustomSlopeAngle(const FHitResult& Floor) const override;

protected:
	FRotator GetRotationFromSeed(FRotator Seed) const override;
};
