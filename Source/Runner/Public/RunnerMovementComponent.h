// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "RunnerMovementComponent.generated.h"

UENUM(BlueprintType)
enum class ELevel : uint8
{
	Bad,
	Normal,
	Good,
	Perfect,
};

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

	// 渐渐调整到 RollAngleInAir 的时间
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skate Controller")
	float RollRotationSpeedInAir = 10.0f;

	// 玩家主动调整 Roll 时的 Roll 变化速度
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skate Controller")
	float PlayerRollRotationSpeedInAir = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skate Controller")
	float RollRotationRestoreSpeedInAir = 10.0f;

	// 玩家控制能达到的最大的 Roll 角度
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skate Controller")
	float MaxRollRotationInAir = 60.0f;

	// 通过调整 gravity scale 来控制空中 z 速度的变化速率
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skate Controller")
	float ZVelocityInAir = -100.0f; // 空中时的 Z 速度

	// 玩家按下 down 时最大达到的 Z 速度
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skate Controller")
	float MaxZVelocityInAir = -200.0f;

	// 当玩家松开 down 时，Z 速度恢复到 ZVelocityInAir 的速率
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skate Controller")
	float ZVelocityRestoreSpeed = 30.0f;

	// 当玩家按下 down 时，Z 速度的变化速率
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skate Controller")
	float ZVelocityDownSpeed = 50.0f;

	// 滑板在空中的 Pitch 角度
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skate Controller")
	float PitchInAir = 0.0f;

	// player 在空中转向时滑板的 Pitch 角度
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skate Controller")
	float TurnPitchInAir = 30.0f;

	// 滑板在空中的 Pitch 变化速率
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skate Controller")
	float PitchChangeSpeedInAir = 20.0f;


	// 当 z 方向速度值大于此值时才能够起飞
	// UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skate Controller")
	// float MinStartZVelocityInAir = 30.0f;

	// 起飞时最小的 Z 速度
	// UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skate Controller")
	// float MinSTartZVelocityInAir = 50.0f;

	// 起飞时的最小 Roll 角度
	// UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skate Controller")
	// float MinStartRollAngleInAir = 10.0f;

	// 向上起飞时的最大 Z 速度
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skate Controller")
	float MaxStartZVelocityInAir = 300.0f;

	// 起飞时的最大 Roll 角度
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skate Controller")
	float MaxStartRollAngleInAir = 45.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skate Controller")
	float BadDiffPitch = 30.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skate Controller")
	float BadDiffRoll = 30.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skate Controller")
	float PerfectDiffPitch = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skate Controller")
	float PerfectDiffRoll = 10.0f;

	// 最小的空中时间
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skate Controller")
	float MinimumAirTime = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skate Controller")
	float MaxThrustSpeed = 3000.0f;

	// 碰撞到物体后的忽略该物体的时间
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skate Controller")
	float IgnoreTime;

protected:
	void BeginPlay() override;

	void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	void PhysWalking(float deltaTime, int32 Iterations) override;
	void CalcVelocity(float DeltaTime, float Friction, bool bFluid, float BrakingDeceleration) override;
	void PhysWalking2(float deltaTime, int32 Iterations);

	void UpdateSkateBoardRotation(float DeltaTime);
	float UpdateRollInAir(float DeltaTime, float CurrentRoll, float DownValue) const;
	float UpdatePitchInAir(float DeltaTime, float CurrentPitch, float TurnValue) const;

	void MoveAlongFloor(const FVector& InVelocity, float DeltaSeconds, FStepDownResult* OutStepDownResult = NULL) override;
	void HandleImpact(const FHitResult& Hit, float TimeSlice = 0.f, const FVector& MoveDelta = FVector::ZeroVector) override;
	bool ShouldCatchAir(const FFindFloorResult& OldFloor, const FFindFloorResult& NewFloor) override;
	void ProcessLanded(const FHitResult& Hit, float remainingTime, int32 Iterations) override;
	FVector NewFallVelocity(const FVector& InitialVelocity, const FVector& Gravity, float DeltaTime) const override;

public:
	void StartThrust();
	void StopThrust();

private:
	UPROPERTY()
	TObjectPtr<class USkeletalMeshComponent> SkateboardMesh;

	class AWorldGenerator* WorldGenerator;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skate Controller", meta = (AllowPrivateAccess = "true"))
	float DebugDeltaTime = 0.2f;

	float InAirTime = 0.0f;

	float DefaultMaxWalkingSpeed;

	// 上一帧在地面时滑板的目标旋转，用来指示移动方向
	FRotator LastTickTargetRotation;

	FRotator CalcWakingTargetRotation(bool& bAccuracy) const;
	// 计算起飞时的 Z 速度
	float CalcStartZVelocity() const;

	ELevel CalcLandLevel(FRotator TargetRotation) const;


};
