// Fill out your copyright notice in the Description page of Project Settings.

#include "RunnerMovementComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "CoreGlobals.h"
#include "DrawDebugHelpers.h"
#include "EngineUtils.h"
#include "EnhancedInputComponent.h"
#include "GameFramework/Character.h"
#include "Logging/LogVerbosity.h"
#include "Math/RotationMatrix.h"
#include "Math/UnrealMathUtility.h"
#include "Runner/RunnerCharacter.h"
#include "WorldGenerator.h"

DECLARE_CYCLE_STAT(TEXT("Skateboard PhysWalking"), STAT_SkateboardPhysWalking, STATGROUP_Character);

// Defines for build configs
#if DO_CHECK && !UE_BUILD_SHIPPING // Disable even if checks in shipping are enabled.
	#define devCode(Code) checkCode(Code)
#else
	#define devCode(...)
#endif

URunnerMovementComponent::URunnerMovementComponent(const FObjectInitializer& ObjectInitializer)
		: Super(ObjectInitializer)
{
	bOrientRotationToMovement = false;
	// 保证玩家爬坡时水平速度不会减少
	bMaintainHorizontalGroundVelocity = true;
}

void URunnerMovementComponent::BeginPlay()
{
	Super::BeginPlay();
	// Find the SkateboardMesh component
	GetOwner()->ForEachComponent<USkeletalMeshComponent>(false, [this](USkeletalMeshComponent* Component) {
		if (Component->GetName().Contains(TEXT("SkateBoard")))
		{
			SkateboardMesh = Component;
			return false; // Stop iterating once we find the SkateboardMesh
		}
		return true; // Continue iterating
	});

	if (!SkateboardMesh)
	{
		UE_LOG(LogTemp, Warning, TEXT("URunnerMovementComponent: SkateboardMesh not found!"));
	}

	TActorIterator<AWorldGenerator> It(GetWorld());
	for (; It; ++It)
	{
		WorldGenerator = *It;
		if (WorldGenerator)
		{
			break;
		}
	}
	if (!WorldGenerator)
	{
		UE_LOG(LogTemp, Warning, TEXT("ARunnerCharacter: WorldGenerator not found!"));
	}
}

void URunnerMovementComponent::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	// 移动后更新滑板的旋转
	UpdateSkateBoardRotation(DeltaTime);
	InAirTime += DeltaTime;
	// UE_LOG(LogTemp, Log, TEXT("Skateboard Velocity Z: %f, Size: %f"), Velocity.Z, Velocity.Size());
}

ELevel URunnerMovementComponent::CalcLandLevel(FRotator TargetRotation) const
{
	// TODO: Implement land level calculation based on target rotation
	auto CurrentSkateboardRotation = SkateboardMesh->GetComponentRotation();
	auto DiffPitch = FMath::Abs(CurrentSkateboardRotation.Pitch - TargetRotation.Pitch);
	auto DiffRoll = FMath::Abs(CurrentSkateboardRotation.Roll - TargetRotation.Roll);
	if (DiffPitch > BadDiffPitch)
	{
		UE_LOG(LogTemp, Warning, TEXT("Bad Level, Target Pitch: %f, Current Pitch: %f, Target Roll: %f, Current Roll: %f"),
				TargetRotation.Pitch, CurrentSkateboardRotation.Pitch, TargetRotation.Roll, CurrentSkateboardRotation.Roll);
		return ELevel::Bad;
	}
	else if (DiffRoll > BadDiffRoll)
	{
		UE_LOG(LogTemp, Warning, TEXT("Bad Level, Target Roll: %f, Current Roll: %f, Target Pitch: %f, Current Pitch: %f"),
				TargetRotation.Roll, CurrentSkateboardRotation.Roll, TargetRotation.Pitch, CurrentSkateboardRotation.Pitch);
		return ELevel::Bad;
	}
	else if (DiffPitch <= PerfectDiffPitch && DiffRoll <= PerfectDiffRoll)
	{
		UE_LOG(LogTemp, Warning, TEXT("Perfect Level, Target Pitch: %f, Current Pitch: %f, Target Roll: %f, Current Roll: %f"),
				TargetRotation.Pitch, CurrentSkateboardRotation.Pitch, TargetRotation.Roll, CurrentSkateboardRotation.Roll);
		return ELevel::Perfect;
	}
	else if (DiffPitch <= PerfectDiffPitch || DiffRoll <= PerfectDiffRoll)
	{
		UE_LOG(LogTemp, Warning, TEXT("Good Level, Target Pitch: %f, Current Pitch: %f, Target Roll: %f, Current Roll: %f"),
				TargetRotation.Pitch, CurrentSkateboardRotation.Pitch, TargetRotation.Roll, CurrentSkateboardRotation.Roll);
		return ELevel::Good;
	}
	UE_LOG(LogTemp, Warning, TEXT("Normal Level, Target Pitch: %f, Current Pitch: %f, Target Roll: %f, Current Roll: %f"),
			TargetRotation.Pitch, CurrentSkateboardRotation.Pitch, TargetRotation.Roll, CurrentSkateboardRotation.Roll);
	return ELevel::Normal;
}

