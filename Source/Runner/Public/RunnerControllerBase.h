// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "RunnerControllerBase.generated.h"

/**
 * 
 */
UCLASS()
class RUNNER_API ARunnerControllerBase : public APlayerController
{
	GENERATED_BODY()
	
public:	
	UFUNCTION(BlueprintImplementableEvent, Category = "Runner")
	void GameOver(int32 EndScore);

	UFUNCTION(BlueprintCallable, Category = "Runner")
	void WhenCountDownOver();

	UPROPERTY(EditAnywhere, Category = "Runner")
	float StartThrustTime = 0.5f;

	FRotator GetControlRotation() const override;

};
