// Fill out your copyright notice in the Description page of Project Settings.

#include "BPBarrierSpawner.h"
#include "Containers/AllowShrinking.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "UObject/WeakObjectPtrTemplates.h"

void ABPBarrierSpawner::SpawnBarriers(TArrayView<RandomPoint> Positions, FInt32Point Tile, AWorldGenerator* WorldGenerator)
{
	if (!WorldGenerator || !BarrierClass)
	{
		return;
	}

	for (auto& Point : Positions)
	{
		if (!CanSpawnThisBarrier(Tile, Point.UVPos, WorldGenerator))
		{
			continue; // 跳过不允许生成障碍物的区域
		}

		FTransform Transform;
		GetTransformFromSeed(Transform, Point, Tile, WorldGenerator);
		AActor* CachedActor = nullptr;

		// 检查是否有缓存的障碍物可用
		while (CachedBarriers.Num() > 0)
		{
			TWeakObjectPtr<AActor> CachedBarrier = CachedBarriers.Pop(EAllowShrinking::No);
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

void ABPBarrierSpawner::RemoveTile(FInt32Point Tile)
{
	TArray<TWeakObjectPtr<AActor>> OutValues;
	SpawnedBarriers.MultiFind(Tile, OutValues);
	for (auto WeakActor : OutValues)
	{
		if (CachedBarriers.Num() < MaxCachedBarriers)
		{
			CachedBarriers.Add(WeakActor);
		}
		else
		{
			// 如果缓存已满，直接销毁
			if (AActor* Actor = WeakActor.Get())
			{
				Actor->Destroy();
			}
		}
	}
	SpawnedBarriers.Remove(Tile);
}