void URunnerMovementComponent::ProcessLanded(const FHitResult& Hit, float remainingTime, int32 Iterations)
{
	// TODO
	bool bAccuracy = false;
	LastTickTargetRotation = CalcWakingTargetRotation(bAccuracy);

	if (InAirTime > MinimumAirTime)
	{
		auto LandLevel = ELevel::Bad;
		if (bAccuracy)
		{
			LandLevel = CalcLandLevel(LastTickTargetRotation);
			Cast<ARunnerCharacter>(GetOwner())->ShowLandUI(LandLevel);
		}
	}
	InAirTime = 0.0f;

	Super::ProcessLanded(Hit, remainingTime, Iterations);
	// 重置空中时间
}

FVector URunnerMovementComponent::NewFallVelocity(const FVector& InitialVelocity, const FVector& Gravity, float DeltaTime) const
{
	FVector Result = InitialVelocity;

	auto* Runner = Cast<ARunnerCharacter>(GetOwner());
	if (DeltaTime > 0.f && Runner)
	{
		if (Runner->GetDownValue() > 0.9f)
		{
			Result.Z = FMath::Max(Result.Z - ZVelocityDownSpeed * DeltaTime, MaxZVelocityInAir);
		}
		else if (Result.Z < ZVelocityInAir)
		{
			Result.Z = FMath::Min(Result.Z + ZVelocityRestoreSpeed * DeltaTime, ZVelocityInAir);
		}
		else
		{
			Result.Z = FMath::Max(Result.Z - Gravity.Size() * DeltaTime, ZVelocityInAir);
		}
	}

	return Result;
}

