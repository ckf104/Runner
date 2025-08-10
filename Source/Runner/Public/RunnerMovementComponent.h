// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "RunnerMovementComponent.generated.h"

UENUM(BlueprintType)
enum class ELevel : uint8
{
	Bad,
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
	float IgnoreTime = 1.0f;

	// 起飞的阈值
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skate Controller")
	float TakeoffThreshold = 800.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skate Controller")
	float TakeoffSpeedScale = 0.5f;

	// 寻找木板的 trace 长度
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skate Controller")
	float TraceDistanceToFindFloor = 200.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skate Controller")
	float BadLandReduceSpeedScale = 0.6f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skate Controller")
	float MudReduceSpeedScale = 0.5f;

	UPROPERTY(EditAnywhere, Category = "Skate Controller")
	float GasPercentageWhenPerfect = 0.2f;

	UPROPERTY(EditAnywhere, Category = "Skate Controller")
	float GasPercentageWhenGood = 0.1f;

	// 激活 down 状态时减少 z 方向的速度值
	UPROPERTY(EditAnywhere, Category = "Skate Controller")
	float DownReduceZVelocity = 1000.0f;

	UPROPERTY(EditAnywhere, Category = "Skate Controller")
	float BadReduceSpeedTime = 0.5f;

	UPROPERTY(EditAnywhere, Category = "Skate Controller")
	float PerfectThrustSpeedTime = 0.5f;

	UPROPERTY(EditAnywhere, Category = "Skate Controller")
	float GameStartVelocity = 2500.0f;

	UPROPERTY(EditAnywhere, Category = "Items")
	float FlyStiffness = 50.0f; // 弹簧模型的刚度

	UPROPERTY(EditAnywhere, Category = "Items")
	float FlyDamping = 10.0f; // 弹簧模型的阻尼

	UPROPERTY(EditAnywhere, Category = "Items")
	float TargetHeightInFlying = 2500.0f;

	UPROPERTY(EditAnywhere, Category = "Items")
	float TargetTimeInFlying = 1.5f;

	UPROPERTY(EditAnywhere, Category = "Items")
	float StartZVelocityInFlying = 2000.0f;

	UPROPERTY(EditAnywhere, Category = "Items")
	float EndZVelocityInFlying = 100.0f; // 飞行结束时的 Z 速度

	double V0;
	double A;
	double InitZ;
	float FlyTime = 0.0f;

	float RealTargetHeightInFlying;

	UPROPERTY(EditAnywhere, Category = "Items")
	float TargetSpeedInFlying = 1000.0f;

	// 是否使用曲率来判断起飞
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skate Controller")
	bool bUseCurvatureForTakeoff = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skate Controller")
	bool bUseVisualNormal = true;

	// 滑板有 Down，Thrust，SlowDown 三种状态，Down 状态影响 Z 方向的速度
	// SlowDown 和 Thrust 状态影响水平方向的速度，三种状态互不干扰，可以同时存在
	bool bDown = false;

	bool bHipHop = false; // 是否跳过舞了

	// 有多种来源，因此使用 int8 来表示状态
	int8 Thrusting = false;

	bool bGameStart = false;

	// 音效
	UPROPERTY(EditAnywhere, Category = "Sound")
	TObjectPtr<class USoundBase> TakeOffSound;

	UPROPERTY(EditAnywhere, Category = "Sound")
	TObjectPtr<class USoundBase> LightningSound; // 拾到闪电时的声音

	// UPROPERTY(EditAnywhere, Category = "Sound")
	// TObjectPtr<class USoundBase> LandSound;

	// UPROPERTY(EditAnywhere, Category = "Sound")
	// TObjectPtr<class USoundBase> DownSound;

	UPROPERTY(EditAnywhere, Category = "Sound")
	TObjectPtr<class USoundBase> LandSoundByLevel[3]; // Bad, Good, Perfect

	UPROPERTY(EditAnywhere, Category = "Sound")
	TObjectPtr<class USoundBase> DamageSound;

	UPROPERTY(EditAnywhere, Category = "Sound")
	float LandAudioInterval = 0.5f;

	UPROPERTY(EditAnywhere, Category = "Sound")
	float PlayTakeOffSoundInAirTime = 0.5f;

	UPROPERTY(EditAnywhere, Category = "Sound")
	float PlayTakeOffSoundMinHeight = 300.0f;

	float CurrentAudioInterval = 0.0f;

