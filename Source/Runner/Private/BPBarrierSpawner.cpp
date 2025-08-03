// Fill out your copyright notice in the Description page of Project Settings.

#include "BPBarrierSpawner.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "UObject/WeakObjectPtrTemplates.h"

int32 ABPBarrierSpawner::GetBarrierCountAnyThread(double RandomValue) const
{
	return FMath::RoundToInt(float(FMath::Lerp(MinBarrierCount, MaxBarrierCount, RandomValue)));
}

void ABPBarrierSpawner::SpawnBarriers(TArrayView<RandomPoint> Positions, FInt32Point Tile, AWorldGenerator* WorldGenerator)
{
	if (!WorldGenerator || !BarrierClass)
	{
		return;
	}

	for (auto& Point : Positions)
	{
		WorldGenerator->TransformUVToWorldPos(Point, Tile);
		FVector WorldPosition = Point.Transform.GetTranslation();
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
			CachedActor->SetActorRelativeTransform(Point.Transform);
		}
		else
		{
			CachedActor = GetWorld()->SpawnActor<AActor>(BarrierClass, Point.Transform);
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