FRotator URunnerMovementComponent::CalcWakingTargetRotation(bool& bAccuracy) const
{
	if (SkateboardMesh)
	{
		auto CurrentTransform = SkateboardMesh->GetComponentTransform();
		auto NewWSRotation = FRotator::ZeroRotator;
		NewWSRotation.Yaw = GetOwner()->GetActorRotation().Yaw - 90.0f; // 滑板的前向是 Y 轴，所以需要减去 90 度

		auto DesiredTransform = FTransform(NewWSRotation, CurrentTransform.GetLocation(), CurrentTransform.GetScale3D());
		auto BWCSPos = SkateboardMesh->GetSocketTransform("BW", RTS_Component).GetTranslation();
		auto BWWSPos = DesiredTransform.TransformPosition(BWCSPos);
		auto FWCSPos = SkateboardMesh->GetSocketTransform("FW", RTS_Component).GetTranslation();
		auto FWWSPos = DesiredTransform.TransformPosition(FWCSPos);

		auto GroundHeightInBW = WorldGenerator->GetHeightFromHorizontalPos(FVector2D(BWWSPos.X, BWWSPos.Y));
		auto GroundHeightInFW = WorldGenerator->GetHeightFromHorizontalPos(FVector2D(FWWSPos.X, FWWSPos.Y));

		auto AdjustedBWHeight = BWWSPos.Z - GroundHeightInBW >= SkateboardTraceDistance ? BWWSPos.Z : GroundHeightInBW;
		auto AdjustedBWPos = FVector(BWWSPos.X, BWWSPos.Y, AdjustedBWHeight);
		auto AdjustedFWHeight = FWWSPos.Z - GroundHeightInFW >= SkateboardTraceDistance ? FWWSPos.Z : GroundHeightInFW;
		auto AdjustedFWPos = FVector(FWWSPos.X, FWWSPos.Y, AdjustedFWHeight);

		bAccuracy = BWWSPos.Z - GroundHeightInBW < SkateboardTraceDistance && FWWSPos.Z - GroundHeightInFW < SkateboardTraceDistance;

		// DrawDebugBox(GetWorld(), AdjustedBWPos, FVector(10.0f), FColor::Red, false, 1.0f, 0, 1.0f);
		// DrawDebugBox(GetWorld(), AdjustedFWPos, FVector(10.0f), FColor::Green, false, 1.0f, 0, 1.0f);
		// DrawDebugBox(GetWorld(), FVector(AdjustedBWPos.X, AdjustedBWPos.Y, GroundHeightInBW), FVector(10.0f), FColor::Red, false, 1.0f, 0, 1.0f);
		// DrawDebugBox(GetWorld(), FVector(AdjustedFWPos.X, AdjustedFWPos.Y, GroundHeightInFW), FVector(10.0f), FColor::Green, false, 1.0f, 0, 1.0f);
		// if (Print)
		// {
		// 	if (BWWSPos.Z - GroundHeightInBW >= SkateboardTraceDistance)
		// 	{
		// 		UE_LOG(LogTemp, Warning, TEXT("Back Wheel Too High! BW Z Pos: %f, GroundHeight: %f"),
		// 				BWWSPos.Z, GroundHeightInBW);
		// 	}
		// 	else
		// 	{
		// 		UE_LOG(LogTemp, Log, TEXT("Back Wheel OK BW Z Pos: %f, GroundHeight: %f"),
		// 				BWWSPos.Z, GroundHeightInBW);
		// 	}
		// 	if (FWWSPos.Z - GroundHeightInFW >= SkateboardTraceDistance)
		// 	{
		// 		UE_LOG(LogTemp, Warning, TEXT("Front Wheel Too High! FW Z Pos: %f, GroundHeight: %f"),
		// 				FWWSPos.Z, GroundHeightInFW);
		// 	}
		// 	else
		// 	{
		// 		UE_LOG(LogTemp, Log, TEXT("Front Wheel OK FW Z Pos: %f, GroundHeight: %f"),
		// 				FWWSPos.Z, GroundHeightInFW);
		// 	}
		// }

		auto Xaxis = (AdjustedFWPos - AdjustedBWPos).GetSafeNormal();
		auto NewRoll = FMath::RadiansToDegrees(FMath::Asin(-Xaxis.Z));

		NewWSRotation.Roll = NewRoll;
		DesiredTransform.SetRotation(FQuat(NewWSRotation));

		auto BLWCSPos = SkateboardMesh->GetSocketTransform("BLW", RTS_Component).GetTranslation();
		auto BRWCSPos = SkateboardMesh->GetSocketTransform("BRW", RTS_Component).GetTranslation();
		auto BLWWSPos = DesiredTransform.TransformPosition(BLWCSPos);
		auto BRWWSPos = DesiredTransform.TransformPosition(BRWCSPos);

		auto GroundHeightInBLW = WorldGenerator->GetHeightFromHorizontalPos(FVector2D(BLWWSPos.X, BLWWSPos.Y));
		auto GroundHeightInBRW = WorldGenerator->GetHeightFromHorizontalPos(FVector2D(BRWWSPos.X, BRWWSPos.Y));

		auto AdjustedBLWHeight = BLWWSPos.Z - GroundHeightInBLW >= SkateboardTraceDistance ? BLWWSPos.Z : GroundHeightInBLW;
		auto AdjustedBLWPos = FVector(BLWWSPos.X, BLWWSPos.Y, AdjustedBLWHeight);
		auto AdjustedBRWHeight = BRWWSPos.Z - GroundHeightInBRW >= SkateboardTraceDistance ? BRWWSPos.Z : GroundHeightInBRW;
		auto AdjustedBRWPos = FVector(BRWWSPos.X, BRWWSPos.Y, AdjustedBRWHeight);

		bAccuracy &= BLWWSPos.Z - GroundHeightInBLW < SkateboardTraceDistance && BRWWSPos.Z - GroundHeightInBRW < SkateboardTraceDistance;

		// if (Print)
		// {
		// 	if (BLWWSPos.Z - GroundHeightInBLW >= SkateboardTraceDistance)
		// 	{
		// 		UE_LOG(LogTemp, Warning, TEXT("Back Left Wheel Too High! BLW Z Pos: %f, GroundHeight: %f"),
		// 				BLWWSPos.Z, GroundHeightInBLW);
		// 	}
		// 	else
		// 	{
		// 		UE_LOG(LogTemp, Log, TEXT("Back Left Wheel OK BLW Z Pos: %f, GroundHeight: %f"),
		// 				BLWWSPos.Z, GroundHeightInBLW);
		// 	}
		// 	if (BRWWSPos.Z - GroundHeightInBRW >= SkateboardTraceDistance)
		// 	{
		// 		UE_LOG(LogTemp, Warning, TEXT("Back Right Wheel Too High! BRW Z Pos: %f, GroundHeight: %f"),
		// 				BRWWSPos.Z, GroundHeightInBRW);
		// 	}
		// 	else
		// 	{
		// 		UE_LOG(LogTemp, Log, TEXT("Back Right Wheel OK BRW Z Pos: %f, GroundHeight: %f"),
		// 				BRWWSPos.Z, GroundHeightInBRW);
		// 	}
		// }

		auto Yaxis = (AdjustedBRWPos - AdjustedBLWPos).GetSafeNormal();
		auto NewPitch = FMath::RadiansToDegrees(FMath::Asin(-Yaxis.Z));

		NewWSRotation.Pitch = NewPitch;
		return NewWSRotation;
	}
	return FRotator::ZeroRotator;
}

