// Fill out your copyright notice in the Description page of Project Settings.

#include "RunnerMovementComponent.h"
#include "BarrierSpawner.h"
#include "Components/AudioComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "CoreGlobals.h"
#include "DrawDebugHelpers.h"
#include "Engine/EngineTypes.h"
#include "Engine/TimerHandle.h"
#include "EngineUtils.h"
#include "EnhancedInputComponent.h"
#include "GameFramework/Character.h"
#include "Kismet/GameplayStatics.h"
#include "Logging/LogVerbosity.h"
#include "Math/Color.h"
#include "Math/RotationMatrix.h"
#include "Math/UnrealMathUtility.h"
#include "Runner/RunnerCharacter.h"
#include "TimerManager.h"
#include "UObject/WeakObjectPtrTemplates.h"
#include "WorldGenerator.h"

DECLARE_CYCLE_STAT(TEXT("Skateboard PhysWalking"), STAT_SkateboardPhysWalking, STATGROUP_Character);
DECLARE_CYCLE_STAT(TEXT("Skateboard PhysFalling"), STAT_SkateboardPhysFalling, STATGROUP_Character);
DECLARE_CYCLE_STAT(TEXT("Skateboard AdjustFloorHeight"), STAT_SkateboardAdjustFloorHeight, STATGROUP_Character);

DEFINE_LOG_CATEGORY_STATIC(LogRunnerMovement, Log, All);

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
	GetOwner()->ForEachComponent<UAudioComponent>(false, [this](UAudioComponent* Component) {
		if (Component->GetName().Equals(TEXT("AudioForLand")))
		{
			LandAudio = Component;
			LandAudio->OnAudioFinishedNative.AddUObject(this, &URunnerMovementComponent::OnAudioFinished);
			return false; // Stop iterating once we find the LandAudio
		}
		return true; // Continue iterating
	});

	if (!SkateboardMesh)
	{
		// UE_LOG(LogRunnerMovement, Warning, TEXT("URunnerMovementComponent: SkateboardMesh not found!"));
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
		// UE_LOG(LogRunnerMovement, Warning, TEXT("ARunnerCharacter: WorldGenerator not found!"));
	}
}

void URunnerMovementComponent::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	// 移动后更新滑板的旋转
	UpdateSkateBoardRotation(DeltaTime);
	auto OldInAirTime = InAirTime;
	InAirTime += DeltaTime;
	// if (OldInAirTime < PlayTakeOffSoundInAirTime && InAirTime >= PlayTakeOffSoundInAirTime)
	if (!bHipHop)
	{
		if (LandAudio && !LandAudio->IsPlaying() && CurrentAudioInterval >= LandAudioInterval)
		{
			auto Pos = GetOwner()->GetActorLocation();
			auto VisualPos = WorldGenerator->GetVisualHeightFromHorizontalPos(FVector2D(Pos));
			if (Pos.Z >= VisualPos + PlayTakeOffSoundMinHeight)
			{
				LandAudio->SetSound(TakeOffSound);
				LandAudio->Play();
				Cast<ARunnerCharacter>(GetOwner())->StartDanceMontage();
				bHipHop = true;
			}
			// UE_LOG(LogRunnerMovement, Log, TEXT("URunnerMovementComponent:: TickComponent: Player Z: %f, Visual Z: %f, Difference: %f"),
			// 		Pos.Z, VisualPos, Pos.Z - VisualPos);
		}
	}

	CurrentAudioInterval += DeltaTime;
	// UE_LOG(LogRunnerMovement, Log, TEXT("Skateboard Ground Velocity Size: %f, Thrusting: %d, SlowDown: %d, MaxSpeed: %f"), Velocity.Size2D(), Thrusting, SlowDown, GetMaxSpeed());
}

ELevel URunnerMovementComponent::CalcLandLevel(FRotator TargetRotation) const
{
	// TODO: Implement land level calculation based on target rotation
	auto CurrentSkateboardRotation = SkateboardMesh->GetComponentRotation();
	auto DiffPitch = FMath::Abs(CurrentSkateboardRotation.Pitch - TargetRotation.Pitch);
	auto DiffRoll = FMath::Abs(CurrentSkateboardRotation.Roll - TargetRotation.Roll);
	// if (DiffPitch > BadDiffPitch)
	// {
	// 	// UE_LOG(LogRunnerMovement, Warning, TEXT("Bad Level, Target Pitch: %f, Current Pitch: %f, Target Roll: %f, Current Roll: %f"),
	// 	// 		TargetRotation.Pitch, CurrentSkateboardRotation.Pitch, TargetRotation.Roll, CurrentSkateboardRotation.Roll);
	// 	return ELevel::Bad;
	// }
	// else if (DiffRoll > BadDiffRoll)
	// {
	// 	// UE_LOG(LogRunnerMovement, Warning, TEXT("Bad Level, Target Roll: %f, Current Roll: %f, Target Pitch: %f, Current Pitch: %f"),
	// 	// 		TargetRotation.Roll, CurrentSkateboardRotation.Roll, TargetRotation.Pitch, CurrentSkateboardRotation.Pitch);
	// 	return ELevel::Bad;
	// }
	// else if (DiffPitch <= PerfectDiffPitch && DiffRoll <= PerfectDiffRoll)
	// {
	// 	// UE_LOG(LogRunnerMovement, Warning, TEXT("Perfect Level, Target Pitch: %f, Current Pitch: %f, Target Roll: %f, Current Roll: %f"),
	// 	// 		TargetRotation.Pitch, CurrentSkateboardRotation.Pitch, TargetRotation.Roll, CurrentSkateboardRotation.Roll);
	// 	return ELevel::Perfect;
	// }
	// else if (DiffPitch <= PerfectDiffPitch || DiffRoll <= PerfectDiffRoll)
	// {
	// 	// UE_LOG(LogRunnerMovement, Warning, TEXT("Good Level, Target Pitch: %f, Current Pitch: %f, Target Roll: %f, Current Roll: %f"),
	// 	// 		TargetRotation.Pitch, CurrentSkateboardRotation.Pitch, TargetRotation.Roll, CurrentSkateboardRotation.Roll);
	// 	return ELevel::Good;
	// }
	// UE_LOG(LogRunnerMovement, Warning, TEXT("Normal Level, Target Pitch: %f, Current Pitch: %f, Target Roll: %f, Current Roll: %f"),
	// 		TargetRotation.Pitch, CurrentSkateboardRotation.Pitch, TargetRotation.Roll, CurrentSkateboardRotation.Roll);

	if (DiffRoll <= PerfectDiffRoll)
	{
		UE_LOG(LogRunnerMovement, Log, TEXT("Perfect Level, Target Roll: %f, Current Roll: %f"),
				TargetRotation.Roll, CurrentSkateboardRotation.Roll);
		return ELevel::Perfect;
	}
	else if (DiffRoll >= BadDiffRoll)
	{
		UE_LOG(LogRunnerMovement, Warning, TEXT("Bad Level, Target Roll: %f, Current Roll: %f"),
				TargetRotation.Roll, CurrentSkateboardRotation.Roll);
		return ELevel::Bad;
	}
	else
	{
		UE_LOG(LogRunnerMovement, Log, TEXT("Good Level, Target Roll: %f, Current Roll: %f"),
				TargetRotation.Roll, CurrentSkateboardRotation.Roll);
		return ELevel::Good;
	}
	// return ELevel::Normal;
}

