// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Missile.generated.h"

UCLASS()
class RUNNER_API AMissile : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	AMissile();

	void PostInitProperties() override;

	// 导弹到达打击点的时间
	UPROPERTY(EditAnywhere, Category = "Missile")
	float ReachTime = 1.5f;

	UPROPERTY(EditAnywhere, Category = "Missile")
	float SphereStartScale = 1.0f;

	UPROPERTY(EditAnywhere, Category = "Missile")
	float SphereEndScale = 10.0f;

	UPROPERTY(EditAnywhere, Category = "Missile")
	float BombTime = 0.5f;

	UPROPERTY(EditAnywhere, Category = "Missile")
	float DisappearTime = 1.0f;

	UPROPERTY(EditAnywhere, Category = "Missile")
	float CameraShakeInnerRadius = 1000.0f;

	UPROPERTY(EditAnywhere, Category = "Missile")
	float CameraShakeOuterRadius = 2000.0f;

	UPROPERTY(EditAnywhere, Category = "Missile")
	float CameraShakeFalloff = 1.0f;

	UPROPERTY(EditAnywhere, Category = "Missile")
	TSubclassOf<class UCameraShakeBase> CameraShakeClass;

	// 导弹频繁发射的情况下每次爆炸都发出声音有点噪音污染了
	// UPROPERTY(EditAnywhere, Category = "Missile")
	// TObjectPtr<class USoundBase> MissileSound;

private:
	float CurrentTime = 0.0f;

	FVector StartPos;
	FVector TargetPos;

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;
	

public:	
	void Tick(float DeltaTime) override;
	UFUNCTION(BlueprintCallable, Category = "Missile")
	
	void SetTargetPos(FVector Target); 

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TObjectPtr<class UStaticMeshComponent> StaticMeshComp;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TObjectPtr<class UNiagaraComponent> TrailComp;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TObjectPtr<class UDecalComponent> DecalComp;

	UPROPERTY(EditAnywhere, Category = "Missile")
	TObjectPtr<class UStaticMeshComponent> SphereComp;
	
	// UPROPERTY(EditAnywhere)
	// TObjectPtr<class UProjectileMovementComponent> ProjectileMovementComp;

	
};