float URunnerMovementComponent::UpdateRollInAir(float DeltaTime, float CurrentRoll, float DownValue) const
{
	float TargetRoll;
	float InterpSpeed;
	if (DownValue > 0.9f)
	{
		InterpSpeed = PlayerRollRotationSpeedInAir;
		TargetRoll = MaxRollRotationInAir;
	}
	else if (CurrentRoll > RollAngleInAir)
	{
		TargetRoll = RollAngleInAir; // 如果当前 Roll 小于 RollAngleInAir，则调整到 RollAngleInAir
		InterpSpeed = RollRotationRestoreSpeedInAir;
	}
	else
	{
		TargetRoll = RollAngleInAir;
		InterpSpeed = RollRotationSpeedInAir; // 否则使用默认的 Roll 旋转速度
	}
	return FMath::FInterpConstantTo(CurrentRoll, TargetRoll, DeltaTime, InterpSpeed);
}

float URunnerMovementComponent::UpdatePitchInAir(float DeltaTime, float CurrentPitch, float TurnValue) const
{
	float TargetPitch;
	if (TurnValue > 0.9f)
	{
		TargetPitch = TurnPitchInAir; // 如果按下 down，则使用默认的 Pitch 角度
	}
	else if (TurnValue < -0.9f)
	{
		TargetPitch = -TurnPitchInAir; // 如果按下 down，则使用默认的 Pitch 角度
	}
	else
	{
		TargetPitch = PitchInAir; // 否则使用默认的 Pitch 角度
	}
	return FMath::FInterpConstantTo(CurrentPitch, TargetPitch, DeltaTime, PitchChangeSpeedInAir);
}

void URunnerMovementComponent::UpdateSkateBoardRotation(float DeltaTime)
{
	// 在平面上时做滑板 IK
	if (!SkateboardMesh || DeltaTime <= 0.0f)
	{
		return; // 如果没有滑板网格或 DeltaTime 小于等于 0，则不更新
	}
	if (IsWalking())
	{
		bool bAccuracy = false;
		LastTickTargetRotation = CalcWakingTargetRotation(bAccuracy);
		auto InterpRotation = FMath::RInterpConstantTo(SkateboardMesh->GetComponentRotation(), LastTickTargetRotation, DeltaTime, SkateboardRotationSpeed);
		InterpRotation.Yaw = LastTickTargetRotation.Yaw;
		SkateboardMesh->SetWorldRotation(InterpRotation);
	}
	else if (IsFalling())
	{
		auto CurrentRotation = SkateboardMesh->GetRelativeRotation();
		auto CurrentRoll = CurrentRotation.Roll;
		auto* Runner = Cast<ARunnerCharacter>(GetOwner());
		if (Runner)
		{
			auto DownValue = Runner->GetDownValue();
			CurrentRotation.Roll = UpdateRollInAir(DeltaTime, CurrentRoll, DownValue);
			auto TurnValue = Runner->GetTurnValue();
			CurrentRotation.Pitch = UpdatePitchInAir(DeltaTime, CurrentRotation.Pitch, TurnValue);

			SkateboardMesh->SetRelativeRotation(CurrentRotation);
		}
	}
}

void URunnerMovementComponent::HandleImpact(const FHitResult& Hit, float LastMoveTimeSlice, const FVector& RampVector)
{
	Super::HandleImpact(Hit, LastMoveTimeSlice, RampVector);
}

bool URunnerMovementComponent::ShouldCatchAir(const FFindFloorResult& OldFloor, const FFindFloorResult& NewFloor)
{
	auto OldNormal = OldFloor.HitResult.ImpactNormal;
	auto NewNormal = NewFloor.HitResult.ImpactNormal;

	auto VelocityDir = Velocity.GetSafeNormal();
	auto OldDot = FVector::DotProduct(OldNormal, VelocityDir);
	auto NewDot = FVector::DotProduct(NewNormal, VelocityDir);
	if (NewDot - OldDot >= DeltaNormalThreshold)
	{
		// UE_LOG(LogTemp, Warning, TEXT("ShouldCatchAir: OldDot: %f, NewDot: %f, Delta: %f"), OldDot, NewDot, NewDot - OldDot);
		InAirTime = 0.0f; // 重置空中时间
		Velocity.Z = CalcStartZVelocity();
		return true;
	}
	return false;
}