static FName GetLandText(ELevel LandLevel, int32 PerfectCombo)
{
	switch (LandLevel)
	{
		case ELevel::Perfect:
		{
			if (PerfectCombo == 1)
			{
				return FName(TEXT("完美着陆"));
			}
			else
			{
				FString PerfectComboString = FString::Printf(TEXT("%dX 完美着陆"), PerfectCombo);
				return FName(*PerfectComboString);
			}
		}
		case ELevel::Good:
			return FName(TEXT("优秀着陆"));
		case ELevel::Bad:
		default:
			return FName(TEXT("拙劣着陆"));
	}
}

static FLinearColor GetTextColor(ELevel LandLevel)
{
	switch (LandLevel)
	{
		case ELevel::Perfect:
		case ELevel::Good:
			return FLinearColor(1.0f, 0.932972f, 0.014806f, 1.0f); // Yellow
		case ELevel::Bad:
		default:
			return FLinearColor::White; // White
	}
}

void URunnerMovementComponent::ProcessLanded(const FHitResult& Hit, float remainingTime, int32 Iterations)
{
	bHipHop = false; // 重置跳过舞状态
	bool bAccuracy = false;
	LastTickTargetRotation = CalcWakingTargetRotation(bAccuracy);

	// 仅当落到地面上时才处理
	auto* Runner = Cast<ARunnerCharacter>(GetOwner());
	if (Hit.GetActor() == WorldGenerator)
	{
		if (Runner->IsInMud())
		{
			Runner->StartSlowDown(ESlowDownSource::Mud);
		}
		if (InAirTime > MinimumAirTime)
		{
			auto LandLevel = ELevel::Bad;
			if (bAccuracy)
			{
				LandLevel = CalcLandLevel(LastTickTargetRotation);
				PerfectCombo = (LandLevel == ELevel::Perfect) ? PerfectCombo + 1 : 0;
				ProcessLandLevel(LandLevel);
				auto LandText = GetLandText(LandLevel, PerfectCombo);
				Runner->ShowLandUI(LandText, GetTextColor(LandLevel));
			}
		}
	}
	// 重置空中时间
	InAirTime = 0.0f;
	StopDown();

	// UGameplayStatics::PlaySound2D(this, LandSound);

	Super::ProcessLanded(Hit, remainingTime, Iterations);
	Runner->StopActiveMontage(0.2f);
}

void URunnerMovementComponent::ProcessLandLevel(ELevel LandLevel)
{
	auto* Runner = Cast<ARunnerCharacter>(GetOwner());
	if (LandAudio && !LandAudio->IsPlaying() && CurrentAudioInterval >= LandAudioInterval)
	{
		auto State = LandAudio->GetPlayState();
		UE_LOG(LogRunnerMovement, Log, TEXT("URunnerMovementComponent::ProcessLandLevel: Play Land Sound for Level %d, State %s"), static_cast<int32>(LandLevel), *UEnum::GetValueAsString(State));
		LandAudio->SetSound(LandSoundByLevel[static_cast<int32>(LandLevel)]);
		LandAudio->Play();
	}
	if (Runner)
	{
		switch (LandLevel)
		{
			case ELevel::Perfect:
			{
				Runner->AddGas(GasPercentageWhenPerfect);
				Runner->StartThrust(EThrustSource::FreeThrust, PerfectThrustSpeedTime);
			}
			break;
			case ELevel::Good:
				Runner->AddGas(GasPercentageWhenGood);
				break;
			case ELevel::Bad:
			{
				Runner->StartSlowDown(ESlowDownSource::BadLand);
				FTimerHandle SlowDownTimerHandle;
				TWeakObjectPtr<ARunnerCharacter> WeakRunner(Runner);
				Runner->GetWorld()->GetTimerManager().SetTimer(SlowDownTimerHandle, [WeakRunner]() {
					if (WeakRunner.IsValid())
					{
						WeakRunner->StopSlowDown();
					} }, BadReduceSpeedTime, false);
			}
			break;
			default:
				break;
		}
	}
}

void URunnerMovementComponent::TakeDamage()
{
	// 受伤音效会打断其它角色音效
	LandAudio->SetSound(DamageSound);
	LandAudio->Play();
}

