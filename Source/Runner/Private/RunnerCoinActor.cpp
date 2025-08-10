// Fill out your copyright notice in the Description page of Project Settings.


#include "RunnerCoinActor.h"
#include "Components/SphereComponent.h"

ARunnerCoinActor::ARunnerCoinActor()
{
  Collision->ComponentTags.Add("GoldCoin");
}


void ARunnerCoinActor::Tick(float DeltaTime)
{
  Super::Tick(DeltaTime);
  auto Rot = FRotator(0, 300 * DeltaTime, 0);
  AddActorLocalRotation(Rot);
}



