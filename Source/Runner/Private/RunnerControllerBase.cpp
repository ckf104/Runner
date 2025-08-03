// Fill out your copyright notice in the Description page of Project Settings.


#include "RunnerControllerBase.h"
#include "Engine/TimerHandle.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "Math/MathFwd.h"
#include "Runner/RunnerCharacter.h"
#include "RunnerMovementComponent.h"
#include "TimerManager.h"
#include "WorldGenerator.h"


FRotator ARunnerControllerBase::GetControlRotation() const
{
  auto Ret = Super::GetControlRotation();
  // Override to return a custom control rotation if needed
  // if (auto* OwnedPawn = GetPawn())
  // {
  //   auto Yaw = OwnedPawn->GetActorRotation().Yaw;
  //   Ret.Yaw = Yaw;
  // }
  return Ret;
}

void ARunnerControllerBase::WhenCountDownOver()
{
  Cast<ARunnerCharacter>(GetPawn())->StartThrust(EThrustSource::FreeThrust);
  FTimerHandle Handler;
  GetWorld()->GetTimerManager().SetTimer(Handler, [this]() {
    Cast<ARunnerCharacter>(GetPawn())->StopThrust(EThrustSource::FreeThrust);
  }, StartThrustTime, false);

  Cast<URunnerMovementComponent>(Cast<ARunnerCharacter>(GetPawn())->GetCharacterMovement())->SetGameStart();
  
  TActorIterator<AWorldGenerator> It(GetWorld());
  if (It)
  {
    auto* WorldGenerator = *It;
    WorldGenerator->SetGameStart();
  }
}