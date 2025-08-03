// Copyright 2023, Dumitru Zahvatov, All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/Overlay.h"
#include "Components/Image.h"
#include "Components/VerticalBox.h"
#include "Components/SizeBox.h"

#include "Components/Border.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Animation/WidgetAnimation.h"
#include "HealthBarWidget.generated.h"


UENUM()
enum class EHealthBarTextFormat : uint8
{
	/** Display current value only. */
	Current,
	/** Display max value only. */
	Max,
	/** Display current and max values, ex. 50/100. */
	CurrentMax,
	/** Display percentage, ex. 50%. */
	Percent,
	/** Display any custom text. */
	CustomText
};

UENUM()
enum class EHealthBarTextPosition : uint8
{
	/** Place text inside of the health bar. */
	Inside,
	/** Place text outside of the health bar. Left and right text are placed to the left and right side of the health bar, middle text is placed above. */
	Outside
};

UENUM()
enum class EHealthBarAnimationType : uint8
{
	/** No animation will occur. */
	None,
	/** The progress bar will flash with a specified color and the animated part will fade out. */
	Flash,
	/** The progress bar will flash with a specified color and the animated part will move from the previous value to the new value. */
	CatchUp,
	/** The animated part will ease in/out. */
	Fade
};

UENUM()
enum class EHealthBarStripesType : uint8
{
	/** No stripes */
	None,
	/** Divide progress bar in the exact number of sections. */
	ExactNumber,
	/** Number of sections depends on value per section. */
	ValueBased
};


USTRUCT(BlueprintType)
struct FHealthBarTextSetup
{
	GENERATED_BODY()

public:
	/** Is this text block visible? */
	UPROPERTY(EditAnywhere, Category = "HealthBar|Text", meta = (DisplayName = "Is Visible"))
	bool bVisible;

	/** The color of the text block. */
	UPROPERTY(EditAnywhere, Category = "HealthBar|Text", AdvancedDisplay, meta = (DisplayName = "Color"))
	FLinearColor Color;

	/** The size of the text block. */
	UPROPERTY(EditAnywhere, Category = "HealthBar|Text", AdvancedDisplay, meta = (DisplayName = "Size"))
	int32 Size;

	/** Define whether the text should have an outline. */
	UPROPERTY(EditAnywhere, Category = "HealthBar|Text", meta = (DisplayName = "Outline"))
	bool Outline;

	/** Where the text block should be placed relative to the progress bar. */
	UPROPERTY(EditAnywhere, Category = "HealthBar|Text", meta = (DisplayName = "Position"))
	EHealthBarTextPosition Position;

	/**  The offset of the text block. */
	UPROPERTY(EditAnywhere, Category = "HealthBar|Text", meta = (DisplayName = "Offset"))
	FVector2D Offset;

	/** Specify how the text should be displayed. */
	UPROPERTY(EditAnywhere, Category = "HealthBar|Text", meta = (DisplayName = "Format"))
	EHealthBarTextFormat Format;

	/** Custom text to be displayed if Format is set to "CustomText". */
	UPROPERTY(EditAnywhere, Category = "HealthBar|Text", meta = (EditCondition = "Format == EHealthBarTextFormat::CustomText", EditConditionHides, DisplayName = "Custom Text"))
	FText CustomText;

	FHealthBarTextSetup()
	{
		bVisible = false;
		Color = FColor::White;
		Size = 10;
		Outline = 1;
		Position = EHealthBarTextPosition::Inside;
		Offset = FVector2D(0.0f, 0.0f);
		Format = EHealthBarTextFormat::Percent;
		CustomText = FText();
	}
};


USTRUCT(BlueprintType)
struct FHealthBarAnimationSetup
{
	GENERATED_BODY()

public:
	/** How the progress bar should be animated. */
	UPROPERTY(EditAnywhere, Category = "HealthBar|Animation")
	EHealthBarAnimationType Type;

	/** The color used for animation. Does not work with Fade animation type. */
	UPROPERTY(EditAnywhere, Category = "HealthBar|Animation")
	FLinearColor Color;

	FHealthBarAnimationSetup()
	{
		Type = EHealthBarAnimationType::None;
		Color = FLinearColor(1.0f, 1.0f, 1.0f, 1.0f);
	}
};

USTRUCT(BlueprintType)
struct FAdditionalHealthBarSetup
{
	GENERATED_BODY();

public:
	/** The height of the additional progress bar. The width of the bar will be the same as the main progress bar. */
	UPROPERTY(EditAnywhere, Category = "HealthBar|Additional Bars")
	float Height;

