// Fill out your copyright notice in the Description page of Project Settings.

#include "GameUIUserWidget.h"
#include "Components/TextBlock.h"
#include "EngineUtils.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Math/MathFwd.h"
#include "WorldGenerator.h"

FVector3f UGameUIUserWidget::GetParamsPercentage(float Percentage)
{
	float LifeP0, LifeP1, LifeP2;
	float T1 = 1.0f / 3.0f;
	float T2 = 2.0f / 3.0f;
	if (Percentage <= T1)
	{
		LifeP0 = Percentage * 3.0f;
		LifeP1 = 0.0f;
		LifeP2 = 0.0f;
	}
	else if (Percentage <= T2)
	{
		LifeP0 = 1.0f;
		LifeP1 = (Percentage - T1) * 3.0f;
		LifeP2 = 0.0f;
	}
	else
	{
		LifeP0 = 1.0f;
		LifeP1 = 1.0f;
		LifeP2 = FMath::Clamp((Percentage - T2) * 3.0f, 0.0f, 1.0f);
	}
	return FVector3f(LifeP0, LifeP1, LifeP2);
}

void UGameUIUserWidget::UpdateLife(float Percentage)
{
	LifeRealPercentage = Percentage;
	FVector3f LifeParams = GetParamsPercentage(Percentage);
	auto LifeP0 = LifeParams.X;
	auto LifeP1 = LifeParams.Y;
	auto LifeP2 = LifeParams.Z;

	Life0Material->SetScalarParameterValue("RealPercentage", LifeP0);
	Life1Material->SetScalarParameterValue("RealPercentage", LifeP1);
	Life2Material->SetScalarParameterValue("RealPercentage", LifeP2);
}

void UGameUIUserWidget::UpdateLifeImmediately(float Percentage)
{
	LifeShowPercentage = Percentage;
	LifeRealPercentage = Percentage;
	auto LifeParams = GetParamsPercentage(Percentage);
	auto LifeP0 = LifeParams.X;
	auto LifeP1 = LifeParams.Y;
	auto LifeP2 = LifeParams.Z;
	Life0Material->SetScalarParameterValue("RealPercentage", LifeP0);
	Life1Material->SetScalarParameterValue("RealPercentage", LifeP1);
	Life2Material->SetScalarParameterValue("RealPercentage", LifeP2);
	Life0Material->SetScalarParameterValue("ShowPercentage", LifeP0);
	Life1Material->SetScalarParameterValue("ShowPercentage", LifeP1);
	Life2Material->SetScalarParameterValue("ShowPercentage", LifeP2);
}

void UGameUIUserWidget::UpdateGas(float Percentage)
{
	Percentage = FMath::Clamp(Percentage, 0.0f, 1.0f);
	GasRealPercentage = Percentage;
	FVector3f GasParams = GetParamsPercentage(Percentage);
	auto GasP0 = GasParams.X;
	auto GasP1 = GasParams.Y;
	auto GasP2 = GasParams.Z;

	Gas0Material->SetScalarParameterValue("RealPercentage", GasP0);
	Gas1Material->SetScalarParameterValue("RealPercentage", GasP1);
	Gas2Material->SetScalarParameterValue("RealPercentage", GasP2);
}

void UGameUIUserWidget::UpdateGasImmediately(float Percentage)
{
	GasShowPercentage = Percentage;
	GasRealPercentage = Percentage;
	auto GasParams = GetParamsPercentage(Percentage);
	auto GasP0 = GasParams.X;
	auto GasP1 = GasParams.Y;
	auto GasP2 = GasParams.Z;
	Gas0Material->SetScalarParameterValue("RealPercentage", GasP0);
	Gas1Material->SetScalarParameterValue("RealPercentage", GasP1);
	Gas2Material->SetScalarParameterValue("RealPercentage", GasP2);
	Gas0Material->SetScalarParameterValue("ShowPercentage", GasP0);
	Gas1Material->SetScalarParameterValue("ShowPercentage", GasP1);
	Gas2Material->SetScalarParameterValue("ShowPercentage", GasP2);
}

void UGameUIUserWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if  (GetWorld()->IsPaused())
	{
		return; // 如果游戏暂停，则不更新 UI
	}

	if (bGameStart)
	{
		ScoreTime += InDeltaTime;
	}
	if (ScoreTime >= 1.0f)
	{
		ScoreTime -= 1.0f;
		auto Difficulty = WorldGenerator->CurrentDifficulty;
		Difficulty = FMath::Clamp(Difficulty, 0, ScorePerSecond.Num() - 1);
		AddScore(ScorePerSecond[Difficulty]);
	}

	if (PendingScore > 0)
	{
		TotalScore += PendingScore;
		Score->SetText(FText::AsNumber(TotalScore));
		PendingScore = 0;
	}
	if (PendingLife != 0.0f)
	{
		auto NewLife = FMath::Clamp(LifeRealPercentage + PendingLife, 0.0f, 1.0f);
		PendingLife = 0.0f;
		UpdateLife(NewLife);
	}
	if (LifeRealPercentage != LifeShowPercentage)
	{
		LifeShowPercentage = FMath::FInterpConstantTo(LifeShowPercentage, LifeRealPercentage, InDeltaTime, LifeChangeSpeed);
		auto LifeParams = GetParamsPercentage(LifeShowPercentage);
		auto LifeP0 = LifeParams.X;
		auto LifeP1 = LifeParams.Y;
		auto LifeP2 = LifeParams.Z;
		Life0Material->SetScalarParameterValue("ShowPercentage", LifeP0);
		Life1Material->SetScalarParameterValue("ShowPercentage", LifeP1);
		Life2Material->SetScalarParameterValue("ShowPercentage", LifeP2);
	}
	if (GasRealPercentage != GasShowPercentage)
	{
		GasShowPercentage = FMath::FInterpConstantTo(GasShowPercentage, GasRealPercentage, InDeltaTime, GasChangeSpeed);
		auto GasParams = GetParamsPercentage(GasShowPercentage);
		auto GasP0 = GasParams.X;
		auto GasP1 = GasParams.Y;
		auto GasP2 = GasParams.Z;
		Gas0Material->SetScalarParameterValue("ShowPercentage", GasP0);
		Gas1Material->SetScalarParameterValue("ShowPercentage", GasP1);
		Gas2Material->SetScalarParameterValue("ShowPercentage", GasP2);
	}
}

void UGameUIUserWidget::NativeConstruct()
{
	Super::NativeConstruct();

	auto* Mat = Cast<UMaterialInterface>(Life0->GetBrush().GetResourceObject());
	if (Mat)
	{
		Life0Material = UMaterialInstanceDynamic::Create(Mat, this);
		Life0->SetBrushFromMaterial(Life0Material);
	};
	Mat = Cast<UMaterialInterface>(Life1->GetBrush().GetResourceObject());
	if (Mat)
	{
		Life1Material = UMaterialInstanceDynamic::Create(Mat, this);
		Life1->SetBrushFromMaterial(Life1Material);
	}
	Mat = Cast<UMaterialInterface>(Life2->GetBrush().GetResourceObject());
	if (Mat)
	{
		Life2Material = UMaterialInstanceDynamic::Create(Mat, this);
		Life2->SetBrushFromMaterial(Life2Material);
	}
	Mat = Cast<UMaterialInterface>(Gas0->GetBrush().GetResourceObject());
	if (Mat)
	{
		Gas0Material = UMaterialInstanceDynamic::Create(Mat, this);
		Gas0->SetBrushFromMaterial(Gas0Material);
	}
	Mat = Cast<UMaterialInterface>(Gas1->GetBrush().GetResourceObject());
	if (Mat)
	{
		Gas1Material = UMaterialInstanceDynamic::Create(Mat, this);
		Gas1->SetBrushFromMaterial(Gas1Material);
	}
	Mat = Cast<UMaterialInterface>(Gas2->GetBrush().GetResourceObject());
	if (Mat)
	{
		Gas2Material = UMaterialInstanceDynamic::Create(Mat, this);
		Gas2->SetBrushFromMaterial(Gas2Material);
	}

	TActorIterator<AWorldGenerator> It(GetWorld());
	if (It)
	{
		WorldGenerator = *It;
	}

	ScorePerSecond.SetNum(5);
	ScorePerSecond[0] = 100;
	ScorePerSecond[1] = 125;
	ScorePerSecond[2] = 150;
	ScorePerSecond[3] = 175;
	ScorePerSecond[4] = 200;
}