//void URunnerMovementComponent::NotifyJumpApex()
//{
//	UE_LOG(LogRunnerMovement, Log, TEXT("URunnerMovementComponent::NotifyJumpApex called, InAirTime: %f, PlayTakeOffSoundInAirTime: %f, CurrentAudioInterval: %f"), InAirTime, PlayTakeOffSoundInAirTime, CurrentAudioInterval);
//	if (LandAudio && !LandAudio->IsPlaying() && InAirTime >= PlayTakeOffSoundInAirTime && CurrentAudioInterval >= LandAudioInterval)
//	{
//		LandAudio->SetSound(TakeOffSound);
//		LandAudio->Play();
//	}
//
//	Super::NotifyJumpApex();
//}

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

		// UPrimitiveComponent* HitComponentBW = nullptr;
		// UPrimitiveComponent* HitComponentFW = nullptr;
		auto GroundHeightInBW = WorldGenerator->GetHeightFromHorizontalPos(FVector2D(BWWSPos.X, BWWSPos.Y) /*, HitComponentBW*/);
		auto GroundHeightInFW = WorldGenerator->GetHeightFromHorizontalPos(FVector2D(FWWSPos.X, FWWSPos.Y) /*, HitComponentFW*/);

		auto bValidFloor = CurrentFloor.HitResult.IsValidBlockingHit();
		auto TraceDistanceFW = SkateboardTraceDistance;
		auto TraceDistanceBW = SkateboardTraceDistance;
		// if (bValidFloor && HitComponentFW != CurrentFloor.HitResult.GetComponent())
		// {
		// 	TraceDistanceFW = SkateboardTraceDistance / 2.0f;
		// }
		// if (bValidFloor && HitComponentBW != CurrentFloor.HitResult.GetComponent())
		// {
		// 	TraceDistanceBW = SkateboardTraceDistance / 2.0f;
		// }
		auto AdjustedBWHeight = FMath::Abs(BWWSPos.Z - GroundHeightInBW) >= TraceDistanceBW ? BWWSPos.Z : GroundHeightInBW;
		auto AdjustedBWPos = FVector(BWWSPos.X, BWWSPos.Y, AdjustedBWHeight);
		auto AdjustedFWHeight = FMath::Abs(FWWSPos.Z - GroundHeightInFW) >= TraceDistanceFW ? FWWSPos.Z : GroundHeightInFW;
		auto AdjustedFWPos = FVector(FWWSPos.X, FWWSPos.Y, AdjustedFWHeight);

		bAccuracy = FMath::Abs(BWWSPos.Z - GroundHeightInBW) < TraceDistanceBW && FMath::Abs(FWWSPos.Z - GroundHeightInFW) < TraceDistanceFW;

		// DrawDebugBox(GetWorld(), AdjustedBWPos, FVector(10.0f), FColor::Red, false, 1.0f, 0, 1.0f);
		// DrawDebugBox(GetWorld(), AdjustedFWPos, FVector(10.0f), FColor::Green, false, 1.0f, 0, 1.0f);
		// DrawDebugBox(GetWorld(), FVector(AdjustedBWPos.X, AdjustedBWPos.Y, GroundHeightInBW), FVector(10.0f), FColor::Red, false, 1.0f, 0, 1.0f);
		// DrawDebugBox(GetWorld(), FVector(AdjustedFWPos.X, AdjustedFWPos.Y, GroundHeightInFW), FVector(10.0f), FColor::Green, false, 1.0f, 0, 1.0f);
		// if (Print)
		// {
		// 	if (BWWSPos.Z - GroundHeightInBW >= SkateboardTraceDistance)
		// 	{
		// 		UE_LOG(LogRunnerMovement, Warning, TEXT("Back Wheel Too High! BW Z Pos: %f, GroundHeight: %f"),
		// 				BWWSPos.Z, GroundHeightInBW);
		// 	}
		// 	else
		// 	{
		// 		UE_LOG(LogRunnerMovement, Log, TEXT("Back Wheel OK BW Z Pos: %f, GroundHeight: %f"),
		// 				BWWSPos.Z, GroundHeightInBW);
		// 	}
		// 	if (FWWSPos.Z - GroundHeightInFW >= SkateboardTraceDistance)
		// 	{
		// 		UE_LOG(LogRunnerMovement, Warning, TEXT("Front Wheel Too High! FW Z Pos: %f, GroundHeight: %f"),
		// 				FWWSPos.Z, GroundHeightInFW);
		// 	}
		// 	else
		// 	{
		// 		UE_LOG(LogRunnerMovement, Log, TEXT("Front Wheel OK FW Z Pos: %f, GroundHeight: %f"),
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

		// UPrimitiveComponent* HitComponentBLW = nullptr;
		// UPrimitiveComponent* HitComponentBRW = nullptr;
		auto GroundHeightInBLW = WorldGenerator->GetHeightFromHorizontalPos(FVector2D(BLWWSPos.X, BLWWSPos.Y) /*, HitComponentBLW*/);
		auto GroundHeightInBRW = WorldGenerator->GetHeightFromHorizontalPos(FVector2D(BRWWSPos.X, BRWWSPos.Y) /*, HitComponentBRW*/);

		auto TraceDistanceBLW = SkateboardTraceDistance;
		auto TraceDistanceBRW = SkateboardTraceDistance;
		// if (bValidFloor && HitComponentBLW != CurrentFloor.HitResult.GetComponent())
		// {
		// 	TraceDistanceBLW = SkateboardTraceDistance / 2.0f;
		// }
		// if (bValidFloor && HitComponentBRW != CurrentFloor.HitResult.GetComponent())
		// {
		// 	TraceDistanceBRW = SkateboardTraceDistance / 2.0f;
		// }

		auto AdjustedBLWHeight = FMath::Abs(BLWWSPos.Z - GroundHeightInBLW) >= TraceDistanceBLW ? BLWWSPos.Z : GroundHeightInBLW;
		auto AdjustedBLWPos = FVector(BLWWSPos.X, BLWWSPos.Y, AdjustedBLWHeight);
		auto AdjustedBRWHeight = FMath::Abs(BRWWSPos.Z - GroundHeightInBRW) >= TraceDistanceBRW ? BRWWSPos.Z : GroundHeightInBRW;
		auto AdjustedBRWPos = FVector(BRWWSPos.X, BRWWSPos.Y, AdjustedBRWHeight);

		bAccuracy &= FMath::Abs(BLWWSPos.Z - GroundHeightInBLW) < TraceDistanceBLW && FMath::Abs(BRWWSPos.Z - GroundHeightInBRW) < TraceDistanceBRW;

		// if (Print)
		// {
		// 	if (BLWWSPos.Z - GroundHeightInBLW >= SkateboardTraceDistance)
		// 	{
		// 		UE_LOG(LogRunnerMovement, Warning, TEXT("Back Left Wheel Too High! BLW Z Pos: %f, GroundHeight: %f"),
		// 				BLWWSPos.Z, GroundHeightInBLW);
		// 	}
		// 	else
		// 	{
		// 		UE_LOG(LogRunnerMovement, Log, TEXT("Back Left Wheel OK BLW Z Pos: %f, GroundHeight: %f"),
		// 				BLWWSPos.Z, GroundHeightInBLW);
		// 	}
		// 	if (BRWWSPos.Z - GroundHeightInBRW >= SkateboardTraceDistance)
		// 	{
		// 		UE_LOG(LogRunnerMovement, Warning, TEXT("Back Right Wheel Too High! BRW Z Pos: %f, GroundHeight: %f"),
		// 				BRWWSPos.Z, GroundHeightInBRW);
		// 	}
		// 	else
		// 	{
		// 		UE_LOG(LogRunnerMovement, Log, TEXT("Back Right Wheel OK BRW Z Pos: %f, GroundHeight: %f"),
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
	// 不能将这个函数推迟到 AActor::NotifyHit 中，因为那时候可能已经积累了许多个 blocking hit 了
	// HandleBlockingHit(Hit);
}