	/** The maximum value of progress bar. */
	UPROPERTY(EditAnywhere, Category = "HealthBar|Additional Bars")
	float MaxValue;

	/** The current value of progress bar. */
	UPROPERTY(EditAnywhere, Category = "HealthBar|Additional Bars")
	float CurrentValue;

	/** The texture used for progress bar background. */
	UPROPERTY(EditAnywhere, Category = "HealthBar|Additional Bars")
	UTexture* Texture;

	/** The color of the background. */
	UPROPERTY(EditAnywhere, Category = "HealthBar|Additional Bars")
	FLinearColor Color;

	FAdditionalHealthBarSetup()
	{
		Height = 8.0f;
		MaxValue = 100.0f;
		CurrentValue = 100.0f;
		Texture = nullptr;
		Color = FLinearColor(1.0f, 1.0f, 1.0f, 1.0f);
	}
};

USTRUCT(BlueprintType)
struct FRunnerAdditionalHealthBarSetup
{
	GENERATED_BODY();

public:
	/** The height of the additional progress bar. The width of the bar will be the same as the main progress bar. */
	UPROPERTY(EditAnywhere, Category = "HealthBar|Additional Bars")
	float Height;

	/** The maximum value of progress bar. */
	UPROPERTY(EditAnywhere, Category = "HealthBar|Additional Bars")
	float MaxValue;

	/** The current value of progress bar. */
	UPROPERTY(EditAnywhere, Category = "HealthBar|Additional Bars")
	float CurrentValue;

	/** The texture used for progress bar background. */
	UPROPERTY(EditAnywhere, Category = "HealthBar|Additional Bars")
	UTexture* Texture;

	/** The color of the background. */
	UPROPERTY(EditAnywhere, Category = "HealthBar|Additional Bars")
	FLinearColor Color;

	UPROPERTY(EditAnywhere, Category = "HealthBar|Additional Bars")
	FHealthBarAnimationSetup DamageAnimationType;	

	UPROPERTY(EditAnywhere, Category = "HealthBar|Additional Bars")
	FHealthBarAnimationSetup HealAnimationType;

	FRunnerAdditionalHealthBarSetup()
	{
		Height = 8.0f;
		MaxValue = 100.0f;
		CurrentValue = 100.0f;
		Texture = nullptr;
		Color = FLinearColor(1.0f, 1.0f, 1.0f, 1.0f);
		DamageAnimationType = FHealthBarAnimationSetup();
		HealAnimationType = FHealthBarAnimationSetup();
	}
};

/**
* Store a pointer to additional progress bar
* As well as current and maximum value
* We can access additonal bars as array elements later to change their values
*/
USTRUCT()
struct FAdditionalHealthBar
{
	GENERATED_BODY();

public:
	UProgressBar* ProgressBar;
	float Max;
	float Current;

	FAdditionalHealthBar()
	{
		ProgressBar = nullptr;
		Max = 0.0f;
		Current = 0.0f;
	}
};


/**
 * 
 */
UCLASS(Blueprintable)
class SIMPLEHEALTHBAR_API UHealthBarWidget : public UUserWidget
{
	GENERATED_BODY()
	
public:
	/**
	* Setup section
	* These methods are called once on the bootstrap phase
	*/
	void SetWidgetSize(const FVector2D& Size, int32 BorderWidth);
	UFUNCTION(BlueprintCallable, Category = "HealthBar|Setup")
	void SetProgressBarAppearance(UTexture* ProgressBarTexture, const FLinearColor& ProgressBarColor, const FHealthBarAnimationSetup& DamageAnimationSetup, const FHealthBarAnimationSetup& HealAnimationSetup);
	void SetBorderColor(const FLinearColor& Color);
	void SetLeftTextBlockAppearance(FHealthBarTextSetup& SetupData, FVector2D& HealthBarSize, int32 BorderWidth);
	void SetMiddleTextBlockAppearance(FHealthBarTextSetup& SetupData, FVector2D& HealthBarSize, int32 BorderWidth);
	void SetRightTextBlockAppearance(FHealthBarTextSetup& SetupData, FVector2D& HealthBarSize, int32 BorderWidth);
	void SetTextBlockBinding(UTextBlock* TextBlock, FHealthBarTextSetup SetupData);

	UFUNCTION(BlueprintCallable, Category = "HealthBar|Setup")
	void SetupAdditionalBars(const TArray<FAdditionalHealthBarSetup>& SetupData);
	UFUNCTION(BlueprintCallable, Category = "HealthBar|Setup")
	void SetupRunnerAdditionalBars(const TArray<FRunnerAdditionalHealthBarSetup>& SetupData);
	// Setup section END


