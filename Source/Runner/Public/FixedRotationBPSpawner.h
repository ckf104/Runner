// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BPBarrierSpawner.h"
#include "FixedRotationBPSpawner.generated.h"

/**
 * 
 */
UCLASS()
class RUNNER_API AFixedRotationBPSpawner : public ABPBarrierSpawner
{
	GENERATED_BODY()
	
public:	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Barrier Spawner")
	FRotator FixedRotation = FRotator::ZeroRotator; // 固定的旋转角度

protected:
	FRotator GetRotationFromSeed(FRotator Seed) const override { return FixedRotation; } // 返回固定的旋转角度;
};
