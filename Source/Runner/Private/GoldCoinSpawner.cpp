// Fill out your copyright notice in the Description page of Project Settings.

#include "GoldCoinSpawner.h"
#include "DrawDebugHelpers.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Character.h"
#include "ISMBridgeSpawner.h"
#include "Kismet/GameplayStatics.h"
#include "Math/MathFwd.h"
#include "Misc/AssertionMacros.h"
#include "RunnerMovementComponent.h"
#include "WorldGenerator.h"

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
		UE_LOG(LogBarrierSpawner, Error, TEXT("AGoldCoinSpawner::BeginPlay: No AISMBridgeSpawner found in the world!"));
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

int32 AGoldCoinSpawner::GenerateGoldTrace(FVector WorldStartPos, double StartYaw, double StartZVelocity, FInt32Point Tile, AWorldGenerator* WorldGenerator, FVector2D RandomSeed)
{
	FVector CoinPos = WorldStartPos;
	double ZVelocity = StartZVelocity;
	double Time = 0.0;
	int32 CoinNumber = 0;

	for (int32 i = 0; i < MaxCoinNumber; ++i)
	{
		// 计算当前位置
		FVector2D CurrentDirection = GetCurrentGroundDirection(Time);
		double X = CoinPos.X + MaxWalkingSpeed * SpawnTimeInterval * CurrentDirection.X;
		double Y = CoinPos.Y + MaxWalkingSpeed * SpawnTimeInterval * CurrentDirection.Y;
		double Z = CoinPos.Z + ZVelocity * SpawnTimeInterval + 0.5 * Gravity * SpawnTimeInterval * SpawnTimeInterval;

		// DrawDebugBox(GetWorld(), FVector(X, Y, Z), FVector(50, 50, 50), FColor::Yellow, true, 5.0f);

		auto WorldHeight = WorldGenerator->GetVisualHeightFromHorizontalPos(FVector2D(X, Y));
		// DrawDebugBox(GetWorld(), FVector(X, Y, WorldHeight), FVector(50, 50, 50), FColor::Red, true, 5.0f);
		// 地面的碰撞是异步生成的，因此我们使用高度图直接检查碰撞
		if (Z < WorldHeight + BarrierRadius)
		{
			// UE_LOG(LogBarrierSpawner, Warning, TEXT("AGoldCoinSpawner::GenerateGoldTrace: Coin Z position %f is below world height %f at (%f, %f), stop generating further coins."), Z, WorldHeight, X, Y);
			break; // 如果不允许埋地，直接停止生成
		}

		CoinPos = FVector(X, Y, Z);

		// 生成金币
		FActorSpawnParameters SpawnParams;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::DontSpawnIfColliding;

		auto BarrierToSpawn = BarrierClass;
		auto ItemNumber = -1;
		if (i == ItemAppearNumber)
		{
			auto bItemAppear = RandomSeed.X < ItemAppearProbability;
			if (bItemAppear)
			{
				ItemNumber = FMath::Clamp(FMath::FloorToInt32(RandomSeed.Y * Itemclasses.Num()), 0, Itemclasses.Num() - 1);
				BarrierToSpawn = Itemclasses[ItemNumber];
			}
		}
		// 检查 special laser 的位置，看生成云是否合适
		// 根据目前的生成逻辑，在生成金币时，下一个 tile 的障碍物应该都生成好了

		if (ItemNumber == 0 && !CanGenerateCloudTrace(WorldGenerator, CoinPos.X / (WorldGenerator->CellSize * WorldGenerator->XCellNumber)))
		{
			BarrierToSpawn = BarrierClass; // 如果不能生成云，则使用默认的金币类
			ItemNumber = -1;
		}

		auto* CoinActor = GetWorld()->SpawnActor<AActor>(BarrierToSpawn, CoinPos, FRotator(0, StartYaw, 0), SpawnParams);
		if (CoinActor)
		{
			// DrawDebugBox(GetWorld(), CoinPos, FVector(50, 50, 50), FColor::Green, true, 10.0f);
			SpawnedBarriers.Add(Tile, CoinActor);
			CoinNumber++;
		}
		else
		{
			// UE_LOG(LogBarrierSpawner, Warning, TEXT("AGoldCoinSpawner::GenerateGoldTrace: Failed to spawn coin at %s"), *CoinPos.ToString());
			break;
		}
		// 处理云的生成
		if (ItemNumber == 0)
		{
			GenerateCloudTrace(CoinPos, Tile);
			break; // 云生成后，停止生成后续的金币
		}

		// 计算下一个时间点的速度
		double NextTime = Time + SpawnTimeInterval;
		double NextZVelocity = ZVelocity + Gravity * SpawnTimeInterval;

		NextZVelocity = FMath::Max(NextZVelocity, ZVelocityInAir);

		Time = NextTime;
		ZVelocity = NextZVelocity;
	}
	return CoinNumber;
}