// bool URunnerMovementComponent::HandleBlockingHit(const FHitResult& Hit)
// {
// 	auto bWalkable = IsWalkable(Hit);
// 	if (!bWalkable)
// 	{
// 		auto* HitComponent = Hit.GetComponent();
// 		if (HitComponent)
// 		{
// 			// UE_LOG(LogRunnerMovement, Warning, TEXT("URunnerMovementComponent::HandleImpact: Hit Object %s!"), *HitComponent->GetName());
// 			auto* ISM = Cast<UInstancedStaticMeshComponent>(HitComponent);
// 			if (ISM)
// 			{
// 				auto* BI = ISM->GetBodyInstance(FName(), true, Hit.Item);
// 				if (BI->GetResponseToChannel(ECC_Pawn) == ECR_Block)
// 				{
// 					BI->SetResponseToChannel(ECC_Pawn, ECR_Ignore);
// 					// Restore the collision response after IgnoreTime
// 					FTimerHandle Handle;
// 					TWeakObjectPtr<UInstancedStaticMeshComponent> WeakISM = ISM;
// 					GetWorld()->GetTimerManager().SetTimer(
// 							Handle, [WeakISM, HitItem = Hit.Item]() {
// 								auto ISM = WeakISM.Get();
// 								if (ISM)
// 								{
// 									auto* BI = ISM->GetBodyInstance(FName(), true, HitItem);
// 									BI->SetResponseToChannel(ECC_Pawn, ECR_Block);
// 								}
// 							},
// 							IgnoreTime, false);
// 					Cast<ARunnerCharacter>(GetOwner())->TakeHitImpact();
// 				}
// 			}
// 			// 碰撞到的是普通的 Primitive Component
// 			else
// 			{
// 				if (HitComponent->GetCollisionResponseToChannel(ECC_Pawn) == ECR_Block)
// 				{
// 					HitComponent->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
// 					TWeakObjectPtr<UPrimitiveComponent> WeakComponent = HitComponent;
// 					FTimerHandle Handle;
// 					GetWorld()->GetTimerManager().SetTimer(
// 							Handle, [WeakComponent]() {
// 								if (WeakComponent.IsValid())
// 								{
// 									WeakComponent->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
// 								}
// 							},
// 							IgnoreTime, false);
// 					Cast<ARunnerCharacter>(GetOwner())->TakeHitImpact();
// 				}
// 			}
// 		}
// 	}
// 	// 返回 true 表示这是一个需要处理的阻挡碰撞
// 	return !bWalkable;
// }

bool URunnerMovementComponent::CheckFall(const FFindFloorResult& OldFloor, const FHitResult& Hit, const FVector& Delta, const FVector& OldLocation, float remainingTime, float timeTick, int32 Iterations, bool bMustJump)
{
	if (!HasValidData())
	{
		return false;
	}

	if (bMustJump || CanWalkOffLedges())
	{
		HandleWalkingOffLedge(OldFloor.HitResult.ImpactNormal, OldFloor.HitResult.Normal, OldLocation, timeTick);
		if (IsMovingOnGround())
		{
			// If still walking, then fall. If not, assume the user set a different mode they want to keep.
			OldHit = OldFloor.HitResult;
			StartFalling(Iterations, remainingTime, timeTick, Delta, OldLocation);
		}
		return true;
	}
	return false;
}

void URunnerMovementComponent::StartFalling(int32 Iterations, float remainingTime, float timeTick, const FVector& Delta, const FVector& subLoc)
{
	InAirTime = 0.0f; // 重置空中时间
	Velocity.Z = CalcStartZVelocity();
	// bNotifyApex = true; // 确保在开始下落时通知跳跃顶点

	// UGameplayStatics::PlaySound2D(this, TakeOffSound);
	// UE_LOG(LogRunnerMovement, Warning, TEXT("URunnerMovementComponent::StartFalling: Start Falling! New Z Velocity: %f, Actor Location: %s"), Velocity.Z, *GetOwner()->GetActorLocation().ToString());
	Super::StartFalling(Iterations, remainingTime, timeTick, Delta, subLoc);
	// UE_LOG(LogRunnerMovement, Warning, TEXT("URunnerMovementComponent::StartFalling: Start Falling Call end! New Z Velocity: %f, Actor Location: %s"), Velocity.Z, *GetOwner()->GetActorLocation().ToString());
}

