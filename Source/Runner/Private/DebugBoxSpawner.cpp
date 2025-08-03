// Fill out your copyright notice in the Description page of Project Settings.


#include "DebugBoxSpawner.h"
#include "DrawDebugHelpers.h"

void ADebugBoxSpawner::SpawnBarriers(TArrayView<RandomPoint> Positions, FInt32Point Tile, AWorldGenerator* WorldGenerator)
{
  if (!WorldGenerator)
  {
    return;
  }

  for (auto& Point : Positions)
  {
    WorldGenerator->TransformUVToWorldPos(Point, Tile);
    FVector BoxExtent = FVector(50.0f / 2.0f, 50.0f / 2.0f, 50.0f / 2.0f);

    // Spawn a debug box at the calculated position
    DrawDebugBox(WorldGenerator->GetWorld(), Point.Transform.GetTranslation(), BoxExtent, FColor::Red, true, -1.0f, 0, 5.0f);
  }
}