	void SetMaxValue(const float MaxValue);
	void AdditionalBarSetMaxValue(const float MaxValue, const int AdditionalBarIndex);
	UFUNCTION(BlueprintCallable, Category = "HealthBar|Setup")
	void SetCurrentValue(const float CurrentValue);
	UFUNCTION(BlueprintCallable, Category = "HealthBar|Setup")
	void AdditionalBarSetCurrentValue(const float CurrentValue, const int AdditionalBarIndex);
	UFUNCTION(BlueprintCallable, Category = "HealthBar|Setup")
	void RunnerAdditionalBarSetCurrentValue(const float CurrentValue, const int AdditionalBarIndex, const bool bAnimate);
	UFUNCTION(BlueprintCallable, Category = "HealthBar|Setup")
	void SetMaxAndCurrentValues(const float MaxValue, const float CurrentValue, bool bAnimation = true);
	void AdditionalBarSetMaxAndCurrentValues(const float MaxValue, const float CurrentValue, const int AdditionalBarIndex);
	void Add(const float Amount);
	void AdditionalBarAdd(const float Amount, const int AdditionalBarIndex);
	void Subtract(const float Amount);
	void AdditionalBarSubtract(const float Amount, const int AdditionalBarIndex);
	void SetLeftCustomText(FText Text);
	void SetMiddleCustomText(FText Text);
	void SetRightCustomText(FText Text);
	void SetOpacity(float InOpacity);
	void SetFillColor(FLinearColor& InColor);
	void SetStripes(EHealthBarStripesType& InStripesType, int32 InNumberOfStripedSections, float InValuePerStripedSection);

protected:
	virtual void NativeOnInitialized();

private:
	/** Widgets */

	UPROPERTY(Transient, Meta = (BindWidget))
	UOverlay* Overlay;

	UPROPERTY(Transient, Meta = (BindWidget))
	UImage* BorderImage;

	UPROPERTY(Transient, Meta = (BindWidget))
	UVerticalBox* VerticalBox;
	
	UPROPERTY(Transient, Meta = (BindWidget))
	USizeBox* MainSizeBox;

	UPROPERTY(Transient, Meta = (BindWidget))
	UProgressBar* ProgressBar;

	UPROPERTY(Transient, Meta = (BindWidget))
	UTextBlock* LeftTextBlock;

	UPROPERTY(Transient, Meta = (BindWidget))
	UTextBlock* MiddleTextBlock;

	UPROPERTY(Transient, Meta = (BindWidget))
	UTextBlock* RightTextBlock;

	UPROPERTY(Transient, Meta = (BindWidgetAnim))
	UWidgetAnimation* OnDamageAnim;

	UPROPERTY(Transient, Meta = (BindWidgetAnim))
	UWidgetAnimation* OnHealingAnim;


	
	/** Assets */

	UPROPERTY(EditDefaultsOnly, Category = "HealthBar")
	UMaterialInterface* DefaultMaterial;

	UPROPERTY(EditDefaultsOnly, Category = "HealthBar")
	UMaterialInterface* FlashMaterial;

	UPROPERTY(EditDefaultsOnly, Category = "HealthBar")
	UMaterialInterface* CatchUpMaterial;

	UPROPERTY(EditDefaultsOnly, Category = "HealthBar")
	UMaterialInterface* FadeMaterial;



	/**
	* Bindings
	* Used to bind text block values other than CustomText
	*/

	UFUNCTION()
	FText CurrentValueBinding();

	UFUNCTION()
	FText MaxValueBinding();

	UFUNCTION()
	FText CurrentMaxValueBinding();

	UFUNCTION()
	FText PercentBinding();



	float Max;
	float Current;
	UMaterialInstanceDynamic* BackgroundMaterial;
	UMaterialInstanceDynamic* FillMaterial;
	FHealthBarAnimationSetup DamageAnimation;
	FHealthBarAnimationSetup HealAnimation;
	TArray<FAdditionalHealthBar> AdditionalBars;
	TArray<FHealthBarAnimationSetup> AdditionalBarsDamageAnimations;
	TArray<FHealthBarAnimationSetup> AdditionalBarsHealAnimations;

	void UpdateProgressBarValue(bool bAnimate = true);
	void UpdateAdditionalProgressBarValue(FAdditionalHealthBar& AdditionalBar);
	void PlayDamageAnimation(float PreviousPercentage, float NewPercentage);
	void PlayHealAnimation(float PreviousPercentage, float NewPercentage);

	UMaterialInterface* GetHealthBarMaterialAsset(FHealthBarAnimationSetup SetupData);

	
};