bool URunnerMovementComponent::ShouldCatchAir(const FFindFloorResult& OldFloor, const FFindFloorResult& NewFloor)
{
	ensure(OldFloor.HitResult.IsValidBlockingHit() && NewFloor.HitResult.IsValidBlockingHit());
	auto OldHitActor = OldFloor.HitResult.GetActor();
	auto NewHitActor = NewFloor.HitResult.GetActor();

	if (OldHitActor == WorldGenerator && NewHitActor != WorldGenerator)
	{
		// 离开地面，脱离泥潭
		auto* Runner = Cast<ARunnerCharacter>(GetOwner());
		if (Runner->IsInMud())
		{
			Runner->StopSlowDown();
		}
	}
	else if (OldHitActor != WorldGenerator && NewHitActor == WorldGenerator)
	{
		// 重新接触地面
		auto* Runner = Cast<ARunnerCharacter>(GetOwner());
		if (Runner->IsInMud())
		{
			Runner->StartSlowDown(ESlowDownSource::Mud);
		}
	}

	// 只处理地面的情况
	if (OldHitActor != WorldGenerator || NewHitActor != WorldGenerator)
	{
		return Super::ShouldCatchAir(OldFloor, NewFloor);
	}

	FVector OldNormal, NewNormal;
	if (bUseVisualNormal)
	{
		// 使用 WorldGenerator 获取视觉法线
		OldNormal = WorldGenerator->GetNormalFromHorizontalPos(FVector2D(OldFloor.HitResult.ImpactPoint));
		NewNormal = WorldGenerator->GetNormalFromHorizontalPos(FVector2D(NewFloor.HitResult.ImpactPoint));
	}
	else
	{
		// 使用 HitResult 的法线
		OldNormal = OldFloor.HitResult.ImpactNormal;
		NewNormal = NewFloor.HitResult.ImpactNormal;
	}

	auto VelocityDir = Velocity.GetSafeNormal();

	auto GroundVdir = Velocity.GetSafeNormal2D();
	auto Delta = WorldGenerator->CellSize;
	// auto CurrentPos =

	auto TanNew = FVector::VectorPlaneProject(VelocityDir, OldNormal);
	auto TanOld = FVector::VectorPlaneProject(VelocityDir, NewNormal);

	// 计算一阶导数
	auto SizeNew = TanNew.Size2D();
	auto dNew = SizeNew != 0.0 ? TanNew.Z / SizeNew : 0.0;
	auto SizeOld = TanOld.Size2D();
	auto dOld = SizeOld != 0.0 ? TanOld.Z / SizeOld : 0.0;

	auto GroundNew = FVector(NewFloor.HitResult.ImpactPoint.X, NewFloor.HitResult.ImpactPoint.Y, 0.0);
	auto GroundOld = FVector(OldFloor.HitResult.ImpactPoint.X, OldFloor.HitResult.ImpactPoint.Y, 0.0);
	auto DeltaDis = (GroundNew - GroundOld).Size2D();

	// 二阶导数
	auto dd = DeltaDis > 0.0 ? (dNew - dOld) / DeltaDis : 0.0;

	auto Length = FMath::Pow(1 + dNew * dNew, 1.5);
	auto Curvature = dd / Length;

	auto VelocitySize = Velocity.Size2D();
	// UE_LOG(LogRunnerMovement, Warning, TEXT("GroundNewPos: %s, GroundOldPos: %s, NormalNew: %s, NormalOld: %s, TanNew: %s, TanOld: %s, dNew: %lf, dOld: %lf, dd: %lf, Length: %lf, Curvature: %lf"), *GroundNew.ToString(), *GroundOld.ToString(), *NewNormal.ToString(), *OldNormal.ToString(), *TanNew.ToString(), *TanOld.ToString(), dNew, dOld, dd, Length, Curvature * VelocitySize * VelocitySize);

	auto OldDot = FVector::DotProduct(OldNormal, VelocityDir);
	auto NewDot = FVector::DotProduct(NewNormal, VelocityDir);

	if (bUseCurvatureForTakeoff && Curvature * VelocitySize * VelocitySize > TakeoffThreshold)
	{
		// 离开地面，脱离泥潭
		auto* Runner = Cast<ARunnerCharacter>(GetOwner());
		if (Runner->IsInMud())
		{
			Runner->StopSlowDown();
		}
		// UE_LOG(LogRunnerMovement, Warning, TEXT("ShouldCatchAir: StartFalling!"));
		return true;
	}
	else if (!bUseCurvatureForTakeoff && (NewDot - OldDot) >= DeltaNormalThreshold)
	{
		// 离开地面，脱离泥潭
		auto* Runner = Cast<ARunnerCharacter>(GetOwner());
		if (Runner->IsInMud())
		{
			Runner->StopSlowDown();
		}
		// UE_LOG(LogRunnerMovement, Warning, TEXT("ShouldCatchAir: OldDot: %f, NewDot: %f, Delta: %f"), OldDot, NewDot, NewDot - OldDot);
		return true;
	}
	return false;
}

double URunnerMovementComponent::CalcStartZVelocity(double Roll, double GroundSpeedSize, double TakeoffSpeedScale, double MaxStartZVelocityInAir)
{
	double ResultZ = 0.0;
	// 朝下的方向
	if (Roll >= 0.0f)
	{
		auto TanTheta = FMath::Tan(FMath::DegreesToRadians(Roll));
		auto ZVelocity = TanTheta * GroundSpeedSize;
		// auto Alpha = FMath::Clamp(LastRoll / MaxStartRollAngleInAir, 0.0f, 1.0f);
		ResultZ = -FMath::Clamp(ZVelocity, 0.0, MaxStartZVelocityInAir);
	}
	// 朝上的方向
	else
	{
		auto TanTheta = FMath::Tan(FMath::DegreesToRadians(-Roll));
		auto ZVelocity = TanTheta * GroundSpeedSize;
		// auto Alpha = FMath::Clamp(-LastRoll / MaxStartRollAngleInAir, 0.0f, 1.0f);
		ResultZ = FMath::Clamp(ZVelocity * TakeoffSpeedScale, 0.0, MaxStartZVelocityInAir);
	}
	// UE_LOG(LogRunnerMovement, Warning, TEXT("LastRoll: %f, TanTheta: %f, ResultZ: %f"), Roll, FMath::Tan(FMath::DegreesToRadians(Roll)), ResultZ);
	return ResultZ;
}

double URunnerMovementComponent::CalcStartZVelocity() const
{
	auto LastRoll = LastTickTargetRotation.Roll;
	if (OldHit.IsValidBlockingHit())
	{
		auto OldHitActor = Cast<ABarrierSpawner>(OldHit.GetActor());
		if (OldHitActor && OldHitActor->BarrierHasCustomSlope())
		{
			LastRoll = OldHitActor->GetCustomSlopeAngle(OldHit.Item);
		}
		OldHit.Reset();
	}
	// UE_LOG(LogRunnerMovement, Warning, TEXT("URunnerMovementComponent::CalcStartZVelocity: LastRoll: %f"), LastRoll);
	auto ResultZ = CalcStartZVelocity(LastRoll, Velocity.Size2D(), TakeoffSpeedScale, MaxStartZVelocityInAir);
	return ResultZ;
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

	// UE_LOG(LogRunnerMovement, Log, TEXT("URunnerMovementComponent::CalcVelocity called with bZeroRequestedAcceleration: %s, bVelocityOverMax: %s, AnalogInputModifier: %f, MaxSpeed: %f"), bZeroRequestedAcceleration ? TEXT("true") : TEXT("false"), bVelocityOverMax ? TEXT("true") : TEXT("false"), AnalogInputModifier, MaxSpeed);
	// UE_LOG(LogRunnerMovement, Log, TEXT("URunnerMovementComponent::CalcVelocity called with VectorImm: %s, VectorImm2: %s, NewMaxInput: %f"), *VectorImm.ToString(), *VectorImm2.ToString(), NewMaxInput);
	// UE_LOG(LogRunnerMovement, Log, TEXT("URunnerMovementComponent::CalcVelocity called with DeltaTime: %f, Friction: %f, bFluid: %d, BrakingDeceleration: %f, Accelaration: %s, OldVelocity: %s, OldSize %f, NewVelocity: %s, NewSize %f"), DeltaTime, Friction, bFluid, BrakingDeceleration, *Acceleration.ToString(), *OOldVelocity.ToString(), OOldVelocity.Size(), *Velocity.ToString(), Velocity.Size());
	// 速度设置为与滑板的前进的方向一致
	if (IsWalking() && SkateboardMesh)
	{
		auto NewVelocity = Velocity.Size() * GetOwner()->GetActorForwardVector();
		Velocity.X = NewVelocity.X;
		Velocity.Y = NewVelocity.Y;
		// Velocity.Z = NewVelocity.Z;
	}
}

