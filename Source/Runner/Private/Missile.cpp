// Fill out your copyright notice in the Description page of Project Settings.

#include "Missile.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/DecalComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "Particles/ParticleSystemComponent.h"

// Sets default values
AMissile::AMissile()
{
	PrimaryActorTick.bCanEverTick = true;

	StaticMeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("StaticMeshComponent"));
	RootComponent = StaticMeshComp;

	// 使用 projectile 不好控制时间
	// ProjectileMovementComp = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("ProjectileMovementComponent"));
	// ProjectileMovementComp->SetUpdatedComponent(RootComponent);

	TrailComp = CreateDefaultSubobject<UNiagaraComponent>(TEXT("Trail"));
	TrailComp->SetupAttachment(RootComponent);

	DecalComp = CreateDefaultSubobject<UDecalComponent>(TEXT("DecalComponent"));
	DecalComp->SetupAttachment(RootComponent);

	SphereComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SphereComponent"));
	SphereComp->SetupAttachment(RootComponent);
	SphereComp->SetHiddenInGame(true);
}

void AMissile::PostInitProperties()
{
	Super::PostInitProperties();
}

// Called when the game starts or when spawned
void AMissile::BeginPlay()
{
	Super::BeginPlay();

	StartPos = GetActorLocation();
	DecalComp->SetUsingAbsoluteLocation(true);
	DecalComp->SetUsingAbsoluteRotation(true);
	DecalComp->SetWorldLocationAndRotation(TargetPos, FRotator(-90.0, 0.0, 0.0));
	SphereComp->CreateDynamicMaterialInstance(0);
}

void AMissile::SetTargetPos(FVector Target)
{
	TargetPos = Target;
	DecalComp->SetWorldLocation(TargetPos);
}

void AMissile::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	auto OldTime = CurrentTime;
	CurrentTime += DeltaTime;
	if (CurrentTime < ReachTime)
	{
		auto LerpAlpha = FMath::Clamp(CurrentTime / ReachTime, 0.0f, 1.0f);
		FVector NewPos = FMath::Lerp(StartPos, TargetPos, LerpAlpha);
		SetActorLocation(NewPos);
	}
	else if (OldTime < ReachTime)
	{
		// 导弹到达打击点
		SetActorLocation(TargetPos);
		StaticMeshComp->SetVisibility(false);
		TrailComp->Deactivate();
		SphereComp->SetHiddenInGame(false);

		APlayerCameraManager::PlayWorldCameraShake(GetWorld(), CameraShakeClass, TargetPos, CameraShakeInnerRadius, CameraShakeOuterRadius, CameraShakeFalloff);
	}

	if (CurrentTime >= ReachTime && CurrentTime < ReachTime + BombTime)
	{
		SphereComp->SetWorldScale3D(FVector(FMath::Lerp(SphereStartScale, SphereEndScale, (CurrentTime - ReachTime) / BombTime)));
	}
	else if (CurrentTime >= ReachTime + BombTime && CurrentTime < ReachTime + BombTime + DisappearTime)
	{
		auto Opacity = 1 - FMath::Clamp((CurrentTime - ReachTime - BombTime) / DisappearTime, 0.0f, 1.0f);
		Cast<UMaterialInstanceDynamic>(SphereComp->GetMaterial(0))->SetScalarParameterValue("Opacity", Opacity);
	}
	else if (CurrentTime >= ReachTime + BombTime + DisappearTime)
	{
		Destroy();
	}

	// if (bDestorySelf)
	// {
	// 	FFXSystemSpawnParameters SpawnParams;
	// 	SpawnParams.WorldContextObject = this;
	// 	SpawnParams.SystemTemplate = BombEffect;
	// 	SpawnParams.Location = TargetPos;
	// 	SpawnParams.Rotation = FRotator::ZeroRotator;
	// 	SpawnParams.Scale = FVector(NiagraBombScale);
	// 	SpawnParams.bAutoActivate = true;
	// 	SpawnParams.bAutoDestroy = true;
	// 	SpawnParams.PoolingMethod = EPSCPoolMethod::AutoRelease;
	// 	SpawnParams.bPreCullCheck = false;
	// 	UNiagaraFunctionLibrary::SpawnSystemAtLocationWithParams(SpawnParams);
	// 	Destroy();
	// }
}
