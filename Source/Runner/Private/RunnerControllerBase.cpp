// Fill out your copyright notice in the Description page of Project Settings.


#include "RunnerControllerBase.h"
#include "GameFramework/Pawn.h"
#include "Math/MathFwd.h"


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