float URunnerMovementComponent::GetMaxSpeed() const
{
	auto Ret = Thrusting > 0 ? MaxThrustSpeed : MaxWalkSpeed;
	auto Scale = SlowDown > 0 ? BadReduceSpeedScale : 1.0f;
	return Ret * Scale;
}

void URunnerMovementComponent::MoveAlongFloor(const FVector& InVelocity, float DeltaSeconds, FStepDownResult* OutStepDownResult)
{
	Super::MoveAlongFloor(InVelocity, DeltaSeconds, OutStepDownResult);
	return;
	// if (!CurrentFloor.IsWalkableFloor())
	// {
	// 	return;
	// }
	//
	// const FVector Delta = ProjectToGravityFloor(InVelocity) * DeltaSeconds;
	// FHitResult Hit(1.f);
	// FVector RampVector = ComputeGroundMovementDelta(Delta, CurrentFloor.HitResult, CurrentFloor.bLineTrace);
	// SafeMoveUpdatedComponent(RampVector, UpdatedComponent->GetComponentQuat(), true, Hit);
	// float LastMoveTimeSlice = DeltaSeconds;
	//
	// if (Hit.bStartPenetrating)
	// {
	// 	// TODO: 这代码该如何处理？我不希望 slide along surface，感觉角直接穿过去会更好
	//
	// 	// Allow this hit to be used as an impact we can deflect off, otherwise we do nothing the rest of the update and appear to hitch.
	// 	HandleImpact(Hit);
	// 	// SlideAlongSurface(Delta, 1.f, Hit.Normal, Hit, true);
	//
	// 	if (Hit.bStartPenetrating)
	// 	{
	// 		OnCharacterStuckInGeometry(&Hit);
	// 	}
	// }
	// else if (Hit.IsValidBlockingHit())
	// {
	// 	// We impacted something (most likely another ramp, but possibly a barrier).
	// 	float PercentTimeApplied = Hit.Time;
	// 	if ((Hit.Time > 0.f) && (GetGravitySpaceZ(Hit.Normal) > UE_KINDA_SMALL_NUMBER) && IsWalkable(Hit))
	// 	{
	// 		// Another walkable ramp.
	// 		const float InitialPercentRemaining = 1.f - PercentTimeApplied;
	// 		// 这里暂时先沿着地面走吧，既然都撞上东西了，应该这里不能飞起来
	// 		RampVector = ComputeGroundMovementDelta(Delta * InitialPercentRemaining, Hit, false);
	// 		LastMoveTimeSlice = InitialPercentRemaining * LastMoveTimeSlice;
	// 		SafeMoveUpdatedComponent(RampVector, UpdatedComponent->GetComponentQuat(), true, Hit);
	//
	// 		const float SecondHitPercent = Hit.Time * InitialPercentRemaining;
	// 		PercentTimeApplied = FMath::Clamp(PercentTimeApplied + SecondHitPercent, 0.f, 1.f);
	// 	}
	//
	// 	if (Hit.IsValidBlockingHit())
	// 	{
	// 		HandleImpact(Hit, LastMoveTimeSlice, RampVector);
	// 	}
	// }
}

void URunnerMovementComponent::FindFloor(const FVector& CapsuleLocation, FFindFloorResult& OutFloorResult, bool bCanUseCachedLocation, const FHitResult* DownwardSweepResult) const
{
	if (IsWalking())
	{
		auto* HitComp = OutFloorResult.HitResult.GetComponent();
		if (HitComp && HitComp->ComponentHasTag("Takeoff"))
		{
			FHitResult OutHit;
			GetWorld()->LineTraceSingleByChannel(OutHit, CapsuleLocation, CapsuleLocation - FVector(0, 0, TraceDistanceToFindFloor), ECC_GameTraceChannel1);
			if (!OutHit.IsValidBlockingHit() || (OutHit.GetComponent() != HitComp))
			{
				OutFloorResult.Clear();
				return;
			}
		}
		// FTransform OutInstanceTransform;
		// auto* ISM = Cast<UInstancedStaticMeshComponent>(HitComp);
		// if (ISM && ISM->GetInstanceTransform(OutFloorResult.HitResult.Item, OutInstanceTransform))
		// {
		// 	UE_LOG(LogRunnerMovement, Warning, TEXT("URunnerMovementComponent::FindFloor HitResult: CompName %s, item %d, Rotation: %s"), *HitComp->GetName(), OutFloorResult.HitResult.Item, *OutInstanceTransform.GetRotation().Rotator().ToString());
		// }
	}
	Super::FindFloor(CapsuleLocation, OutFloorResult, bCanUseCachedLocation, DownwardSweepResult);
}