float URunnerMovementComponent::CalcStartZVelocity() const
{
	auto LastRoll = LastTickTargetRotation.Roll;

	// 朝下的方向
	if (LastRoll >= 0.0f)
	{
		auto Alpha = FMath::Clamp(LastRoll / MaxStartRollAngleInAir, 0.0f, 1.0f);
		return FMath::Lerp(0.0f, ZVelocityInAir, Alpha);
	}
	// 朝上的方向
	else
	{
		auto Alpha = FMath::Clamp(-LastRoll / MaxStartRollAngleInAir, 0.0f, 1.0f);
		return FMath::Lerp(0.0f, MaxStartZVelocityInAir, Alpha);
	}
}

// MaxWalkingSpeed 此时限制的是 skateboard 的水平速度
void URunnerMovementComponent::CalcVelocity(float DeltaTime, float Friction, bool bFluid, float BrakingDeceleration)
{
	auto OOldVelocity = Velocity;
	// Super::CalcVelocity(DeltaTime, Friction, bFluid, BrakingDeceleration);

	if (!HasValidData() || HasAnimRootMotion() || DeltaTime < MIN_TICK_TIME || (CharacterOwner && CharacterOwner->GetLocalRole() == ROLE_SimulatedProxy && !bWasSimulatingRootMotion))
	{
		return;
	}

	Friction = FMath::Max(0.f, Friction);
	const float MaxAccel = GetMaxAcceleration();
	float MaxSpeed = GetMaxSpeed();

	// Check if path following requested movement
	bool bZeroRequestedAcceleration = true;
	FVector RequestedAcceleration = FVector::ZeroVector;
	float RequestedSpeed = 0.0f;
	if (ApplyRequestedMove(DeltaTime, MaxAccel, MaxSpeed, Friction, BrakingDeceleration, RequestedAcceleration, RequestedSpeed))
	{
		bZeroRequestedAcceleration = false;
	}

	if (bForceMaxAccel)
	{
		// Force acceleration at full speed.
		// In consideration order for direction: Acceleration, then Velocity, then Pawn's rotation.
		if (Acceleration.SizeSquared() > UE_SMALL_NUMBER)
		{
			Acceleration = Acceleration.GetSafeNormal() * MaxAccel;
		}
		else
		{
			Acceleration = MaxAccel * (Velocity.SizeSquared() < UE_SMALL_NUMBER ? UpdatedComponent->GetForwardVector() : Velocity.GetSafeNormal());
		}

		AnalogInputModifier = 1.f;
	}

	// Path following above didn't care about the analog modifier, but we do for everything else below, so get the fully modified value.
	// Use max of requested speed and max speed if we modified the speed in ApplyRequestedMove above.
	const float MaxInputSpeed = FMath::Max(MaxSpeed * AnalogInputModifier, GetMinAnalogSpeed());
	MaxSpeed = FMath::Max(RequestedSpeed, MaxInputSpeed);

	// Apply braking or deceleration
	const bool bZeroAcceleration = Acceleration.IsZero();
	const bool bVelocityOverMax = IsExceedingMaxSpeed(MaxSpeed);

	// Only apply braking if there is no acceleration, or we are over our max speed and need to slow down to it.
	if ((bZeroAcceleration && bZeroRequestedAcceleration) || bVelocityOverMax)
	{
		const FVector OldVelocity = Velocity;

		const float ActualBrakingFriction = (bUseSeparateBrakingFriction ? BrakingFriction : Friction);
		ApplyVelocityBraking(DeltaTime, ActualBrakingFriction, BrakingDeceleration);

		// Don't allow braking to lower us below max speed if we started above it.
		if (bVelocityOverMax && Velocity.SizeSquared() < FMath::Square(MaxSpeed) && FVector::DotProduct(Acceleration, OldVelocity) > 0.0f)
		{
			Velocity = OldVelocity.GetSafeNormal() * MaxSpeed;
		}
	}
	else if (!bZeroAcceleration)
	{
		// Friction affects our ability to change direction. This is only done for input acceleration, not path following.
		const FVector AccelDir = Acceleration.GetSafeNormal();
		const float VelSize = Velocity.Size();
		Velocity = Velocity - (Velocity - AccelDir * VelSize) * FMath::Min(DeltaTime * Friction, 1.f);
	}
	auto VectorImm = Velocity;

	// Apply fluid friction
	if (bFluid)
	{
		Velocity = Velocity * (1.f - FMath::Min(Friction * DeltaTime, 1.f));
	}

	auto NewMaxInput = 0.0f;
	auto VectorImm2 = FVector::ZeroVector;
	// Apply input acceleration
	if (!bZeroAcceleration)
	{
		const float NewMaxInputSpeed = IsExceedingMaxSpeed(MaxInputSpeed) ? Velocity.Size() : MaxInputSpeed;
		Velocity += Acceleration * DeltaTime;
		Velocity = Velocity.GetClampedToMaxSize(NewMaxInputSpeed);

		VectorImm2 = Velocity;
		NewMaxInput = NewMaxInputSpeed;
	}

	// Apply additional requested acceleration
	if (!bZeroRequestedAcceleration)
	{
		const float NewMaxRequestedSpeed = IsExceedingMaxSpeed(RequestedSpeed) ? Velocity.Size() : RequestedSpeed;
		Velocity += RequestedAcceleration * DeltaTime;
		Velocity = Velocity.GetClampedToMaxSize(NewMaxRequestedSpeed);
	}

	if (bUseRVOAvoidance)
	{
		CalcAvoidanceVelocity(DeltaTime);
	}

	// UE_LOG(LogTemp, Log, TEXT("URunnerMovementComponent::CalcVelocity called with bZeroRequestedAcceleration: %s, bVelocityOverMax: %s, AnalogInputModifier: %f, MaxSpeed: %f"), bZeroRequestedAcceleration ? TEXT("true") : TEXT("false"), bVelocityOverMax ? TEXT("true") : TEXT("false"), AnalogInputModifier, MaxSpeed);
	// UE_LOG(LogTemp, Log, TEXT("URunnerMovementComponent::CalcVelocity called with VectorImm: %s, VectorImm2: %s, NewMaxInput: %f"), *VectorImm.ToString(), *VectorImm2.ToString(), NewMaxInput);
	// UE_LOG(LogTemp, Log, TEXT("URunnerMovementComponent::CalcVelocity called with DeltaTime: %f, Friction: %f, bFluid: %d, BrakingDeceleration: %f, Accelaration: %s, OldVelocity: %s, OldSize %f, NewVelocity: %s, NewSize %f"), DeltaTime, Friction, bFluid, BrakingDeceleration, *Acceleration.ToString(), *OOldVelocity.ToString(), OOldVelocity.Size(), *Velocity.ToString(), Velocity.Size());
	// 速度设置为与滑板的前进的方向一致
	if (IsWalking() && SkateboardMesh)
	{
		auto NewVelocity = Velocity.Size() * GetOwner()->GetActorForwardVector();
		Velocity.X = NewVelocity.X;
		Velocity.Y = NewVelocity.Y;
		// Velocity.Z = NewVelocity.Z;
	}
}

