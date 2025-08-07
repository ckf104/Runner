// Fill out your copyright notice in the Description page of Project Settings.


#include "MissileComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "Kismet/GameplayStatics.h"
#include "Math/UnrealMathUtility.h"
#include "Missile.h"
#include "WorldGenerator.h"


// Sets default values for this component's properties
UMissileComponent::UMissileComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;

	// ...
}


// Called when the game starts
void UMissileComponent::BeginPlay()
{
	Super::BeginPlay();
	SetComponentTickInterval(TickInterval);
}

FVector UMissileComponent::PredictPlayerPos(FVector PlayerPos)
{
	auto Speed = TotalDistance / TotalTime;
	auto CenterPos = FVector2D(PlayerPos.X + Speed * MissileReachTime, PlayerPos.Y);

	auto Angle = FMath::RandRange(0.0, 360.0); // 随机角度
	auto Rand = FMath::RandRange(0.0, 1.0);
	auto PowRand = FMath::Pow(Rand, double(Tension)); // 根据 Tension 调整随机性

	auto Radius = RandomRadius * PowRand;

	auto OffsetX = Radius * FMath::Cos(FMath::DegreesToRadians(Angle));
	auto OffsetY = Radius * FMath::Sin(FMath::DegreesToRadians(Angle));

	auto PredictPos = FVector2D(CenterPos.X + OffsetX, CenterPos.Y + OffsetY);

	auto* WorldGenerator = Cast<AWorldGenerator>(GetOwner());

	auto ClampedPos = WorldGenerator->ClampToWorld(PredictPos, MissileRadius);

	auto Height = WorldGenerator->GetHeightFromHorizontalPos(ClampedPos);

	return FVector(PredictPos.X, PredictPos.Y, Height);
}

FVector UMissileComponent::RandomMissileStartPos(FVector TargetPos)
{
	auto RandAngle = FMath::RandRange(0.0, 180.0);
	auto RandRadius = FMath::RandRange(MissileMinStartOffset, MissileMaxStartOffset);
	auto OffsetX = RandRadius * FMath::Cos(FMath::DegreesToRadians(RandAngle));
	auto OffsetY = RandRadius * FMath::Sin(FMath::DegreesToRadians(RandAngle));
	return FVector(TargetPos.X + OffsetX, TargetPos.Y + OffsetY, TargetPos.Z + 4000.0);
}

// Called every frame
void UMissileComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	auto* Character = UGameplayStatics::GetPlayerCharacter(this, 0);		
	if (Character == nullptr)
	{
		return;
	}
	auto CurrentPos = Character->GetActorLocation().X;

	auto* WorldGenerator = Cast<AWorldGenerator>(GetOwner());
	auto CurrentDifficulty = WorldGenerator->CurrentDifficulty;
	auto DistanceDifficulty = FMath::Clamp(CurrentDifficulty, 0, ExpectedDistance.Num() - 1);
	
	// 没有移动或者当前的难度不会触发导弹
	if (CurrentPos == LastPlayerPos || ExpectedDistance[DistanceDifficulty] <= 0.0f)
	{
		return;
	}

	TotalDistance += FMath::Abs(CurrentPos - LastPlayerPos);
	TotalTime += DeltaTime;

	double lambda = 1.0 / ExpectedDistance[DistanceDifficulty];
	auto Probability = FMath::Exp(-lambda * TotalDistance);

	auto Rand = FMath::FRandRange(0.0, 1.0);
	if (Rand > Probability || bDebug)
	{
		// Trigger missile launch
		auto PredictPos = PredictPlayerPos(Character->GetActorLocation());
		auto StartPos = RandomMissileStartPos(PredictPos);	

		auto ZAxis = (PredictPos - StartPos).GetSafeNormal();
		auto Rotation = FRotationMatrix::MakeFromZ(ZAxis).Rotator();

		FActorSpawnParameters SpawnParams;
		SpawnParams.bDeferConstruction = true;
		auto* Missile = GetWorld()->SpawnActor<AMissile>(MissileClass, StartPos, Rotation, SpawnParams);
		Missile->SetTargetPos(PredictPos);
		Missile->FinishSpawning(FTransform(Rotation, StartPos));

		TotalDistance = 0.0;
		TotalTime = 0.0;
		LastPlayerPos = Character->GetActorLocation().X;
	}
}