void URunnerMovementComponent::AdjustFloorHeight()
{
	SCOPE_CYCLE_COUNTER(STAT_SkateboardAdjustFloorHeight);

	// If we have a floor check that hasn't hit anything, don't adjust height.
	if (!CurrentFloor.IsWalkableFloor())
	{
		return;
	}

	float OldFloorDist = CurrentFloor.FloorDist;
	if (CurrentFloor.bLineTrace)
	{
		if (OldFloorDist < MIN_FLOOR_DIST && CurrentFloor.LineDist >= MIN_FLOOR_DIST)
		{
			// This would cause us to scale unwalkable walls
			// UE_LOG(LogRunnerMovement, VeryVerbose, TEXT("Adjust floor height aborting due to line trace with small floor distance (line: %.2f, sweep: %.2f)"), CurrentFloor.LineDist, CurrentFloor.FloorDist);
			return;
		}
		else
		{
			// Falling back to a line trace means the sweep was unwalkable (or in penetration). Use the line distance for the vertical adjustment.
			OldFloorDist = CurrentFloor.LineDist;
		}
	}

	// Move up or down to maintain floor height.
	if (OldFloorDist < MIN_FLOOR_DIST || OldFloorDist > MAX_FLOOR_DIST)
	{
		FHitResult AdjustHit(1.f);
		const double InitialZ = GetGravitySpaceZ(UpdatedComponent->GetComponentLocation());
		const float AvgFloorDist = (MIN_FLOOR_DIST + MAX_FLOOR_DIST) * 0.5f;
		float MoveDist = AvgFloorDist - OldFloorDist;

		// 新增逻辑, 不允许在起跳板上向下跑
		if (CurrentFloor.HitResult.IsValidBlockingHit())
		{
			auto HitComponent = CurrentFloor.HitResult.GetComponent();
			if (HitComponent && HitComponent->ComponentHasTag("Takeoff"))
			{
				MoveDist = FMath::Max(MoveDist, 0.0f);
			}
		}

		SafeMoveUpdatedComponent(MoveDist * -GetGravityDirection(), UpdatedComponent->GetComponentQuat(), true, AdjustHit);
		// UE_LOG(LogRunnerMovement, VeryVerbose, TEXT("Adjust floor height %.3f (Hit = %d)"), MoveDist, AdjustHit.bBlockingHit);

		if (!AdjustHit.IsValidBlockingHit())
		{
			CurrentFloor.FloorDist += MoveDist;
		}
		else if (MoveDist > 0.f)
		{
			const double CurrentZ = GetGravitySpaceZ(UpdatedComponent->GetComponentLocation());
			CurrentFloor.FloorDist += CurrentZ - InitialZ;
		}
		else
		{
			checkSlow(MoveDist < 0.f);
			CurrentFloor.FloorDist = GetGravitySpaceZ(UpdatedComponent->GetComponentLocation() - AdjustHit.Location);
			if (IsWalkable(AdjustHit))
			{
				CurrentFloor.SetFromSweep(AdjustHit, CurrentFloor.FloorDist, true);
			}
		}

		// Don't recalculate velocity based on this height adjustment, if considering vertical adjustments.
		// Also avoid it if we moved out of penetration
		bJustTeleported |= !bMaintainHorizontalGroundVelocity || (OldFloorDist < 0.f);

		// If something caused us to adjust our height (especially a depentration) we should ensure another check next frame or we will keep a stale result.
		if (CharacterOwner && CharacterOwner->GetLocalRole() != ROLE_SimulatedProxy)
		{
			bForceNextFloorCheck = true;
		}
	}
}

