// Fill out your copyright notice in the Description page of Project Settings.

#include "RunnerMovementComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "CoreGlobals.h"
#include "DrawDebugHelpers.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "Logging/LogVerbosity.h"
#include "Math/RotationMatrix.h"
#include "Math/UnrealMathUtility.h"
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
	// 我们希望速度与地面切线一致，因此移动的距离和速度保持一致
	bMaintainHorizontalGroundVelocity = false;
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
	// UE_LOG(LogTemp, Log, TEXT("Skateboard Velocity: %s, Size: %f"), *Velocity.ToString(), Velocity.Size());
}

void URunnerMovementComponent::UpdateSkateBoardRotation(float DeltaTime)
{
	if (SkateboardMesh && DeltaTime > 0.0f && IsWalking())
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

		DrawDebugBox(GetWorld(), AdjustedBWPos, FVector(10.0f), FColor::Red, false, 1.0f, 0, 1.0f);
		DrawDebugBox(GetWorld(), AdjustedFWPos, FVector(10.0f), FColor::Green, false, 1.0f, 0, 1.0f);
		DrawDebugBox(GetWorld(), FVector(AdjustedBWPos.X, AdjustedBWPos.Y, GroundHeightInBW), FVector(10.0f), FColor::Red, false, 1.0f, 0, 1.0f);
		DrawDebugBox(GetWorld(), FVector(AdjustedFWPos.X, AdjustedFWPos.Y, GroundHeightInFW), FVector(10.0f), FColor::Green, false, 1.0f, 0, 1.0f);

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

		auto Yaxis = (AdjustedBRWPos - AdjustedBLWPos).GetSafeNormal();
		auto NewPitch = FMath::RadiansToDegrees(FMath::Asin(-Yaxis.Z));

		NewWSRotation.Pitch = NewPitch;
		DesiredTransform.SetRotation(FQuat(NewWSRotation));

		auto InterpRotation = FMath::RInterpConstantTo(SkateboardMesh->GetComponentRotation(), NewWSRotation, DeltaTime, SkateboardRotationSpeed);
		InterpRotation.Yaw = NewWSRotation.Yaw;
		SkateboardMesh->SetWorldRotation(InterpRotation);
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
		UE_LOG(LogTemp, Warning, TEXT("ShouldCatchAir: OldDot: %f, NewDot: %f, Delta: %f"), OldDot, NewDot, NewDot - OldDot);
		return true;
	}
	return false;
}

void URunnerMovementComponent::CalcVelocity(float DeltaTime, float Friction, bool bFluid, float BrakingDeceleration)
{
	auto OldVelocity = Velocity;
	Super::CalcVelocity(DeltaTime, Friction, bFluid, BrakingDeceleration);
	// 速度设置为与滑板的前进的方向一致
	if (IsWalking() && SkateboardMesh)
	{
		auto NewVelocity = Velocity.Size() * SkateboardMesh->GetRightVector();
		Velocity.X = NewVelocity.X;
		Velocity.Y = NewVelocity.Y;
		Velocity.Z = NewVelocity.Z;
		// UE_LOG(LogTemp, Log, TEXT("URunnerMovementComponent::CalcVelocity called with DeltaTime: %f, Friction: %f, bFluid: %d, BrakingDeceleration: %f, OldVelocity: %s, NewVelocity: %s"), DeltaTime, Friction, bFluid, BrakingDeceleration, *OldVelocity.ToString(), *Velocity.ToString());
	}
	// if (IsWalking() && NewVelocity.Z < Velocity.Z)
	// {
	// 	Velocity.Z = FMath::Max(NewVelocity.Z, Velocity.Z - SkateboardWalkingZReduceSpeed * DeltaTime);
	// }
	// else
	// {
	// 	Velocity.Z = NewVelocity.Z;
	// }
	// Velocity = Velocity.Size() * SkateboardMesh->GetRightVector();
	// UE_LOG(LogTemp, Log, TEXT("Skateboard Velocity: %s, Size: %f"), *Velocity.ToString(), Velocity.Size());
}

void URunnerMovementComponent::MoveAlongFloor(const FVector& InVelocity, float DeltaSeconds, FStepDownResult* OutStepDownResult)
{
	if (!CurrentFloor.IsWalkableFloor())
	{
		return;
	}

	// 相比于 UE 的实现，我们不再将 Velocity 投影到地面上
	const FVector Delta = InVelocity * DeltaSeconds;
	FHitResult Hit(1.f);
	auto RampVector = Delta;
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