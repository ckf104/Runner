// Fill out your copyright notice in the Description page of Project Settings.


#include "ISMBarrierSpawner.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Containers/AllowShrinking.h"
#include "Misc/AssertionMacros.h"


AISMBarrierSpawner::AISMBarrierSpawner()
{
  // Create the Instanced Static Mesh Component
  ISMComponent = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("ISMComponent"));
  RootComponent = ISMComponent;
  ISMComponent->SetMobility(EComponentMobility::Static);
}

void AISMBarrierSpawner::SpawnBarriers(const TArray<RandomPoint>& Positions, FInt32Point Tile, AWorldGenerator* WorldGenerator)
{
  if (!ISMComponent || !WorldGenerator)
  {
    return;
  }

  TArray<int32> InstanceIndices;
  InstanceIndices.SetNumUninitialized(Positions.Num());
  int32 Idx = 0;
  for (const auto& Point : Positions)
  {
    FTransform Transform;
    auto WorldPosition = WorldGenerator->GetWorldPositionFromUV(Point.Position, Tile);
    Transform.SetLocation(WorldPosition);

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

void AISMBarrierSpawner::RemoveTile(FInt32Point Tile, AWorldGenerator* WorldGenerator)
{
  if (!ISMComponent || !WorldGenerator)
  {
    return;
  }

  auto InstanceIndices = TileInstanceIndices.Find(Tile);
  if (InstanceIndices)
  {
    ReplaceInstanceIndices.Append(*InstanceIndices);
    TileInstanceIndices.Remove(Tile);
  }
}