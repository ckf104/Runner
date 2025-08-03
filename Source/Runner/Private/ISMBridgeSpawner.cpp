// Fill out your copyright notice in the Description page of Project Settings.


#include "ISMBridgeSpawner.h"
#include "Math/MathFwd.h"

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
	InstanceIndices.SetNumUninitialized(Positions.Num());
	int32 Idx = 0;
	for (auto& Point : Positions)
	{
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
			ISMComponent->UpdateInstanceTransform(InstanceIndex, Transform, true, false, false);
			InstanceIndices[Idx++] = InstanceIndex;
		}
		else
		{
			// Add a new instance
			InstanceIndices[Idx++] = ISMComponent->AddInstance(Transform);
		}
	}
	// 在最后统一标记 render state 为 dirty
	ISMComponent->MarkRenderStateDirty();
	ensure(TileInstanceIndices.Find(Tile) == nullptr); // Ensure no existing entry for this tile
	TileInstanceIndices.Add(Tile, InstanceIndices);
}

double AISMBridgeSpawner::GetCustomSlopeAngle(int32 InstanceIndex) const
{
	FTransform Transform;
	ISMComponent->GetInstanceTransform(InstanceIndex, Transform);

  auto PitchAngle = Transform.GetRotation().Rotator().Pitch;
  return -PitchAngle;
}