void AGoldCoinSpawner::SpawnDeferredBarriers(TArrayView<RandomPoint> Positions, FInt32Point Tile, FVector2D UVPos, AWorldGenerator* WorldGenerator)
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

	TArrayView<RandomPoint> PosBridge;
	TArrayView<RandomPoint> PosGround;
	ensure(Positions.Num() == 2);
	if (Positions.Num() > 1)
	{
		PosGround = Positions.Slice(Positions.Num() - 1, 1);
		PosBridge = Positions.Slice(0, Positions.Num() - 1);
	}
	else
	{
		PosBridge = Positions;
	}

	// 生成桥上的金币
	int32 FinalInstanceIndex = 0;
	for (auto& Point : PosBridge)
	{
		auto CoinRotator = GetRotationFromSeed(Point.Rotation);

		auto SlopeAngle = BridgeSpawner->GetCustomSlopeAngle(Instances->operator[](FinalInstanceIndex));
		FTransform InstanceTransform;
		BridgeSpawner->ISMComponent->GetInstanceTransform(Instances->operator[](FinalInstanceIndex), InstanceTransform, true);

		auto CoinStartPos = InstanceTransform.TransformPosition(CoinStartOffset);
		auto StartZ = URunnerMovementComponent::CalcStartZVelocity(SlopeAngle, MaxWalkingSpeed, TakeoffSpeedScale, MaxStartZVelocityInAir);

		GenerateGoldTrace(CoinStartPos, CoinRotator.Yaw, StartZ, Tile, WorldGenerator, FVector2D(Point.Rotation.Pitch, Point.Rotation.Roll));
		++FinalInstanceIndex;
		if (FinalInstanceIndex >= Instances->Num())
		{
			break; // 防止越界
		}
	}

	// 生成地面上的金币
	if (UVPos.X >= 0.0 && UVPos.Y >= 0.0)
	{
		auto StepU = 1.0 / WorldGenerator->XCellNumber;
		auto CoinRotator = GetRotationFromSeed(PosGround[0].Rotation);
		auto CurrentUV = FVector2D(UVPos);
		auto PrevUV = FVector2D(CurrentUV.X - StepU, CurrentUV.Y);
		auto CurrentPos = WorldGenerator->GetVisualWorldPositionFromUV(CurrentUV, Tile);
		auto PrevPos = WorldGenerator->GetVisualWorldPositionFromUV(PrevUV, Tile);

		auto dOld = (CurrentPos.Z - PrevPos.Z) / WorldGenerator->CellSize;

		auto StartPos = CurrentPos + CoinStartOffset;
		auto PitchAngle = -FMath::RadiansToDegrees(FMath::Atan(dOld));
		auto StartZ = URunnerMovementComponent::CalcStartZVelocity(PitchAngle, MaxWalkingSpeed, TakeoffSpeedScale, MaxStartZVelocityInAir);
		auto GeneratedNumber = GenerateGoldTrace(StartPos, CoinRotator.Yaw, StartZ, Tile, WorldGenerator, FVector2D(PosGround[0].Rotation.Pitch, PosGround[0].Rotation.Roll));
		// UE_LOG(LogBarrierSpawner, Log, TEXT("AGoldCoinSpawner::SpawnBarriers: Generated gold trace at %s, PitchAngle %f, StartZ: %f, GenerateNumber: %d"), *StartPos.ToString(), -PitchAngle, StartZ, GeneratedNumber);
		if (GeneratedNumber > 0)
		{
			// DrawDebugBox(GetWorld(), StartPos, FVector(300, 300, 300), FColor::Yellow, true, 5.0f);
		}
	}
}