	void OnAudioFinished(class UAudioComponent* AudioComponent) { CurrentAudioInterval = 0.0f; };
	// 目前的起飞逻辑会有些误判，如果在起飞时播起飞音效会有问题，所以挪到最高点的时候播
	// void NotifyJumpApex() override;

	// 播放受伤音效
	void TakeDamage();
	// 播放冲刺音效
	void PlayLightningSound();

	// 不会随着平台移动而移动
	void UpdateBasedMovement(float DeltaSeconds) override {};

protected:
	void BeginPlay() override;

	void FindFloor(const FVector& CapsuleLocation, FFindFloorResult& OutFloorResult, bool bCanUseCachedLocation, const FHitResult* DownwardSweepResult = NULL) const override;
	void AdjustFloorHeight() override;

	void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	void PhysWalking(float deltaTime, int32 Iterations) override;
	void CalcVelocity(float DeltaTime, float Friction, bool bFluid, float BrakingDeceleration) override;
	void PhysWalking2(float deltaTime, int32 Iterations);
	void PhysFalling(float deltaTime, int32 Iterations) override;
	void PhysFalling2(float deltaTime, int32 Iterations);

	void UpdateSkateBoardRotation(float DeltaTime);
	float UpdateRollInAir(float DeltaTime, float CurrentRoll, float DownValue) const;
	float UpdatePitchInAir(float DeltaTime, float CurrentPitch, float TurnValue) const;

	void MoveAlongFloor(const FVector& InVelocity, float DeltaSeconds, FStepDownResult* OutStepDownResult = NULL) override;
	void HandleImpact(const FHitResult& Hit, float TimeSlice = 0.f, const FVector& MoveDelta = FVector::ZeroVector) override;
	bool ShouldCatchAir(const FFindFloorResult& OldFloor, const FFindFloorResult& NewFloor) override;
	void ProcessLanded(const FHitResult& Hit, float remainingTime, int32 Iterations) override;
	FVector NewFallVelocity(const FVector& InitialVelocity, const FVector& Gravity, float DeltaTime) const override;

	float GetMaxSpeed() const override;

	void OnMovementUpdated(float DeltaSeconds, const FVector& OldLocation, const FVector& OldVelocity) override;
	void OnMovementModeChanged(EMovementMode PreviousMovementMode, uint8 PreviousCustomMode) override;

	bool StepUpWhenFalling(const FVector& GravDir, const FVector& Delta, const FHitResult &Hit);

	void PhysCustom(float deltaTime, int32 Iterations) override;

	UFUNCTION()
	void UFuncOnMovementUpdated(float DeltaSeconds, FVector OldLocation, FVector OldVelocity);

public:
	void StartThrust() { Thrusting++; }
	void StopThrust()
	{
		Thrusting--;
		ensure(Thrusting >= 0);
	}
	void StartDown();
	void StopDown();
	// bool HandleBlockingHit(const FHitResult& Hit);

	void StartFalling(int32 Iterations, float remainingTime, float timeTick, const FVector& Delta, const FVector& subLoc) override;
	bool CheckFall(const FFindFloorResult& OldFloor, const FHitResult& Hit, const FVector& Delta, const FVector& OldLocation, float remainingTime, float timeTick, int32 Iterations, bool bMustJump) override;

	FVector ConsumeInputVector() override;
	void SetGameStart()
	{
		bGameStart = true;
		Velocity = GetOwner()->GetActorForwardVector() * GameStartVelocity;
	}

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug")
	bool bDisableAutoMove = false; // 是否禁用自动移动

	// 用于 coin spawner 那边调用
	static double CalcStartZVelocity(double Roll, double GroundSpeedSize, double TakeoffSpeedScale, double MaxStartZVelocityInAir);

private:
	mutable FHitResult OldHit;

	UPROPERTY()
	TObjectPtr<class USkeletalMeshComponent> SkateboardMesh;

	class UAudioComponent* LandAudio;
	class AWorldGenerator* WorldGenerator;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skate Controller", meta = (AllowPrivateAccess = "true"))
	float DebugDeltaTime = 0.2f;

	int32 PerfectCombo = 0; // 完美落地的连击数
	float InAirTime = 0.0f;

	// 上一帧在地面时滑板的目标旋转，用来指示移动方向
	FRotator LastTickTargetRotation;

	FRotator CalcWakingTargetRotation(bool& bAccuracy) const;
	// 计算起飞时的 Z 速度
	double CalcStartZVelocity() const;

	ELevel CalcLandLevel(FRotator TargetRotation) const;
	void ProcessLandLevel(ELevel LandLevel);
};
