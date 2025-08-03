// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BarrierSpawner.h"
#include "DebugBoxSpawner.generated.h"

/**
 * 
 */
UCLASS()
class RUNNER_API ADebugBoxSpawner : public ABarrierSpawner
{
	GENERATED_BODY()

public:
		void SpawnBarriers(TArrayView<RandomPoint> Positions, FInt32Point Tile, AWorldGenerator* WorldGenerator) override;
	
	
};