FVector2D AGoldCoinSpawner::PreSpawnBarriers(TArrayView<RandomPoint> Positions, FInt32Point Tile, AWorldGenerator* WorldGenerator)
{
	ensure(Positions.Num() == 2);
	TArrayView<RandomPoint> PosGround;
	PosGround = Positions.Slice(Positions.Num() - 1, 1);

	// 生成地面上的金币
	auto YSize = double(WorldGenerator->YCellNumber) * WorldGenerator->CellSize;
	auto StepU = 1.0 / WorldGenerator->XCellNumber;
	for (auto& Point : PosGround)
	{
		auto CoinRotator = GetRotationFromSeed(Point.Rotation);
		auto StartY = Point.UVPos.Y * YSize;

		auto PrevPos = WorldGenerator->GetVisualWorldPositionFromUV(FVector2D(0, Point.UVPos.Y), Tile);
		auto CurrentPos = WorldGenerator->GetVisualWorldPositionFromUV(FVector2D(StepU, Point.UVPos.Y), Tile);
		auto NextPos = FVector::ZeroVector;
		for (auto i = 1; i < WorldGenerator->XCellNumber - 1; ++i)
		{
			NextPos = WorldGenerator->GetVisualWorldPositionFromUV(FVector2D((i + 1) * StepU, Point.UVPos.Y), Tile);
			// 一阶倒数
			auto dNew = (NextPos.Z - CurrentPos.Z) / WorldGenerator->CellSize;
			auto dOld = (CurrentPos.Z - PrevPos.Z) / WorldGenerator->CellSize;
			// 二阶导数
			auto dd = (dNew - dOld) / WorldGenerator->CellSize;
			auto Length = FMath::Pow(1 + dNew * dNew, 1.5);
			// 曲率
			auto Curvature = dd / Length;
			auto Final = -Curvature * MaxWalkingSpeed * MaxWalkingSpeed;

			// 此处能够起飞，生成金币路径
			// TODO: 把常数替换掉
			if (Final > 1000.0 && CurrentPos.Z > PrevPos.Z)
			{
				auto CurrentUV = FVector2D(i * StepU, Point.UVPos.Y);
				return CurrentUV;
			}
			PrevPos = CurrentPos;
			CurrentPos = NextPos;
		}
	}
	return FVector2D(-1.0, -1.0); // 没有找到合适的生成位置
}

bool AGoldCoinSpawner::DeferSpawnBarriers(TArrayView<RandomPoint> Positions, FInt32Point Tile, FVector2D UVPos, AWorldGenerator* WorldGenerator)
{
	auto DepentTile = FInt32Point(Tile.X + 1, Tile.Y);
	if (!WorldGenerator->IsValidTile(DepentTile))
	{
		return false;
	}
	SpawnDeferredBarriers(Positions, Tile, UVPos, WorldGenerator);
	return true;
}

bool AGoldCoinSpawner::CanGenerateCloudTrace(AWorldGenerator* WorldGenerator, double TilePos) const
{
	// 在地图里比了一下，当前的参数差不多一个是从道具生成到云结束会占据一个 Tile
	constexpr double HalfSize = 0.5;
	auto CenterPos = FMath::Fmod(TilePos + HalfSize, double(WorldGenerator->MoveOriginXTile));
	for (auto LaserPos : WorldGenerator->SpecialLaserPos)
	{
		// 激光位置不允许生成障碍物
		auto Distance = FMath::Abs(LaserPos - CenterPos);
		if (Distance < HalfSize || Distance > (double(WorldGenerator->MoveOriginXTile) - HalfSize))
		{
			return false;
		}
	}
	return true;
}

void AGoldCoinSpawner::GenerateCloudTrace(FVector ItemPos, FInt32Point Tile)
{
	TArray<struct FOverlapResult> OutOverlaps;
	auto CloudPos = ItemPos + CloudOffset;
	GetWorld()->OverlapMultiByChannel(OutOverlaps, CloudPos, FQuat::Identity, ECollisionChannel::ECC_WorldStatic, FCollisionShape::MakeBox(BoxExtent), FCollisionQueryParams::DefaultQueryParam);

	for (const auto& Overlap : OutOverlaps)
	{
		auto* OverlappedActor = Overlap.GetActor();
		if (OverlappedActor && ClassToRemoveWhenOverlap.Contains(OverlappedActor->GetClass()))
		{
			UE_LOG(LogBarrierSpawner, Log, TEXT("AGoldCoinSpawner::GenerateCloudTrace: Remove overlapping actor %s at %s"), *OverlappedActor->GetName(), *OverlappedActor->GetActorLocation().ToString());
			OverlappedActor->Destroy();
		}
	}
	// DrawDebugBox(GetWorld(), CloudPos, BoxExtent, FColor::Blue, true, 5.0f);

	auto CloudActor = GetWorld()->SpawnActor<AActor>(CloudClass, CloudPos, FRotator::ZeroRotator);
	// 云和金币匿属于下一个 Tile
	SpawnedBarriers.Add(FInt32Point(Tile.X + 1, Tile.Y), CloudActor);
	auto CurrentCoinPos = CloudPos + CoinOffsetInCloud;
	auto Offset = MaxWalkingSpeed * SpawnTimeInterval;
	for (auto i = 0; i < CoinNumberInCloud; ++i)
	{
		auto CoinActor = GetWorld()->SpawnActor<AActor>(BarrierClass, CurrentCoinPos, FRotator::ZeroRotator);
		SpawnedBarriers.Add(FInt32Point(Tile.X + 1, Tile.Y), CoinActor);
		CurrentCoinPos.X += Offset;
	}
}