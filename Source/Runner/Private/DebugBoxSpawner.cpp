// Fill out your copyright notice in the Description page of Project Settings.


#include "DebugBoxSpawner.h"
#include "DrawDebugHelpers.h"

void ADebugBoxSpawner::SpawnBarriers(const TArray<RandomPoint>& Positions, FInt32Point Tile, AWorldGenerator* WorldGenerator)
{
  if (!WorldGenerator)
  {
    return;
  }

  for (const auto& Point : Positions)
  {
    FVector WorldPosition = WorldGenerator->GetWorldPositionFromUV(Point.Position, Tile);
    FVector BoxExtent = FVector(50.0f / 2.0f, 50.0f / 2.0f, 50.0f / 2.0f);

    // Spawn a debug box at the calculated position
    DrawDebugBox(WorldGenerator->GetWorld(), WorldPosition, BoxExtent, FColor::Red, true, -1.0f, 0, 5.0f);
  }
}


