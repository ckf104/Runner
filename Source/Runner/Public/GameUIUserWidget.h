// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Components/Image.h"
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Math/MathFwd.h"
#include "UObject/ObjectPtr.h"
#include "GameUIUserWidget.generated.h"

/**
 *
 */
UCLASS()
class RUNNER_API UGameUIUserWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI")
	float GasChangeSpeed = 1.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI")
	float LifeChangeSpeed = 1.0f;

	UPROPERTY(EditAnywhere, Category = "Score")
	int32 CoinScore = 25;

	UPROPERTY(EditAnywhere, Category = "Score")
	int32 ScorePerSecond = 100;

	float ScoreTime = 0.0f;

	static FVector3f GetParamsPercentage(float Percentage);

	void SetGameStart(bool bStart) { bGameStart = bStart; }
	void AddOneCoin() { AddScore(CoinScore); }
	void AddScore(uint32 ScoreToAdd);
	uint32 GetTotalScore() const { return TotalScore; }
	void UpdateLife(float Percentage);
	void UpdateGas(float Percentage);
	void AddGas(float Percentage) { UpdateGas(GasRealPercentage + Percentage); }
	void UpdateLifeImmediately(float Percentage);
	void UpdateGasImmediately(float Percentage);

private:
	UPROPERTY(Transient, Meta = (BindWidget))
	TObjectPtr<class UImage> Life0;

	UPROPERTY(Transient, Meta = (BindWidget))
	TObjectPtr<class UImage> Life1;

	UPROPERTY(Transient, Meta = (BindWidget))
	TObjectPtr<class UImage> Life2;

	UPROPERTY(Transient, Meta = (BindWidget))
	TObjectPtr<class UImage> Gas0;

	UPROPERTY(Transient, Meta = (BindWidget))
	TObjectPtr<class UImage> Gas1;

	UPROPERTY(Transient, Meta = (BindWidget))
	TObjectPtr<class UImage> Gas2;

	UPROPERTY(Transient, Meta = (BindWidget))
	TObjectPtr<class UTextBlock> Score;

	uint32 PendingScore = 0;
	uint32 TotalScore = 0;

	float LifeShowPercentage = 0.0f;
	float LifeRealPercentage = 0.0f;

	float GasShowPercentage = 0.0f;
	float GasRealPercentage = 0.0f;

	bool bGameStart = false;

	class UMaterialInstanceDynamic* Life0Material = nullptr;
	class UMaterialInstanceDynamic* Life1Material = nullptr;
	class UMaterialInstanceDynamic* Life2Material = nullptr;
	class UMaterialInstanceDynamic* Gas0Material = nullptr;
	class UMaterialInstanceDynamic* Gas1Material = nullptr;
	class UMaterialInstanceDynamic* Gas2Material = nullptr;

protected:
	void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	void NativeConstruct() override;
};
