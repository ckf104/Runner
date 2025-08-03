// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "RunnerMovementComponent.generated.h"

/**
 *
 */
UCLASS()
class RUNNER_API URunnerMovementComponent : public UCharacterMovementComponent
{
	GENERATED_BODY()

public:
	URunnerMovementComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skate Controller")
	float SkateboardTraceDistance = 30.0f;

	// 平面上滑板旋转的插值速度
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skate Controller")
	float SkateboardRotationSpeed = 20.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skate Controller")
	float SkateboardWalkingZReduceSpeed = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skate Controller")
	float DeltaNormalThreshold = 0.1f; // 阈值，判断是否需要捕捉空中

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skate Controller")
	float RollAngleInAir = 15.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skate Controller")
	float RotationSpeedInAir = 10.0f;

protected:
	void BeginPlay() override;

	void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	void PhysWalking(float deltaTime, int32 Iterations) override;
	void CalcVelocity(float DeltaTime, float Friction, bool bFluid, float BrakingDeceleration) override;
	void PhysWalking2(float deltaTime, int32 Iterations);

	void UpdateSkateBoardRotation(float DeltaTime);

	void MoveAlongFloor(const FVector& InVelocity, float DeltaSeconds, FStepDownResult* OutStepDownResult = NULL) override;
	void HandleImpact(const FHitResult& Hit, float TimeSlice = 0.f, const FVector& MoveDelta = FVector::ZeroVector) override;
	bool ShouldCatchAir(const FFindFloorResult& OldFloor, const FFindFloorResult& NewFloor) override;

private:
	UPROPERTY()
	TObjectPtr<class USkeletalMeshComponent> SkateboardMesh;

	class AWorldGenerator* WorldGenerator;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skate Controller", meta = (AllowPrivateAccess = "true"))
	float DebugDeltaTime = 0.2f;
};
