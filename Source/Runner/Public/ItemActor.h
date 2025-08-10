// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "CoinActor.h"
#include "ItemActor.generated.h"

/**
 * 
 */
UCLASS()
class RUNNER_API AItemActor : public ACoinActor
{
	GENERATED_BODY()
	
protected:	
	void BeginPlay() override;

public:
	void Tick(float DeltaTime) override;

	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category="Default")
	float Amplitude = 70.0f;

	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category="Default")
	float Frequency = 3.0f;

	float CurrentTime = 0.0f;
	double InitialZ = 0.0;
};
