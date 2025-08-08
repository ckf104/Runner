// Fill out your copyright notice in the Description page of Project Settings.

#include "ISMBridgeSpawner.h"
#include "Math/MathFwd.h"
#include "Templates/UnrealTemplate.h"

AISMBridgeSpawner::AISMBridgeSpawner()
{
	AlignMode = EAlignMode::AlignNormal; // 默认对齐模式为 AlignNormal
}

FRotator AISMBridgeSpawner::GetRotationFromSeed(FRotator Seed) const
{
	FRotator Rotation = FRotator::ZeroRotator;
	Rotation.Pitch = FMath::Lerp(MinBridgeAngle, MaxBridgeAngle, Seed.Pitch);
	return Rotation;
}

void AISMBridgeSpawner::SpawnBarriers(TArrayView<RandomPoint> Positions, FInt32Point Tile, AWorldGenerator* WorldGenerator)
{
	if (!ISMComponent || !WorldGenerator)
	{
		return;
	}

	TArray<int32> InstanceIndices;
	InstanceIndices.Reserve(Positions.Num());
	for (auto& Point : Positions)
	{
		if (!CanSpawnThisBarrier(Tile, Point.UVPos, WorldGenerator))
		{
			continue; // 跳过不允许生成障碍物的区域
		}

		FTransform Transform;
		GetTransformFromSeed(Transform, Point, Tile, WorldGenerator);
		auto Rotator = Transform.GetRotation().Rotator();
		Rotator.Pitch = FMath::Min(Rotator.Pitch, MaxBridgeAngle);
		Transform.SetRotation(Rotator.Quaternion());

		if (ReplaceInstanceIndices.Num() > 0)
		{
			// Reuse an existing instance index if available
			// Transform.SetScale3D(FVector(Point.Size));
			int32 InstanceIndex = ReplaceInstanceIndices.Pop(EAllowShrinking::No);
			ISMComponent->UpdateInstanceTransform(InstanceIndex, Transform, true, false, true);
			InstanceIndices.Add(InstanceIndex);
		}
		else
		{
			// Add a new instance
			InstanceIndices.Add(ISMComponent->AddInstance(Transform, true));
		}
	}
	// 在最后统一标记 render state 为 dirty
	ISMComponent->MarkRenderStateDirty();
	ensure(TileInstanceIndices.Find(Tile) == nullptr); // Ensure no existing entry for this tile
	TileInstanceIndices.Add(Tile, MoveTemp(InstanceIndices));
}

double AISMBridgeSpawner::GetCustomSlopeAngle(int32 InstanceIndex) const
{
	FTransform Transform;
	ISMComponent->GetInstanceTransform(InstanceIndex, Transform);

	auto PitchAngle = Transform.GetRotation().Rotator().Pitch;
	return -PitchAngle;
}
