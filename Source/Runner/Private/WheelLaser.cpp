// Fill out your copyright notice in the Description page of Project Settings.

#include "WheelLaser.h"
#include "Components/StaticMeshComponent.h"
#include "Math/MathFwd.h"
#include "UObject/UObjectGlobals.h"

// Sets default values
AWheelLaser::AWheelLaser()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("RootComponent"));
	CenterSphere = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CenterSphere"));
	CenterSphere->SetupAttachment(RootComponent);
}

// Called when the game starts or when spawned
void AWheelLaser::BeginPlay()
{
	Super::BeginPlay();
}

// Called every frame
void AWheelLaser::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	auto Angle = RotationSpeed * DeltaTime;
	FRotator NewRotation = FRotator(0.0, 0.0, Angle);
	RootComponent->AddRelativeRotation(NewRotation);
}

void AWheelLaser::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	auto StepAngle = 360.0 / LaserCount;
	for (int i = 0; i < LaserCount; ++i)
	{
		auto* LaserMeshComp = NewObject<UStaticMeshComponent>(this, UStaticMeshComponent::StaticClass(), *FString::Printf(TEXT("Laser_%d"), i));
		LaserMeshComp->SetStaticMesh(LaserMesh);
		LaserMeshComp->SetMaterial(0, LaserMaterial);
		LaserMeshComp->SetCollisionProfileName("Barrier");
		LaserMeshComp->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepRelativeTransform);
		LaserMeshComp->RegisterComponent();

		auto Angle = StepAngle * i;
		auto Radians = FMath::DegreesToRadians(Angle);
		auto Y = FMath::Sin(Radians) * Radius;
		auto Z = FMath::Cos(Radians) * Radius;
		auto RelativeLocation = FVector(0.0, Y, Z);

		auto AdditiveRotation = FRotator(0.0, 0.0, Angle);

		auto Rotator = AdditiveRotation.Quaternion() * InitialRotation.Quaternion();
		auto WorldLocation = Transform.TransformPosition(RelativeLocation);
		LaserMeshComp->SetWorldLocationAndRotation(WorldLocation, Rotator);
		LaserMeshComp->SetWorldScale3D(LaserScale);
	}
}