void URunnerMovementComponent::PhysWalking(float deltaTime, int32 Iterations)
{
	Super::PhysWalking(deltaTime, Iterations);
	// PhysWalking2(deltaTime, Iterations);
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

void URunnerMovementComponent::PhysFalling(float deltaTime, int32 Iterations)
{
	Super::PhysFalling(deltaTime, Iterations);
	// PhysFalling2(deltaTime, Iterations);
}

void URunnerMovementComponent::PhysFalling2(float deltaTime, int32 Iterations)
{
	SCOPE_CYCLE_COUNTER(STAT_SkateboardPhysFalling);

	if (deltaTime < MIN_TICK_TIME)
	{
		return;
	}

	const FVector FallAcceleration = ProjectToGravityFloor(GetFallingLateralAcceleration(deltaTime));
	const bool bHasLimitedAirControl = ShouldLimitAirControl(deltaTime, FallAcceleration);

	float remainingTime = deltaTime;
	while ((remainingTime >= MIN_TICK_TIME) && (Iterations < MaxSimulationIterations))
	{
		Iterations++;
		float timeTick = GetSimulationTimeStep(remainingTime, Iterations);
		remainingTime -= timeTick;

		const FVector OldLocation = UpdatedComponent->GetComponentLocation();
		const FQuat PawnRotation = UpdatedComponent->GetComponentQuat();
		bJustTeleported = false;

		const FVector OldVelocityWithRootMotion = Velocity;

		RestorePreAdditiveRootMotionVelocity();

		const FVector OldVelocity = Velocity;

		// Apply input
		const float MaxDecel = GetMaxBrakingDeceleration();
		if (!HasAnimRootMotion() && !CurrentRootMotion.HasOverrideVelocity())
		{
			// Compute Velocity
			{
				// Acceleration = FallAcceleration for CalcVelocity(), but we restore it after using it.
				TGuardValue<FVector> RestoreAcceleration(Acceleration, FallAcceleration);
				if (HasCustomGravity())
				{
					Velocity = ProjectToGravityFloor(Velocity);
					const FVector GravityRelativeOffset = OldVelocity - Velocity;
					CalcVelocity(timeTick, FallingLateralFriction, false, MaxDecel);
					Velocity += GravityRelativeOffset;
				}
				else
				{
					Velocity.Z = 0.f;
					CalcVelocity(timeTick, FallingLateralFriction, false, MaxDecel);
					Velocity.Z = OldVelocity.Z;
				}
			}
		}

		// Compute current gravity
		const FVector Gravity = -GetGravityDirection() * GetGravityZ();
		float GravityTime = timeTick;

		// If jump is providing force, gravity may be affected.
		bool bEndingJumpForce = false;
		if (CharacterOwner->JumpForceTimeRemaining > 0.0f)
		{
			// Consume some of the force time. Only the remaining time (if any) is affected by gravity when bApplyGravityWhileJumping=false.
			const float JumpForceTime = FMath::Min(CharacterOwner->JumpForceTimeRemaining, timeTick);
			GravityTime = bApplyGravityWhileJumping ? timeTick : FMath::Max(0.0f, timeTick - JumpForceTime);

			// Update Character state
			CharacterOwner->JumpForceTimeRemaining -= JumpForceTime;
			if (CharacterOwner->JumpForceTimeRemaining <= 0.0f)
			{
				CharacterOwner->ResetJumpState();
				bEndingJumpForce = true;
			}
		}

		// Apply gravity
		Velocity = NewFallVelocity(Velocity, Gravity, GravityTime);

		// UE_LOG(LogCharacterMovement, Log, TEXT("dt=(%.6f) OldLocation=(%s) OldVelocity=(%s) OldVelocityWithRootMotion=(%s) NewVelocity=(%s)"), timeTick, *(UpdatedComponent->GetComponentLocation()).ToString(), *OldVelocity.ToString(), *OldVelocityWithRootMotion.ToString(), *Velocity.ToString());
		ApplyRootMotionToVelocity(timeTick);
		DecayFormerBaseVelocity(timeTick);

		// See if we need to sub-step to exactly reach the apex. This is important for avoiding "cutting off the top" of the trajectory as framerate varies.
		const FVector::FReal GravityRelativeOldVelocityWithRootMotionZ = GetGravitySpaceZ(OldVelocityWithRootMotion);
		if (GravityRelativeOldVelocityWithRootMotionZ > 0.f && GetGravitySpaceZ(Velocity) <= 0.f && NumJumpApexAttempts < MaxJumpApexAttemptsPerSimulation)
		{
			const FVector DerivedAccel = (Velocity - OldVelocityWithRootMotion) / timeTick;
			const FVector::FReal GravityRelativeDerivedAccelZ = GetGravitySpaceZ(DerivedAccel);
			if (!FMath::IsNearlyZero(GravityRelativeDerivedAccelZ))
			{
				const float TimeToApex = -GravityRelativeOldVelocityWithRootMotionZ / GravityRelativeDerivedAccelZ;

				// The time-to-apex calculation should be precise, and we want to avoid adding a substep when we are basically already at the apex from the previous iteration's work.
				const float ApexTimeMinimum = 0.0001f;
				if (TimeToApex >= ApexTimeMinimum && TimeToApex < timeTick)
				{
					const FVector ApexVelocity = OldVelocityWithRootMotion + (DerivedAccel * TimeToApex);
					if (HasCustomGravity())
					{
						Velocity = ProjectToGravityFloor(ApexVelocity); // Should be nearly zero anyway, but this makes apex notifications consistent.
					}
					else
					{
						Velocity = ApexVelocity;
						Velocity.Z = 0.f; // Should be nearly zero anyway, but this makes apex notifications consistent.
					}

					// We only want to move the amount of time it takes to reach the apex, and refund the unused time for next iteration.
					const float TimeToRefund = (timeTick - TimeToApex);

					remainingTime += TimeToRefund;
					timeTick = TimeToApex;
					Iterations--;
					NumJumpApexAttempts++;

					// Refund time to any active Root Motion Sources as well
					for (TSharedPtr<FRootMotionSource> RootMotionSource : CurrentRootMotion.RootMotionSources)
					{
						const float RewoundRMSTime = FMath::Max(0.0f, RootMotionSource->GetTime() - TimeToRefund);
						RootMotionSource->SetTime(RewoundRMSTime);
					}
				}
			}
		}

		if (bNotifyApex && (GetGravitySpaceZ(Velocity) < 0.f))
		{
			// Just passed jump apex since now going down
			bNotifyApex = false;
			NotifyJumpApex();
		}

		// Compute change in position (using midpoint integration method).
		FVector Adjusted = 0.5f * (OldVelocityWithRootMotion + Velocity) * timeTick;

		// Special handling if ending the jump force where we didn't apply gravity during the jump.
		if (bEndingJumpForce && !bApplyGravityWhileJumping)
		{
			// We had a portion of the time at constant speed then a portion with acceleration due to gravity.
			// Account for that here with a more correct change in position.
			const float NonGravityTime = FMath::Max(0.f, timeTick - GravityTime);
			Adjusted = (OldVelocityWithRootMotion * NonGravityTime) + (0.5f * (OldVelocityWithRootMotion + Velocity) * GravityTime);
		}

		// Move
		FHitResult Hit(1.f);
		SafeMoveUpdatedComponent(Adjusted, PawnRotation, true, Hit);

		if (!HasValidData())
		{
			return;
		}

		float LastMoveTimeSlice = timeTick;
		float subTimeTickRemaining = timeTick * (1.f - Hit.Time);

		if (Hit.bBlockingHit)
		{
			if (IsValidLandingSpot(UpdatedComponent->GetComponentLocation(), Hit))
			{
				remainingTime += subTimeTickRemaining;
				ProcessLanded(Hit, remainingTime, Iterations);
				return;
			}
			else
			{
				// Compute impact deflection based on final velocity, not integration step.
				// This allows us to compute a new velocity from the deflected vector, and ensures the full gravity effect is included in the slide result.
				Adjusted = Velocity * timeTick;

				// See if we can convert a normally invalid landing spot (based on the hit result) to a usable one.
				if (!Hit.bStartPenetrating && ShouldCheckForValidLandingSpot(timeTick, Adjusted, Hit))
				{
					const FVector PawnLocation = UpdatedComponent->GetComponentLocation();
					FFindFloorResult FloorResult;
					FindFloor(PawnLocation, FloorResult, false);

					// Note that we only care about capsule sweep floor results, since the line trace may detect a lower walkable surface that our falling capsule wouldn't actually reach yet.
					if (!FloorResult.bLineTrace && FloorResult.IsWalkableFloor() && IsValidLandingSpot(PawnLocation, FloorResult.HitResult))
					{
						remainingTime += subTimeTickRemaining;
						ProcessLanded(FloorResult.HitResult, remainingTime, Iterations);
						return;
					}
				}

				HandleImpact(Hit, LastMoveTimeSlice, Adjusted);

				// If we've changed physics mode, abort.
				if (!HasValidData() || !IsFalling())
				{
					return;
				}
				remainingTime += subTimeTickRemaining;
			}
		}

		const FVector GravityProjectedVelocity = ProjectToGravityFloor(Velocity);
		if (GravityProjectedVelocity.SizeSquared() <= UE_KINDA_SMALL_NUMBER * 10.f)
		{
			Velocity = GetGravitySpaceComponentZ(Velocity);
		}
	}
}

// void URunnerMovementComponent::StartThrust()
// {
// 	// DefaultMaxWalkingSpeed = MaxWalkSpeed;
// 	// MaxWalkSpeed = MaxThrustSpeed;
// 	Thrusting++;
// 	// TODO：要不要骤变一下速度？
// }

// void URunnerMovementComponent::StopThrust()
// {
// 	// MaxWalkSpeed = DefaultMaxWalkingSpeed;
// 	Thrusting--;
// 	ensure(Thrusting >= 0);
// }

void URunnerMovementComponent::StartDown()
{
	if (IsFalling() && !bDown)
	{
		bDown = true;
		Velocity.Z = FMath::Max(Velocity.Z - DownReduceZVelocity, MaxZVelocityInAir);
	}
	Cast<ARunnerCharacter>(GetOwner())->StopActiveMontage(0.2f);
}

void URunnerMovementComponent::StopDown()
{
	if (bDown)
	{
		bDown = false;
		// TODO：要不要恢复速度？
		// Velocity.Z = FMath::Max(Velocity.Z + DownReduceZVelocity, MaxZVelocityInAir);
	}
}
