// Fill out your copyright notice in the Description page of Project Settings.

#include "GoldCoinSpawner.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Character.h"
#include "ISMBridgeSpawner.h"
#include "Kismet/GameplayStatics.h"
#include "Math/MathFwd.h"
#include "RunnerMovementComponent.h"

AGoldCoinSpawner::AGoldCoinSpawner()
{
  bDeferSpawn = true; // 默认延迟 spawn
}

void AGoldCoinSpawner::BeginPlay()
{
	TActorIterator<AISMBridgeSpawner> It(GetWorld());
	if (It)
	{
		BridgeSpawner = *It;
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("AGoldCoinSpawner::BeginPlay: No AISMBridgeSpawner found in the world!"));
	}
	auto* Character = UGameplayStatics::GetPlayerCharacter(this, 0);
	if (Character)
	{
		auto* MoveComp = Cast<URunnerMovementComponent>(Character->GetCharacterMovement());
		if (MoveComp)
		{
			// 获取玩家的水平移动速度
			MaxWalkingSpeed = MoveComp->MaxWalkSpeed;
			// 获取玩家的下落速度
			ZVelocityInAir = MoveComp->ZVelocityInAir;
			Gravity = MoveComp->GetGravityZ();
			// 获取玩家按下 down 时的下落速度
			MaxZVelocityInAir = MoveComp->MaxStartZVelocityInAir;
			ZVelocityDownSpeed = MoveComp->ZVelocityDownSpeed;
      // 获取起飞时的 Z 速度
      TakeoffSpeedScale = MoveComp->TakeoffSpeedScale;
      MaxStartZVelocityInAir = MoveComp->MaxStartZVelocityInAir;
		}
	}
	// 不能缓存金币
	MaxCachedBarriers = 0;
}

FVector2D AGoldCoinSpawner::GetCurrentGroundDirection(double time)
{
	// 默认为前方
	return FVector2D(1, 0);
}

void AGoldCoinSpawner::GenerateGoldTrace(FVector WorldStartPos, double StartYaw, double StartZVelocity, FInt32Point Tile, AWorldGenerator* WorldGenerator)
{
	FVector CoinPos = WorldStartPos;
	double ZVelocity = StartZVelocity;
	double Time = 0.0;

	for (int32 i = 0; i < MaxCoinNumber; ++i)
	{
		// 计算当前位置
		FVector2D CurrentDirection = GetCurrentGroundDirection(Time);
		double X = CoinPos.X + MaxWalkingSpeed * SpawnTimeInterval * CurrentDirection.X;
		double Y = CoinPos.Y + MaxWalkingSpeed * SpawnTimeInterval * CurrentDirection.Y;
		double Z = CoinPos.Z + ZVelocity * SpawnTimeInterval + 0.5 * Gravity * SpawnTimeInterval * SpawnTimeInterval;

    DrawDebugBox(GetWorld(), FVector(X, Y, Z), FVector(50, 50, 50), FColor::Yellow, true, 5.0f);

    auto WorldHeight = WorldGenerator->GetVisualHeightFromHorizontalPos(FVector2D(X, Y));
    DrawDebugBox(GetWorld(), FVector(X, Y, WorldHeight), FVector(50, 50, 50), FColor::Red, true, 5.0f);
    // 地面的碰撞是异步生成的，因此我们使用高度图直接检查碰撞
    if (Z < WorldHeight + BarrierRadius)
    {
      UE_LOG(LogTemp, Warning, TEXT("AGoldCoinSpawner::GenerateGoldTrace: Coin Z position %f is below world height %f at (%f, %f)"), Z, WorldHeight, X, Y);
      break;
    }

		CoinPos = FVector(X, Y, Z);

		// 生成金币
		FActorSpawnParameters SpawnParams;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::DontSpawnIfColliding;
		auto* CoinActor = GetWorld()->SpawnActor<AActor>(BarrierClass, CoinPos, FRotator(0, StartYaw, 0), SpawnParams);
		if (CoinActor)
		{
			SpawnedBarriers.Add(Tile, CoinActor);
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("AGoldCoinSpawner::GenerateGoldTrace: Failed to spawn coin at %s"), *CoinPos.ToString());
			break;
		}

		// 计算下一个时间点的速度
		double NextTime = Time + SpawnTimeInterval;
		double NextZVelocity = ZVelocity + Gravity * SpawnTimeInterval;

		NextZVelocity = FMath::Max(NextZVelocity, ZVelocityInAir);

		Time = NextTime;
		ZVelocity = NextZVelocity;
	}
}

void AGoldCoinSpawner::SpawnBarriers(TArrayView<RandomPoint> Positions, FInt32Point Tile, AWorldGenerator* WorldGenerator)
{
	if (!WorldGenerator || !BarrierClass || !BridgeSpawner)
	{
		return;
	}
	const auto* Instances = BridgeSpawner->GetInstanceInTile(Tile);
	if (Instances == nullptr || Instances->IsEmpty())
	{
		return;
	}

	TArray<bool> UsedInstances;
	UsedInstances.SetNumZeroed(Instances->Num());

	for (auto& Point : Positions)
	{
		auto CoinRotator = GetRotationFromSeed(Point.Rotation);
		auto StartInstanceIndex = FMath::Clamp(FMath::FloorToInt32(Point.UVPos.X * Instances->Num()), 0, Instances->Num() - 1);
		int32 FinalInstanceIndex = 0;
		int32 i = 0;
		for (; i < UsedInstances.Num(); i++)
		{
			FinalInstanceIndex = (StartInstanceIndex + i) % UsedInstances.Num();
			if (!UsedInstances[FinalInstanceIndex])
			{
				UsedInstances[FinalInstanceIndex] = true;
				break;
			}
		}
    if (i >= UsedInstances.Num())
    {
      UE_LOG(LogTemp, Warning, TEXT("AGoldCoinSpawner::SpawnBarriers: All Bridge instances in tile %s are used up!"), *Tile.ToString());
      break;
    }

    auto SlopeAngle = BridgeSpawner->GetCustomSlopeAngle(Instances->operator[](FinalInstanceIndex));
    FTransform InstanceTransform;
    BridgeSpawner->ISMComponent->GetInstanceTransform(Instances->operator[](FinalInstanceIndex), InstanceTransform, true);

    auto CoinStartPos = InstanceTransform.TransformPosition(CoinStartOffset);
    auto StartZ = URunnerMovementComponent::CalcStartZVelocity(SlopeAngle, MaxWalkingSpeed, TakeoffSpeedScale, MaxStartZVelocityInAir);

    GenerateGoldTrace(CoinStartPos, CoinRotator.Yaw, StartZ, Tile, WorldGenerator);
	}
}

bool AGoldCoinSpawner::DeferSpawnBarriers(TArrayView<RandomPoint> Positions, FInt32Point Tile, AWorldGenerator* WorldGenerator)
{
  auto DepentTile = FInt32Point(Tile.X + 1, Tile.Y);
  if (!WorldGenerator->IsValidTile(DepentTile))
  {
    return false;
  }
  SpawnBarriers(Positions, Tile, WorldGenerator);
  return true;
}
