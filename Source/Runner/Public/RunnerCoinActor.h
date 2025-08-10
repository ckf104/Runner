// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "CoinActor.h"
#include "RunnerCoinActor.generated.h"

/**
 * 
 */
UCLASS()
class RUNNER_API ARunnerCoinActor : public ACoinActor
{
	GENERATED_BODY()
public:	
	ARunnerCoinActor();
	
protected:
	void Tick(float DeltaTime) override;
};
