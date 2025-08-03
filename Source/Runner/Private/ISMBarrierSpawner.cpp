// Fill out your copyright notice in the Description page of Project Settings.

#include "ISMBarrierSpawner.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Containers/AllowShrinking.h"
#include "Math/MathFwd.h"
#include "Misc/AssertionMacros.h"

AISMBarrierSpawner::AISMBarrierSpawner()
{
	// Create the Instanced Static Mesh Component
	ISMComponent = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("ISMComponent"));
	RootComponent = ISMComponent;
	ISMComponent->SetMobility(EComponentMobility::Static);
}

int32 AISMBarrierSpawner::GetBarrierCountAnyThread(double RandomValue) const
{
	// 根据随机值返回障碍物数量
	return FMath::RoundToInt(float(FMath::Lerp(MinBarrierCount, MaxBarrierCount, RandomValue)));
}

void AISMBarrierSpawner::SpawnBarriers(TArrayView<RandomPoint> Positions, FInt32Point Tile, AWorldGenerator* WorldGenerator)
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

		if (ReplaceInstanceIndices.Num() > 0)
		{
			// Reuse an existing instance index if available
			// Transform.SetScale3D(FVector(Point.Size));
			int32 InstanceIndex = ReplaceInstanceIndices.Pop(EAllowShrinking::No);
			ISMComponent->UpdateInstanceTransform(InstanceIndex, Transform, true, false, true);
			InstanceIndices[Idx++] = InstanceIndex;
		}
		else
		{
			// Add a new instance
			InstanceIndices[Idx++] = ISMComponent->AddInstance(Transform, true);
		}
	}
	// 在最后统一标记 render state 为 dirty
	ISMComponent->MarkRenderStateDirty();
	ensure(TileInstanceIndices.Find(Tile) == nullptr); // Ensure no existing entry for this tile
	TileInstanceIndices.Add(Tile, InstanceIndices);
}

void AISMBarrierSpawner::RemoveTile(FInt32Point Tile)
{
	auto InstanceIndices = TileInstanceIndices.Find(Tile);
	if (InstanceIndices)
	{
		ReplaceInstanceIndices.Append(*InstanceIndices);
		TileInstanceIndices.Remove(Tile);
	}
}