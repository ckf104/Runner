// Fill out your copyright notice in the Description page of Project Settings.

#include "CoinActor.h"
#include "Components/SceneComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Character.h"
#include "Kismet/GameplayStatics.h"
#include "Runner/RunnerCharacter.h"

// Sets default values
ACoinActor::ACoinActor()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
	StaticMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("StaticMesh"));
	RootComponent = StaticMesh;
	Collision = CreateDefaultSubobject<USphereComponent>(TEXT("Collision"));
	Collision->SetupAttachment(RootComponent);
	Collision->SetCollisionProfileName(TEXT("Coin"));
}

// Called when the game starts or when spawned
void ACoinActor::BeginPlay()
{
	Super::BeginPlay();
	Collision->OnComponentBeginOverlap.AddDynamic(this, &ACoinActor::DealOverlap);
}

// Called every frame
void ACoinActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	if (bAttracted)
	{
		CurrentAttractTime += DeltaTime;
		auto* Player = Cast<ARunnerCharacter>(UGameplayStatics::GetPlayerCharacter(this, 0));
		if (Player)
		{
			auto AbsorbPos = Player->AbsorbComp->GetComponentLocation();
			auto CurrentPos = GetActorLocation();
			auto alpha = FMath::Clamp(CurrentAttractTime / AttractTime, 0.0f, 1.0f);
			auto NewPos = FMath::Lerp(CurrentPos, AbsorbPos, alpha);
			SetActorLocation(NewPos);
		}
	}
}

void ACoinActor::DealOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (OtherComp)
	{
		if (OtherComp->ComponentHasTag("Activate"))
		{
			bAttracted = true;
		}
		else if (OtherComp->ComponentHasTag("Absorb"))
		{
			Destroy();
		}
	}
}
