// Fill out your copyright notice in the Description page of Project Settings.

#include "ISMClusterSpawner.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "GameFramework/Actor.h"
#include "Math/MathFwd.h"
#include "UObject/UnrealType.h"

AISMClusterSpawner::AISMClusterSpawner()
{
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("RootComponent"));
	RootComponent->SetMobility(EComponentMobility::Static);
}

// void AISMClusterSpawner::PostInitProperties()
// {
// 	Super::PostInitProperties();
//
// 	// 初始化 ISMComponents
// 	for (int32 i = 0; i < Meshes.Num(); ++i)
// 	{
// 		UInstancedStaticMeshComponent* NewISMComponent = NewObject<UInstancedStaticMeshComponent>(this, UInstancedStaticMeshComponent::StaticClass(), *FString::Printf(TEXT("ISMComponent_%d"), i));
// 		NewISMComponent->SetStaticMesh(Meshes[i]);
// 		NewISMComponent->SetMobility(EComponentMobility::Static);
// 		NewISMComponent->SetCollisionProfileName(CollisionProfileName);
// 		NewISMComponent->SetupAttachment(RootComponent);
// 		NewISMComponent->RegisterComponent();
// 		ISMComponents.Add(NewISMComponent);
// 	}
// 	ReplaceInstanceIndices.SetNum(ISMComponents.Num());
// }

void AISMClusterSpawner::BeginPlay()
{
	Super::BeginPlay();
	ReplaceInstanceIndices.SetNum(ISMComponents.Num());
}

// TODO: 这里用到了 unreal 的随机函数，破坏了可复现性
void AISMClusterSpawner::SpawnBarriers(TArrayView<RandomPoint> Positions, FInt32Point Tile, AWorldGenerator* WorldGenerator)
{
	TArray<FInt32Point> InstanceIndices;
	int32 Idx = 0;

	int32 XSize = WorldGenerator->CellSize * WorldGenerator->XCellNumber;
	int32 YSize = WorldGenerator->CellSize * WorldGenerator->YCellNumber;

	auto IsValidPos = [XSize, YSize, MinDist = this->MinDistanceWithinCluster](FVector2D Pos, const TInlineComponentArray<FVector2D, 8>& PlacedPos) {
		if (Pos.X < 0 || Pos.X >= XSize || Pos.Y < 0 || Pos.Y >= YSize)
		{
			return false;
		}
		for (const auto& Other : PlacedPos)
		{
			if (FVector2D::DistSquared(Pos, Other) < FMath::Square(MinDist))
			{
				return false;
			}
		}
		return true;
	};

	for (auto& Point : Positions)
	{
		auto MeshCountInCluster = FMath::RandRange(MinMeshCountInCluster, MaxMeshCountInCluster);

		auto TilePos = FVector2D(Point.UVPos.X * XSize, Point.UVPos.Y * YSize);

		TInlineComponentArray<FVector2D, 8> PlacedPos;
		PlacedPos.Add(TilePos);

		for (int32 i = 1; i < MeshCountInCluster; ++i)
		{
			int32 TryNumber = 0;
			while (TryNumber < TryTimeBeforeGiveUp)
			{
				auto X = FMath::FRandRange(-1.0, 1.0);
				auto Y = FMath::FRandRange(-1.0, 1.0);
				auto NewPos = TilePos + FVector2D(X, Y) * HalfClusterExtent;
				if (!IsValidPos(NewPos, PlacedPos))
				{
					// 超出边界，重新尝试
					++TryNumber;
					continue;
				}
				else
				{
					PlacedPos.Add(NewPos);
					break;
				}
			}
			if (TryNumber >= TryTimeBeforeGiveUp)
			{
				UE_LOG(LogBarrierSpawner, Warning, TEXT("ISMClusterSpawner: Failed to place enough meshes in cluster at tile %s"), *Tile.ToString());
				break;
			}
		}

		for (auto Pos : PlacedPos)
		{
			auto UV = FVector2D(Pos.X / XSize, Pos.Y / YSize);
			Point.UVPos = UV;

			FTransform Transform;
			GetTransformFromSeed(Transform, Point, Tile, WorldGenerator);

			auto MeshIndex = FMath::RandRange(0, ISMComponents.Num() - 1); // Randomly select an ISM component
			auto ISMComponent = ISMComponents[MeshIndex];
			if (ReplaceInstanceIndices[MeshIndex].Num() > 0)
			{
				// Reuse an existing instance index if available
				auto InstanceIndex = ReplaceInstanceIndices[MeshIndex].Pop(EAllowShrinking::No);
				ISMComponent->UpdateInstanceTransform(InstanceIndex, Transform, true, true, true);
				InstanceIndices.Add(FInt32Point(InstanceIndex, MeshIndex));
			}
			else
			{
				// Add a new instance
				auto InstanceIndex = ISMComponent->AddInstance(Transform, true);
				InstanceIndices.Add(FInt32Point(InstanceIndex, MeshIndex));
			}
		}
	}

	ensure(TileInstanceIndices.Find(Tile) == nullptr); // Ensure no existing entry for this tile
	TileInstanceIndices.Add(Tile, InstanceIndices);
}

