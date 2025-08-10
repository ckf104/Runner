// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/HitResult.h"
#include "GameFramework/Actor.h"
#include "Math/MathFwd.h"
#include "WorldGenerator.h"
#include "BarrierSpawner.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogBarrierSpawner, Log, All);

UENUM()
enum class EAlignMode : uint8
{
	None,
	AlignNormal,	// 对齐到地面法线
	AlignGravity, // 对齐到重力方向
};

UCLASS(Abstract)
class RUNNER_API ABarrierSpawner : public AActor
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	ABarrierSpawner();

	// 这些变量仅允许在编辑器中修改，不能在运行时修改，并且可能被其它线程访问
	// 但 UE 不支持把这些成员变量标记为 const
	UPROPERTY(EditAnywhere, Category = "Barrier Spawner")
	EAlignMode AlignMode = EAlignMode::AlignGravity; // 对齐模式

	// 生成的障碍物所在的组编号，采样时不同组的障碍物相互不影响
	UPROPERTY(EditAnywhere, Category = "Barrier Spawner")
	int32 BarrierGroup = 0;

	UPROPERTY(EditAnywhere, Category = "Barrier Spawner")
	float PoissonDistance = 1200.0f; // 泊松采样的距离

	// 障碍物的偏移量，使得障碍物能陷到地里去
	UPROPERTY(EditAnywhere, Category = "Barrier Spawner")
	float BarrierOffset = 0.0f;

	UPROPERTY(EditAnywhere, Category = "Barrier Spawner")
	float BarrierRadius = 10.0f; // 障碍物半径

	UPROPERTY(EditAnywhere, Category = "Barrier Spawner")
	TArray<int32> MinBarrierCount; // 最小障碍物数量

	UPROPERTY(EditAnywhere, Category = "Barrier Spawner")
	TArray<int32> MaxBarrierCount; // 最大障碍物数量

	UPROPERTY(EditAnywhere, Category = "Barrier Spawner")
	bool bDeferSpawn = false; // 是否延迟 spawn，默认不延迟

protected:
	void TransformAlign(FVector& Location, FRotator& Rotation, AWorldGenerator* WorldGenerator) const;
	void GetTransformFromSeed(FTransform& OutTransform, const RandomPoint& Seed, FInt32Point Tile, AWorldGenerator* WorldGenerator) const;

	virtual FRotator GetRotationFromSeed(FRotator Seed) const;

public:
	bool CanSpawnThisBarrier(FInt32Point Tile, FVector2D UVPos, AWorldGenerator* WorldGenerator) const;

	virtual void SpawnBarriers(TArrayView<RandomPoint> Positions, FInt32Point Tile, AWorldGenerator* WorldGenerator) {}
	virtual FVector2D PreSpawnBarriers(TArrayView<RandomPoint> Positions, FInt32Point Tile, AWorldGenerator* WorldGenerator) { return FVector2D::ZeroVector; }
	virtual bool DeferSpawnBarriers(TArrayView<RandomPoint> Positions, FInt32Point Tile, FVector2D UVPos, AWorldGenerator* WorldGenerator) { return true; }
	virtual void RemoveTile(FInt32Point Tile) {}
	virtual int32 GetBarrierCountAnyThread(double RandomValue, int32 Difficulty) const
	{
		ensure(MinBarrierCount.Num() > 0 && MaxBarrierCount.Num() > 0);
		auto MinCount = MinBarrierCount.IsValidIndex(Difficulty) ? MinBarrierCount[Difficulty] : MinBarrierCount[MinBarrierCount.Num() - 1];
		auto MaxCount = MaxBarrierCount.IsValidIndex(Difficulty) ? MaxBarrierCount[Difficulty] : MaxBarrierCount[MaxBarrierCount.Num() - 1];

		if (MaxCount <= 0)
		{
			return 0;
		}
		auto BarCount = FMath::RoundToInt(float(FMath::Lerp(MinCount, MaxCount, RandomValue)));
		// 当 MaxCount 大于 0 时，确保至少生成一个障碍物
		return FMath::Max(BarCount, 1);
	}
	virtual void MoveWorldOrigin(int32 TileXOffset, double WorldOffsetX) {}

	virtual bool BarrierHasCustomSlope() const { return false; }
	virtual double GetCustomSlopeAngle(int32 InstanceIndex) const { return 0.0; }
};
