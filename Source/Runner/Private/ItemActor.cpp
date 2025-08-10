// Fill out your copyright notice in the Description page of Project Settings.

#include "ItemActor.h"

void AItemActor::BeginPlay()
{
	Super::BeginPlay();
	InitialZ = GetActorLocation().Z;
	CurrentTime = 0.0f;
}

void AItemActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	// 道具上下晃动会影响收集
	if (!bAttracted)
	{
		CurrentTime += DeltaTime;
		float NewZ = InitialZ + FMath::Sin(CurrentTime * Frequency) * Amplitude;
		SetActorLocation(FVector(GetActorLocation().X, GetActorLocation().Y, NewZ));
	}
}