void AISMClusterSpawner::RemoveTile(FInt32Point Tile)
{
	if (TileInstanceIndices.Contains(Tile))
	{
		auto& InstanceIndices = TileInstanceIndices[Tile];
		for (auto Pair : InstanceIndices)
		{
			UInstancedStaticMeshComponent* ISMComponent = ISMComponents[Pair.Y];
			ReplaceInstanceIndices[Pair.Y].Add(Pair.X); // Add to replace pool
		}
		TileInstanceIndices.Remove(Tile);
	}
}

#if WITH_EDITOR

void AISMClusterSpawner::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	// UE_LOG(LogBarrierSpawner, Warning, TEXT("AISMClusterSpawner::PostEditChangeProperty called"));
	static const FName ISMComponentsName = GET_MEMBER_NAME_CHECKED(AISMClusterSpawner, ISMComponents);
	if (PropertyChangedEvent.GetPropertyName() == ISMComponentsName)
	{
		if (PropertyChangedEvent.ChangeType == EPropertyChangeType::ArrayRemove || PropertyChangedEvent.ChangeType == EPropertyChangeType::ArrayClear || PropertyChangedEvent.ChangeType == EPropertyChangeType::ValueSet)
		{
			// 清理已从数组中移除的 ISMComponent
			TArray<UInstancedStaticMeshComponent*> ComponentsToRemove;
			ForEachComponent<UInstancedStaticMeshComponent>(false, [this, &ComponentsToRemove](UInstancedStaticMeshComponent* ISMComponent) {
				if (!ISMComponent)
				{
					return;
				}
				// 清理已被移除的 ISMComponent

				if (!ISMComponents.Contains(ISMComponent))
				{
					// 不能直接 Destroy，否则会导致 actor 的 owned components 在循环中被修改
					ComponentsToRemove.Add(ISMComponent);
				}
			});
			for (UInstancedStaticMeshComponent* ISMComponent : ComponentsToRemove)
			{
				ISMComponent->DestroyComponent();
			}

			// 清理已被移除的 ISMComponent
			for (int32 i = ISMComponents.Num() - 1; i >= 0; --i)
			{
				if (!ISMComponents[i])
				{
					continue;
				}
				if (ISMComponents[i]->GetOuter() != this)
				{
					ISMComponents[i] = nullptr;
				}
			}
			// 移除无效项
			ISMComponents.RemoveAll([](const TObjectPtr<UInstancedStaticMeshComponent>& Comp) { return Comp == nullptr; });
		}
		else if (PropertyChangedEvent.ChangeType == EPropertyChangeType::ArrayAdd)
		{
			// 补充新加的 ISMComponent
			for (int32 i = 0; i < ISMComponents.Num(); ++i)
			{
				if (!ISMComponents[i])
				{
					UInstancedStaticMeshComponent* NewISMComponent = NewObject<UInstancedStaticMeshComponent>(this, UInstancedStaticMeshComponent::StaticClass(), *FString::Printf(TEXT("ISMComponent_%d"), i));
					NewISMComponent->SetMobility(EComponentMobility::Static);
					NewISMComponent->SetCollisionProfileName("Barrier");
					NewISMComponent->SetupAttachment(RootComponent);
					NewISMComponent->RegisterComponent();
					ISMComponents[i] = NewISMComponent;
				}
			}
		}
	}
}
#endif