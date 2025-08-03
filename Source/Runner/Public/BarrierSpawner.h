// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Math/MathFwd.h"
#include "WorldGenerator.h"
#include "BarrierSpawner.generated.h"

UCLASS(Abstract)
class RUNNER_API ABarrierSpawner : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	ABarrierSpawner();

protected:

public:	

	virtual void SpawnBarriers(TArrayView<RandomPoint> Positions, FInt32Point Tile, AWorldGenerator* WorldGenerator) {};
	virtual void RemoveTile(FInt32Point Tile) {};	
	virtual int32 GetBarrierCountAnyThread(double RandomValue) const { return 10; }
};
