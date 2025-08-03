// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Templates/SubclassOf.h"

#include "MissileComponent.generated.h"

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class RUNNER_API UMissileComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	// Sets default values for this component's properties
	UMissileComponent();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Missile")
	TSubclassOf<class AMissile> MissileClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Missile")
	TArray<float> ExpectedDistance;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Missile")
	float TickInterval;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Missile")
	float MissileReachTime = 1.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Missile")
	float MissileRadius = 1000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Missile")
	float RandomRadius = 1000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Missile")
	float Tension = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Missile")
	float MissileMinStartOffset = 2500.0f; // 导弹起始位置的最小偏移量

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Missile")
	float MissileMaxStartOffset = 5000.0f; // 导弹起始位置的最大偏移量

	UPROPERTY(EditAnywhere, Category = "Debug")
	bool bDebug = false;

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:		
		double LastPlayerPos = 0.0; // 上次玩家位置
		double TotalDistance = 0.0; // 总距离
		double TotalTime = 0.0; // 总时间

		FVector PredictPlayerPos(FVector PlayerPos);
		FVector RandomMissileStartPos(FVector TargetPos);
};