void URunnerMovementComponent::MoveAlongFloor(const FVector& InVelocity, float DeltaSeconds, FStepDownResult* OutStepDownResult)
{
	if (!CurrentFloor.IsWalkableFloor())
	{
		return;
	}

	const FVector Delta = ProjectToGravityFloor(InVelocity) * DeltaSeconds;
	FHitResult Hit(1.f);
	FVector RampVector = ComputeGroundMovementDelta(Delta, CurrentFloor.HitResult, CurrentFloor.bLineTrace);
	SafeMoveUpdatedComponent(RampVector, UpdatedComponent->GetComponentQuat(), true, Hit);
	float LastMoveTimeSlice = DeltaSeconds;

	if (Hit.bStartPenetrating)
	{
		// TODO: 这代码该如何处理？我不希望 slide along surface，感觉角直接穿过去会更好

		// Allow this hit to be used as an impact we can deflect off, otherwise we do nothing the rest of the update and appear to hitch.
		HandleImpact(Hit);
		// SlideAlongSurface(Delta, 1.f, Hit.Normal, Hit, true);

		if (Hit.bStartPenetrating)
		{
			OnCharacterStuckInGeometry(&Hit);
		}
	}
	else if (Hit.IsValidBlockingHit())
	{
		// We impacted something (most likely another ramp, but possibly a barrier).
		float PercentTimeApplied = Hit.Time;
		if ((Hit.Time > 0.f) && (GetGravitySpaceZ(Hit.Normal) > UE_KINDA_SMALL_NUMBER) && IsWalkable(Hit))
		{
			// Another walkable ramp.
			const float InitialPercentRemaining = 1.f - PercentTimeApplied;
			// 这里暂时先沿着地面走吧，既然都撞上东西了，应该这里不能飞起来
			RampVector = ComputeGroundMovementDelta(Delta * InitialPercentRemaining, Hit, false);
			LastMoveTimeSlice = InitialPercentRemaining * LastMoveTimeSlice;
			SafeMoveUpdatedComponent(RampVector, UpdatedComponent->GetComponentQuat(), true, Hit);

			const float SecondHitPercent = Hit.Time * InitialPercentRemaining;
			PercentTimeApplied = FMath::Clamp(PercentTimeApplied + SecondHitPercent, 0.f, 1.f);
		}

		if (Hit.IsValidBlockingHit())
		{
			HandleImpact(Hit, LastMoveTimeSlice, RampVector);
		}
	}
}

