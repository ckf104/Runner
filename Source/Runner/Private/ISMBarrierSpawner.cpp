// Fill out your copyright notice in the Description page of Project Settings.

#include "ISMBarrierSpawner.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Containers/AllowShrinking.h"
#include "Math/MathFwd.h"
#include "Misc/AssertionMacros.h"
#include "Templates/UnrealTemplate.h"

AISMBarrierSpawner::AISMBarrierSpawner()
{
	// Create the Instanced Static Mesh Component
	ISMComponent = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("ISMComponent"));
	RootComponent = ISMComponent;
	ISMComponent->SetMobility(EComponentMobility::Static);
	ISMComponent->bReceivesDecals = false; // 不接收 decal
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
		if (!CanSpawnThisBarrier(Tile, Point.UVPos, WorldGenerator))
		{
			continue; // 跳过不允许生成障碍物的区域
		}
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
	TileInstanceIndices.Add(Tile, MoveTemp(InstanceIndices));
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