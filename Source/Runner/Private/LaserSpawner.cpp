// Fill out your copyright notice in the Description page of Project Settings.

#include "LaserSpawner.h"
#include "Engine/World.h"

void ALaserSpawner::SpawnBarriers(TArrayView<RandomPoint> Positions, FInt32Point Tile, AWorldGenerator* WorldGenerator)
{
	if (!WorldGenerator || !BarrierClass)
	{
		return;
	}

	auto YSize = WorldGenerator->YCellNumber * WorldGenerator->CellSize;
	auto YDist = YSize / OneLineLaserNumber;

	for (auto& Point : Positions)
	{
		if (!CanSpawnThisBarrier(Tile, Point.UVPos, WorldGenerator))
		{
			continue; // 跳过不允许生成障碍物的区域
		}

		FTransform Transform;
		GetTransformFromSeed(Transform, Point, Tile, WorldGenerator);

		for (int32 i = 0; i < OneLineLaserNumber; ++i)
		{
			FVector Location = Transform.GetLocation();
			Location.Y += i * YDist;
			if (Location.Y > YSize)
			{
				Location.Y = Location.Y - YSize;
			}
			ensure(Location.Y >= 0 && Location.Y <= YSize); // 确保位置在有效范围内
			Transform.SetLocation(Location);

			AActor* CachedActor = nullptr;

			// 检查是否有缓存的障碍物可用
			while (CachedBarriers.Num() > 0)
			{
				TWeakObjectPtr<AActor> CachedBarrier = CachedBarriers.Pop();
				CachedActor = CachedBarrier.Get();
				if (CachedActor)
				{
					break;
				}
			}
			if (CachedActor)
			{
				CachedActor->SetActorRelativeTransform(Transform);
			}
			else
			{
				CachedActor = GetWorld()->SpawnActor<AActor>(BarrierClass, Transform);
			}
			SpawnedBarriers.Add(Tile, CachedActor);
		}
	}
}
