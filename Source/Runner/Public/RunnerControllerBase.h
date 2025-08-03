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
};
