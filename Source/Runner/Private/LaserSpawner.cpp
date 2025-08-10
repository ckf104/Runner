// Fill out your copyright notice in the Description page of Project Settings.

#include "LaserSpawner.h"
#include "Engine/World.h"
#include "Math/MathFwd.h"

void ALaserSpawner::BeginPlay()
{
	Super::BeginPlay();
	MaxCachedBarriers = 0; // 有两类激光，就不缓存了
}

void ALaserSpawner::SpawnBarriers(TArrayView<RandomPoint> Positions, FInt32Point Tile, AWorldGenerator* WorldGenerator)
{
	if (!WorldGenerator || !BarrierClass)
	{
		return;
	}

	auto YSize = WorldGenerator->YCellNumber * WorldGenerator->CellSize;
	// auto YDist = YSize / OneLineLaserNumber;
	auto bGenerateSpecialLaser = false;

	for (auto& Point : Positions)
	{
		// if (!CanSpawnThisBarrier(Tile, Point.UVPos, WorldGenerator))
		// {
		// 	continue; // 跳过不允许生成障碍物的区域
		// }

		FTransform Transform;
		GetTransformFromSeed(Transform, Point, Tile, WorldGenerator);

		if (WorldGenerator->CurrentDifficulty >= 5 && !bGenerateSpecialLaser && Point.Rotation.Pitch > ProbabilityForBaseLaser && CanSpawnThisBarrier(Tile, Point.UVPos, WorldGenerator))
		{
			WorldGenerator->SpecialLaserPos.Add(FMath::Fmod(Tile.X + Point.UVPos.X, double(WorldGenerator->MoveOriginXTile)));
			
			FVector Location = Transform.GetLocation();
			Location.Y = YSize / 2.0;
			Transform.SetLocation(Location);
			Transform.SetRotation(FQuat::Identity);
			auto CachedActor = GetWorld()->SpawnActor<AActor>(AnotherLaserClass, Transform);
			SpawnedBarriers.Add(Tile, CachedActor);
			bGenerateSpecialLaser = true;
		}
		else
		{
			for (int32 i = 0; i < OneLineLaserNumber; ++i)
			{
				FVector Location = Transform.GetLocation();
				Location.Y += i * LaserDistance;
				if (Location.Y > YSize)
				{
					Location.Y = Location.Y - YSize;
				}

				ensure(Location.Y >= 0 && Location.Y <= YSize); // 确保位置在有效范围内
				Transform.SetLocation(Location);
				Transform.SetRotation(FQuat::Identity);

				AActor* CachedActor = nullptr;
				CachedActor = GetWorld()->SpawnActor<AActor>(BarrierClass, Transform);
				SpawnedBarriers.Add(Tile, CachedActor);
			}
		}
	}
}