void URunnerMovementComponent::PhysWalking(float deltaTime, int32 Iterations)
{
	// Super::PhysWalking(deltaTime, Iterations);
	PhysWalking2(deltaTime, Iterations);
}

void URunnerMovementComponent::PhysWalking2(float deltaTime, int32 Iterations)
{
	SCOPE_CYCLE_COUNTER(STAT_SkateboardPhysWalking);

	if (deltaTime < MIN_TICK_TIME)
	{
		return;
	}

	if (!CharacterOwner || (!CharacterOwner->Controller && !bRunPhysicsWithNoController && !HasAnimRootMotion() && !CurrentRootMotion.HasOverrideVelocity() && (CharacterOwner->GetLocalRole() != ROLE_SimulatedProxy)))
	{
		Acceleration = FVector::ZeroVector;
		Velocity = FVector::ZeroVector;
		return;
	}

	if (!UpdatedComponent->IsQueryCollisionEnabled())
	{
		SetMovementMode(MOVE_Walking);
		return;
	}

	devCode(ensureMsgf(!Velocity.ContainsNaN(), TEXT("PhysWalking: Velocity contains NaN before Iteration (%s)\n%s"), *GetPathNameSafe(this), *Velocity.ToString()));

	bJustTeleported = false;
	bool bCheckedFall = false;
	bool bTriedLedgeMove = false;
	float remainingTime = deltaTime;

	const EMovementMode StartingMovementMode = MovementMode;
	const uint8 StartingCustomMovementMode = CustomMovementMode;

	// Perform the move
	while ((remainingTime >= MIN_TICK_TIME) && (Iterations < MaxSimulationIterations) && CharacterOwner && (CharacterOwner->Controller || bRunPhysicsWithNoController || HasAnimRootMotion() || CurrentRootMotion.HasOverrideVelocity() || (CharacterOwner->GetLocalRole() == ROLE_SimulatedProxy)))
	{
		Iterations++;
		bJustTeleported = false;
		const float timeTick = GetSimulationTimeStep(remainingTime, Iterations);
		remainingTime -= timeTick;

		// Save current values
		UPrimitiveComponent* const OldBase = GetMovementBase();
		const FVector PreviousBaseLocation = (OldBase != NULL) ? OldBase->GetComponentLocation() : FVector::ZeroVector;
		const FVector OldLocation = UpdatedComponent->GetComponentLocation();
		const FFindFloorResult OldFloor = CurrentFloor;

		RestorePreAdditiveRootMotionVelocity();
		// 不再要求 velocity 与 gravity 正交
		// MaintainHorizontalGroundVelocity();

		const FVector OldVelocity = Velocity;
		auto GravityDir = GetGravityDirection();
		Acceleration = FVector::VectorPlaneProject(Acceleration, -GravityDir);

		// Apply acceleration
		// TODO: 解释这个 ledge move 的逻辑
		const bool bSkipForLedgeMove = bTriedLedgeMove;
		if (!HasAnimRootMotion() && !CurrentRootMotion.HasOverrideVelocity() && !bSkipForLedgeMove)
		{
			CalcVelocity(timeTick, GroundFriction, false, GetMaxBrakingDeceleration());
			devCode(ensureMsgf(!Velocity.ContainsNaN(), TEXT("PhysWalking: Velocity contains NaN after CalcVelocity (%s)\n%s"), *GetPathNameSafe(this), *Velocity.ToString()));
		}

		ApplyRootMotionToVelocity(timeTick);
		devCode(ensureMsgf(!Velocity.ContainsNaN(), TEXT("PhysWalking: Velocity contains NaN after Root Motion application (%s)\n%s"), *GetPathNameSafe(this), *Velocity.ToString()));

		if (MovementMode != StartingMovementMode || CustomMovementMode != StartingCustomMovementMode)
		{
			// Root motion could have taken us out of our current mode
			// No movement has taken place this movement tick so we pass on full time/past iteration count
			StartNewPhysics(remainingTime + timeTick, Iterations - 1);
			return;
		}

		// Compute move parameters
		const FVector MoveVelocity = Velocity;
		const FVector Delta = timeTick * MoveVelocity;
		const bool bZeroDelta = Delta.IsNearlyZero();
		FStepDownResult StepDownResult;

		if (bZeroDelta)
		{
			remainingTime = 0.f;
		}
		else
		{
			// try to move forward
			MoveAlongFloor(MoveVelocity, timeTick, &StepDownResult);

			if (IsSwimming()) // just entered water
			{
				StartSwimming(OldLocation, OldVelocity, timeTick, remainingTime, Iterations);
				return;
			}
			else if (MovementMode != StartingMovementMode || CustomMovementMode != StartingCustomMovementMode)
			{
				// pawn ended up in a different mode, probably due to the step-up-and-over flow
				// let's refund the estimated unused time (if any) and keep moving in the new mode
				const float DesiredDist = Delta.Size();
				if (DesiredDist > UE_KINDA_SMALL_NUMBER)
				{
					const float ActualDist = ProjectToGravityFloor(UpdatedComponent->GetComponentLocation() - OldLocation).Size();
					remainingTime += timeTick * (1.f - FMath::Min(1.f, ActualDist / DesiredDist));
				}
				StartNewPhysics(remainingTime, Iterations);
				return;
			}
		}

		// Update floor.
		// StepUp might have already done it for us.
		if (StepDownResult.bComputedFloor)
		{
			CurrentFloor = StepDownResult.FloorResult;
		}
		else
		{
			FindFloor(UpdatedComponent->GetComponentLocation(), CurrentFloor, bZeroDelta, NULL);
		}

		// check for ledges here
		const bool bCheckLedges = !CanWalkOffLedges();
		if (bCheckLedges && !CurrentFloor.IsWalkableFloor())
		{
			// calculate possible alternate movement
			const FVector NewDelta = bTriedLedgeMove ? FVector::ZeroVector : GetLedgeMove(OldLocation, Delta, OldFloor);
			if (!NewDelta.IsZero())
			{
				// first revert this move
				RevertMove(OldLocation, OldBase, PreviousBaseLocation, OldFloor, false);

				// avoid repeated ledge moves if the first one fails
				bTriedLedgeMove = true;

				// Try new movement direction
				Velocity = NewDelta / timeTick;
				remainingTime += timeTick;
				Iterations--;
				continue;
			}
			else
			{
				// see if it is OK to jump
				// @todo collision : only thing that can be problem is that oldbase has world collision on
				bool bMustJump = bZeroDelta || (OldBase == NULL || (!OldBase->IsQueryCollisionEnabled() && MovementBaseUtility::IsDynamicBase(OldBase)));
				if ((bMustJump || !bCheckedFall) && CheckFall(OldFloor, CurrentFloor.HitResult, Delta, OldLocation, remainingTime, timeTick, Iterations, bMustJump))
				{
					return;
				}
				bCheckedFall = true;

				// revert this move
				RevertMove(OldLocation, OldBase, PreviousBaseLocation, OldFloor, true);
				remainingTime = 0.f;
				break;
			}
		}
		else
		{
			// Validate the floor check
			if (CurrentFloor.IsWalkableFloor())
			{
				if (ShouldCatchAir(OldFloor, CurrentFloor))
				{
					HandleWalkingOffLedge(OldFloor.HitResult.ImpactNormal, OldFloor.HitResult.Normal, OldLocation, timeTick);
					if (IsMovingOnGround())
					{
						// If still walking, then fall. If not, assume the user set a different mode they want to keep.
						StartFalling(Iterations, remainingTime, timeTick, Delta, OldLocation);
					}
					return;
				}

				AdjustFloorHeight();
				SetBase(CurrentFloor.HitResult.Component.Get(), CurrentFloor.HitResult.BoneName);
			}
			else if (CurrentFloor.HitResult.bStartPenetrating && remainingTime <= 0.f)
			{
				// The floor check failed because it started in penetration
				// We do not want to try to move downward because the downward sweep failed, rather we'd like to try to pop out of the floor.
				FHitResult Hit(CurrentFloor.HitResult);
				Hit.TraceEnd = Hit.TraceStart + MAX_FLOOR_DIST * -GetGravityDirection();
				const FVector RequestedAdjustment = GetPenetrationAdjustment(Hit);
				ResolvePenetration(RequestedAdjustment, Hit, UpdatedComponent->GetComponentQuat());
				bForceNextFloorCheck = true;
			}

			// check if just entered water
			if (IsSwimming())
			{
				StartSwimming(OldLocation, Velocity, timeTick, remainingTime, Iterations);
				return;
			}

			// See if we need to start falling.
			if (!CurrentFloor.IsWalkableFloor() && !CurrentFloor.HitResult.bStartPenetrating)
			{
				const bool bMustJump = bJustTeleported || bZeroDelta || (OldBase == NULL || (!OldBase->IsQueryCollisionEnabled() && MovementBaseUtility::IsDynamicBase(OldBase)));
				if ((bMustJump || !bCheckedFall) && CheckFall(OldFloor, CurrentFloor.HitResult, Delta, OldLocation, remainingTime, timeTick, Iterations, bMustJump))
				{
					return;
				}
				bCheckedFall = true;
			}
		}

		// If we didn't move at all this iteration then abort (since future iterations will also be stuck).
		if (UpdatedComponent->GetComponentLocation() == OldLocation)
		{
			remainingTime = 0.f;
			break;
		}
	}
}

#undef devCode