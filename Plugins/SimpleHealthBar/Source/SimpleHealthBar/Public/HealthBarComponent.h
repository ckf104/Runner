// Copyright 2023, Dumitru Zahvatov, All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/WidgetComponent.h"
#include "HealthBarWidget.h"
#include "HealthBarComponent.generated.h"

/**
 * 
 */
UCLASS(Blueprintable, hidecategories = (Object, Activation, "Components|Activation", Sockets, Base, Lighting, LOD, Mesh), editinlinenew)
class SIMPLEHEALTHBAR_API UHealthBarComponent : public UWidgetComponent
{
	GENERATED_BODY()

public:
	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction) override;

	UHealthBarComponent(const FObjectInitializer& PCIP);

	/** The base size of the widget. */
	UPROPERTY(EditAnywhere, Category = "HealthBar", meta = (DisplayName = "Size"))
	FVector2D Size;

	/** Maximum value of the progress bar. */
	UPROPERTY(EditAnywhere, Category = "HealthBar", meta = (DisplayName = "Max Value", ClampMin=0.0f))
	float Max;

	/** Current value of the progress bar. */
	UPROPERTY(EditAnywhere, Category = "HealthBar", meta = (DisplayName = "Current Value", ClampMin=0.0f))
	float Current;

	/** The texture used for the background of the progress bar. */
	UPROPERTY(EditAnywhere, Category = "HealthBar|Background", meta = (DisplayName = "Texture"))
	UTexture* ProgressBarTexture;

	/** Background tint color of the progress bar. */
	UPROPERTY(EditAnywhere, Category = "HealthBar|Background", meta = (DisplayName = "Color"))
	FLinearColor ProgressBarColor;

	/** How the progress bar should be divided into sections. */
	UPROPERTY(EditAnywhere, Category = "HealthBar|Background", meta = (DisplayName = "Stripes Type"))
	EHealthBarStripesType StripesType;

	/** Exact number of striped sections that should be drawn. */
	UPROPERTY(EditAnywhere, Category = "HealthBar|Background", meta = (DisplayName = "Number of Sections", EditCondition = "StripesType == EHealthBarStripesType::ExactNumber", EditConditionHides))
	int32 NumberOfStripedSections;

	/** The value per striped section. The amount of stripes will be calculated based on the Max Value. */
	UPROPERTY(EditAnywhere, Category = "HealthBar|Background", meta = (DisplayName = "Value per Section", EditCondition = "StripesType == EHealthBarStripesType::ValueBased", EditConditionHides))
	float ValuePerStripedSection;

	/** The width of the border in pixels. It will be added to the widget size. */
	UPROPERTY(EditAnywhere, Category = "HealthBar|Border", meta = (DisplayName = "Width", ClampMin = 0.0f))
	int32 BorderWidth;

	/** The color of the border. */
	UPROPERTY(EditAnywhere, Category = "HealthBar|Border", meta = (DisplayName = "Color"))
	FLinearColor BorderColor;

	/** The distance where the widget is hidden, with a value of 0 meaning the widget is always visible. */
	UPROPERTY(EditAnywhere, Category = "HealthBar|Visibility", meta = (DisplayName = "Visible Distance", ClampMin=0.0f))
	float MaxDistance;

	/** The default opacity for the widget, with a minimum value of 0 and a maximum value of 1. */
	UPROPERTY(EditAnywhere, Category = "HealthBar|Visibility", meta = (DisplayName = "Base Opacity", ClampMin=0.0f, ClampMax=1.0f))
	float BaseOpacity;

	/** The range where the widget opacity will fade out from BaseOpacity to 0. */
	UPROPERTY(EditAnywhere, Category = "HealthBar|Visibility", meta = (DisplayName = "Opacity Falloff Distance"))
	FVector2D OpacityFalloffDistance;

	/** Whether this widget should be occluded when the owning actor is occluded. */
	UPROPERTY(EditAnywhere, Category = "HealthBar|Visibility", meta = (DisplayName = "Occlusion"))
	bool bOcclusion;

	/** The configuration of the text block. */
	UPROPERTY(EditAnywhere, Category = "HealthBar|Text", meta = (DisplayName = "Left Text"))
	FHealthBarTextSetup LeftTextBlockSetup;

	/** The configuration of the middle text block. */
	UPROPERTY(EditAnywhere, Category = "HealthBar|Text", meta = (DisplayName = "Middle Text"))
	FHealthBarTextSetup MiddleTextBlockSetup;

	/** The configuration of the right text block. */
	UPROPERTY(EditAnywhere, Category = "HealthBar|Text", meta = (DisplayName = "Right Text"))
	FHealthBarTextSetup RightTextBlockSetup;

	/** The animation that plays when the progress bar value decreases (i.e. the bar represents damage). */
	UPROPERTY(EditAnywhere, Category = "Healthbar|Animation", meta = (DisplayName = "Damage Animation"))
	FHealthBarAnimationSetup DamageAnimationSetup;

	/** The animation that plays when the progress bar value increases (i.e. the bar represents healing). */
	UPROPERTY(EditAnywhere, Category = "Healthbar|Animation", meta = (DisplayName = "Heal Animation"))
	FHealthBarAnimationSetup HealAnimationSetup;

	/** Setup additional progress bars. */
	UPROPERTY(EditAnywhere, Category = "HealthBar|Additional Bars", meta = (DisplayName = "Additional Bars"))
	TArray<FAdditionalHealthBarSetup> AdditionalBars;
	


	/** Sets the maximum value of the main progress bar. */
	UFUNCTION(BlueprintCallable, NetMulticast, Reliable, Category = "HealthBar")
	void SetMaxValue(const float MaxValue);
	
	/** Sets the maximum value of an additional progress bar by specified index. */
	UFUNCTION(BlueprintCallable, NetMulticast, Reliable, Category = "HealthBar")
	void AdditionalBarSetMaxValue(const float MaxValue, const int AdditionalBarIndex);

	/** Sets the current value of the main progress bar. */
	UFUNCTION(BlueprintCallable, NetMulticast, Reliable, Category = "HealthBar")
	void SetCurrentValue(const float CurrentValue);

	/** Sets the current value of an additional progress bar by specified index. */
	UFUNCTION(BlueprintCallable, NetMulticast, Reliable, Category = "HealthBar")
	void AdditionalBarSetCurrentValue(const float CurrentValue, const int AdditionalBarIndex);

	/** Sets maximum and current values of the main progress bar. */
	UFUNCTION(BlueprintCallable, NetMulticast, Reliable, Category = "HealthBar")
	void SetMaxAndCurrentValues(const float MaxValue, const float CurrentValue);

	/** Sets maximum and current values of an additional progress bar by specified index. */
	UFUNCTION(BlueprintCallable, NetMulticast, Reliable, Category = "HealthBar")
	void AdditionalBarSetMaxAndCurrentValues(const float MaxValue, const float CurrentValue, const int AdditionalBarIndex);

	/** Adds value to the current value of the main progress bar. */
	UFUNCTION(BlueprintCallable, NetMulticast, Reliable, Category = "HealthBar")
	void Add(const float Amount);

	/** Adds value to the current value of an additional progress bar by specified index. */
	UFUNCTION(BlueprintCallable, NetMulticast, Reliable, Category = "HealthBar")
	void AdditionalBarAdd(const float Amount, const int AdditionalBarIndex);

	/** Subtracts value from the current value of the main progress bar. */
	UFUNCTION(BlueprintCallable, NetMulticast, Reliable, Category = "HealthBar")
	void Subtract(const float Amount);

	/** Subtracts value from the current value of an additional progress bar by specified index. */
	UFUNCTION(BlueprintCallable, NetMulticast, Reliable, Category = "HealthBar")
	void AdditionalBarSubtract(const float Amount, const int AdditionalBarIndex);

	/** Sets the text for the left text block if it is set to display custom text. */
	UFUNCTION(BlueprintCallable, NetMulticast, Reliable, Category = "HealthBar")
	void SetLeftCustomText(const FText& Text);

	/** Sets the text for the middle text block if it is set to display custom text. */
	UFUNCTION(BlueprintCallable, NetMulticast, Reliable, Category = "HealthBar")
	void SetMiddleCustomText(const FText& Text);

	/** Sets the text for the middle text block if it is set to display custom text. */
	UFUNCTION(BlueprintCallable, NetMulticast, Reliable, Category = "HealthBar")
	void SetRightCustomText(const FText& Text);

	/** Sets the color of the border. */
	UFUNCTION(BlueprintCallable, Category = "HealthBar")
	void SetBorderColor(FLinearColor InColor);

	/** Sets the max distance of the widget */
	UFUNCTION(BlueprintCallable, Category = "HealthBar")
	void SetMaxDistance(float InMaxDistance);

	/** Sets the opacity of the widget. */
	UFUNCTION(BlueprintCallable, Category = "HealthBar")
	void SetOpacity(float InOpacity);

	/** Sets the opacity falloff distance of the widget. */
	UFUNCTION(BlueprintCallable, Category = "HealthBar")
	void SetOpacityFalloffDistance(FVector2D InOpacityFalloffDistance);

	/** Sets whether the widget should be occluded when the owning actor is occluded. */
	UFUNCTION(BlueprintCallable, Category = "HealthBar")
	void SetOcclusion(bool InOcclusion);

	/** Sets the fill collor of the main progress bar. */
	UFUNCTION(BlueprintCallable, Category = "HealthBar")
	void SetFillColor(FLinearColor InColor);

	/** Setup stripes for the progress bar. */
	UFUNCTION(BlueprintCallable, Category = "HealthBar")
	void SetStripes(EHealthBarStripesType InStripesType, int32 InNumberOfStripedSections, float InValuePerStripedSection);

	/** Gets the number of additional progress bars. */
	UFUNCTION(BlueprintPure, Category = "HealthBar")
	int GetAdditionalBarsNumber() const { return AdditionalBars.Num(); }

protected:
	virtual void InitWidget();



private:
	UHealthBarWidget* HealthBarWidget;

	bool CalculateVisibility();
};
