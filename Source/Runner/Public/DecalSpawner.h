// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BarrierSpawner.h"
#include "DecalSpawner.generated.h"

/**
 * 
 */
UCLASS()
class RUNNER_API ADecalSpawner : public ABarrierSpawner
{
	GENERATED_BODY()
	
public:	
	ADecalSpawner();

	UPROPERTY(EditAnywhere, Category = "Decal")
	TObjectPtr<class UMaterialInterface> DecalMaterial;

	UPROPERTY(EditAnywhere, Category = "Decal")
	float MinDecalScale = 0.5f;

	UPROPERTY(EditAnywhere, Category = "Decal")
	float MaxDecalScale = 1.5f;

	// UPROPERTY(EditAnywhere, Category = "Decal")
	// float DecalXSize = 512.0f;
	
	UPROPERTY(EditAnywhere, Category = "Decal")
	float SphereScale = 0.75f;

	UPROPERTY(EditAnywhere, Category = "Debug")
	bool bShowSphere = false;

	void SpawnBarriers(TArrayView<RandomPoint> Positions, FInt32Point Tile, AWorldGenerator* WorldGenerator) override;
	void RemoveTile(FInt32Point Tile) override;

	FRotator GetRotationFromSeed(FRotator Seed) const override;

private:
	TMap<FInt32Point, TArray<class UDecalComponent*>> SpawnedDecals; // 存储生成的 Decal 实例
	TArray<class UDecalComponent*> CachedDecals; // 用于缓存已生成的 Decal 实例
};
