// Fill out your copyright notice in the Description page of Project Settings.

#include "BarrierSpawner.h"
#include "Math/RotationMatrix.h"
#include "Math/UnrealMathUtility.h"
#include "WorldGenerator.h"

// Sets default values
ABarrierSpawner::ABarrierSpawner()
{
	// 默认情况下不需要 tick
	PrimaryActorTick.bCanEverTick = false;
	PrimaryActorTick.bStartWithTickEnabled = false;
}

FRotator ABarrierSpawner::GetRotationFromSeed(FRotator Seed) const
{
	FRotator Result = FRotator::ZeroRotator;
	Result.Yaw = Seed.Yaw * 360.0; // 将 0 - 1 的随机变量转换为 0 - 360 度
	return Result;
}

void ABarrierSpawner::GetTransformFromSeed(FTransform& OutTransform, const RandomPoint& Seed, FInt32Point Tile, AWorldGenerator* WorldGenerator) const
{
	// 重映射 UV，避免生成在边缘位置
	FVector2D UVPos = FVector2D::ZeroVector;
	UVPos.X = FMath::Lerp(0.01, 0.99, Seed.UVPos.X);
	UVPos.Y = FMath::Lerp(0.01, 0.99, Seed.UVPos.Y);

	auto Location = WorldGenerator->GetVisualWorldPositionFromUV(UVPos, Tile);
	auto Rotation = GetRotationFromSeed(Seed.Rotation);
	TransformAlign(Location, Rotation, WorldGenerator);
	OutTransform.SetLocation(Location);
	OutTransform.SetRotation(Rotation.Quaternion());
}

void ABarrierSpawner::TransformAlign(FVector& Location, FRotator& Rotation, class AWorldGenerator* WorldGenerator) const
{
	// 对齐到地面法线
	if (AlignMode == EAlignMode::AlignNormal)
	{
		auto Normal = WorldGenerator->GetNormalFromHorizontalPos(FVector2D(Location));
		
		auto ForwardDir = FRotator(0.0, Rotation.Yaw, 0.0).Vector();
		// 取 80 是为了避免超出边界
		auto ForwardPos = Location + ForwardDir * FMath::Min((BarrierRadius / 2), 80.0f);
		
		ForwardPos.Z = WorldGenerator->GetVisualHeightFromHorizontalPos(FVector2D(ForwardPos));
		auto Pitch = (ForwardPos - Location).GetSafeNormal();

		auto AlignedRotation = FRotationMatrix::MakeFromXZ(Pitch, Normal).Rotator();

		Rotation.Pitch += AlignedRotation.Pitch;
		Rotation.Roll += AlignedRotation.Roll;

		Location += Normal * BarrierOffset;
	}
	// 对齐到重力方向
	else if (AlignMode == EAlignMode::AlignGravity)
	{
		// ax + by + cz = 0
		// x^2 + y^2 = Radius^2
		// Z_min = -sqrt((a^2 + b^2)(x^2 + y^2)) / c = -sqrt(1 - Normal.Z^2) * Radius / Normal.Z
		auto Normal = WorldGenerator->GetNormalFromHorizontalPos(FVector2D(Location));

		double Zoffset = 0.0;
		if (Normal.Z > UE_SMALL_NUMBER)
		{
			Zoffset = -FMath::Sqrt((1 - Normal.Z * Normal.Z)) * BarrierRadius / Normal.Z;
		}
		Zoffset += BarrierOffset;

		Location += FVector(0, 0, Zoffset);
	}
	else
	{
		Location += FVector(0, 0, BarrierOffset);
	}
}