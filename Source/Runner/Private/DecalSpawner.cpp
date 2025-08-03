// Fill out your copyright notice in the Description page of Project Settings.


#include "DecalSpawner.h"
#include "Components/DecalComponent.h"
#include "Components/SphereComponent.h"
#include "Containers/AllowShrinking.h"
#include "Math/MathFwd.h"

ADecalSpawner::ADecalSpawner()
{
  RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("RootComponent"));
}

FRotator ADecalSpawner::GetRotationFromSeed(FRotator Seed) const
{
  FRotator Result = FRotator::ZeroRotator;
	Result.Yaw = Seed.Yaw * 360.0; // 将 0 - 1 的随机变量转换为 0 - 360 度
  
  Result.Pitch = -90.0; // 保证 decal 的 X 轴正方向是朝下的
  return Result;
}

void ADecalSpawner::SpawnBarriers(TArrayView<RandomPoint> Positions, FInt32Point Tile, AWorldGenerator* WorldGenerator)
{
  TArray<UDecalComponent*> DecalsInTile;

  for (const RandomPoint& Position : Positions)
  {
		if (!CanSpawnThisBarrier(Tile, Position.UVPos, WorldGenerator))
		{
			continue; // 跳过不允许生成障碍物的区域
		}

    FTransform Transform;
    GetTransformFromSeed(Transform, Position, Tile, WorldGenerator);

    auto DecalScale = FMath::Lerp(MinDecalScale, MaxDecalScale, Position.Rotation.Pitch);
    Transform.SetScale3D(FVector(DecalScale, DecalScale, DecalScale));
  
    UDecalComponent* Decal;
    if (CachedDecals.Num() > 0)
    {
      Decal = CachedDecals.Pop(EAllowShrinking::No);
      Decal->SetWorldTransform(Transform);
    }
    else
    {
      Decal = NewObject<UDecalComponent>(this, UDecalComponent::StaticClass());
      Decal->DecalSize.X = Decal->DecalSize.Y; // 使用默认值 256
      Decal->SetWorldTransform(Transform);
      Decal->SetMaterial(0, DecalMaterial);
      Decal->RegisterComponent();
      Decal->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepWorldTransform);
      
      // 增加球形碰撞体
      auto* Sphere = NewObject<USphereComponent>(this, USphereComponent::StaticClass());
      Sphere->ComponentTags.Add(FName("Mud"));
      Sphere->SetHiddenInGame(!bShowSphere);
      Sphere->SetCollisionProfileName("MudSphere");
      Sphere->InitSphereRadius(Decal->DecalSize.X * SphereScale);
      Sphere->RegisterComponent();
      Sphere->AttachToComponent(Decal, FAttachmentTransformRules::KeepRelativeTransform);
    }
    DecalsInTile.Add(Decal);
  }
  
  ensure(SpawnedDecals.Find(Tile) == nullptr); // Ensure no existing entry for this tile
  SpawnedDecals.Add(Tile, MoveTemp(DecalsInTile));  
}

void ADecalSpawner::RemoveTile(FInt32Point Tile)
{
  auto* Decals = SpawnedDecals.Find(Tile);
  if (Decals)
  {
    CachedDecals.Append(*Decals);
    SpawnedDecals.Remove(Tile);
  }
}
