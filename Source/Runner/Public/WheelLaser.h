// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WheelLaser.generated.h"

UCLASS()
class RUNNER_API AWheelLaser : public AActor
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	AWheelLaser();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:
	UPROPERTY(EditAnywhere, Category = "Laser")
	int32 LaserCount = 8;

	UPROPERTY(EditAnywhere, Category = "Laser")
	int32 AngleType;

	UPROPERTY(EditAnywhere, Category = "Laser")
	float RotationSpeed = 60.0f;

	UPROPERTY(EditAnywhere, Category = "Laser")
	float Radius = 100.0f;

	UPROPERTY(EditAnywhere, Category = "Laser")
	TObjectPtr<class UStaticMesh> LaserMesh;

	UPROPERTY(EditAnywhere, Category = "Laser")
	TObjectPtr<class UMaterialInterface> LaserMaterial;

	UPROPERTY(EditAnywhere, Category = "Laser")
	TObjectPtr<class UStaticMeshComponent> CenterSphere;

	UPROPERTY(EditAnywhere, Category = "Laser")
	FVector LaserScale;

	UPROPERTY(EditAnywhere, Category = "Laser")
	FRotator InitialRotation = FRotator::ZeroRotator;

	// Called every frame
	virtual void Tick(float DeltaTime) override;

	void OnConstruction(const FTransform& Transform) override;
};
