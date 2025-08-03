// Copyright 2023, Dumitru Zahvatov, All rights reserved.


#include "HealthBarWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/OverlaySlot.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/PrimitiveComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Styling/SlateTypes.h"
#include "Kismet/GameplayStatics.h"

void UHealthBarWidget::NativeOnInitialized()
{
	Max = 0.0f;
	Current = 0.0f;
	AdditionalBars = TArray<FAdditionalHealthBar>();
}

void UHealthBarWidget::SetWidgetSize(const FVector2D& Size, int32 BorderWidth)
{
	/** Set progress bar height */
	if (MainSizeBox)
	{
		MainSizeBox->SetHeightOverride(Size.Y);
	}

	if (Overlay)
	{
		/*
		* Set Border size and its position inside CanvasPanel
		* Border Width adds up with ProgressBar Size, so total HealthBar Size is larger than defined
		*/
		if (UCanvasPanelSlot* BorderSlot = Cast<UCanvasPanelSlot>(Overlay->Slot))
		{
			const float SizeX = Size.X + BorderWidth * 2;
			const float SizeY = Size.Y + BorderWidth * 2;
			const float OffsetX = -(Size.X / 2 + BorderWidth);
			const float OffsetY = -(Size.Y / 2 + BorderWidth);
			const FVector2D BorderSize = FVector2D(SizeX, SizeY);
			const FVector2D BorderPosition = FVector2D(OffsetX, OffsetY);
			BorderSlot->SetSize(BorderSize);
			BorderSlot->SetPosition(BorderPosition);
		}
	}

	if (VerticalBox)
	{
		if (UOverlaySlot* BoxSlot = Cast<UOverlaySlot>(VerticalBox->Slot))
		{
			BoxSlot->SetPadding(FMargin(BorderWidth));
		}
	}
}

void UHealthBarWidget::SetProgressBarAppearance(UTexture* ProgressBarTexture, const FLinearColor& ProgressBarColor, const FHealthBarAnimationSetup& DamageAnimationSetup, const FHealthBarAnimationSetup& HealAnimationSetup)
{
	DamageAnimation = DamageAnimationSetup;
	HealAnimation = HealAnimationSetup;
	const FLinearColor BackgroundColor = FLinearColor(0.1f, 0.1f, 0.1f, 1.0f);

	/*
	* Create different material instances for background and fill
	* Set default instance parameters
	*/
	if (ProgressBar)
	{
		FProgressBarStyle ProgressBarStyle = ProgressBar->GetWidgetStyle();
		
		/* Create background material */
		UMaterialInterface* BackgroundMaterialAsset = GetHealthBarMaterialAsset(DamageAnimationSetup);
		if (BackgroundMaterialAsset)
		{
			BackgroundMaterial = UMaterialInstanceDynamic::Create(BackgroundMaterialAsset, this);
			if (BackgroundMaterial)
			{
				if (ProgressBarTexture)
				{
					BackgroundMaterial->SetTextureParameterValue("Texture", ProgressBarTexture);
				}
				BackgroundMaterial->SetVectorParameterValue("Tint", BackgroundColor);
				BackgroundMaterial->SetScalarParameterValue("Animation Progress", 1.0f);
				const FLinearColor DamageAnimationColor = DamageAnimationSetup.Type == EHealthBarAnimationType::Fade ? ProgressBarColor : DamageAnimationSetup.Color;
				BackgroundMaterial->SetVectorParameterValue("Animation Color", DamageAnimationColor);

				ProgressBarStyle.BackgroundImage.SetResourceObject(BackgroundMaterial);
			}
		}

		/* Create fill material */
		UMaterialInterface* FillMaterialAsset = GetHealthBarMaterialAsset(HealAnimationSetup);
		if (FillMaterialAsset)
		{
			FillMaterial = UMaterialInstanceDynamic::Create(FillMaterialAsset, this);
			if (FillMaterial)
			{
				if (ProgressBarTexture)
				{
					FillMaterial->SetTextureParameterValue("Texture", ProgressBarTexture);
				}
				FillMaterial->SetVectorParameterValue("Tint", ProgressBarColor);
				FillMaterial->SetScalarParameterValue("Animation Progress", 1.0f);
				FillMaterial->SetScalarParameterValue("Is Progress Inversed", 1.0f);
				const FLinearColor HealAnimationColor = HealAnimationSetup.Type == EHealthBarAnimationType::Fade ? BackgroundColor : HealAnimationSetup.Color;
				FillMaterial->SetVectorParameterValue("Animation Color", HealAnimationColor);

				ProgressBarStyle.FillImage.SetResourceObject(FillMaterial);
			}
		}
		
		ProgressBar->SetWidgetStyle(ProgressBarStyle);
	}
}

void UHealthBarWidget::SetBorderColor(const FLinearColor& Color)
{
	if (BorderImage)
	{
		BorderImage->SetBrushTintColor(Color);
	}
}

void UHealthBarWidget::SetLeftTextBlockAppearance(FHealthBarTextSetup& SetupData, FVector2D& HealthBarSize, int32 BorderWidth)
{
	if (LeftTextBlock)
	{
		LeftTextBlock->SetVisibility(ESlateVisibility::Visible);
		LeftTextBlock->SetColorAndOpacity(SetupData.Color);
		FSlateFontInfo LeftTextBlockFont = LeftTextBlock->GetFont();
		LeftTextBlockFont.Size = SetupData.Size;
		LeftTextBlockFont.OutlineSettings.OutlineSize = SetupData.Outline ? 1 : 0;
		LeftTextBlock->SetFont(LeftTextBlockFont);

		/*
		* Calculate Position
		* Inside example:      [=50%====        ]
		* Outside example: 50% [========        ]
		*/
		const bool bInside = SetupData.Position == EHealthBarTextPosition::Inside;
		if (UCanvasPanelSlot* LeftTextSlot = Cast<UCanvasPanelSlot>(LeftTextBlock->Slot))
		{
			const float OffsetXInside = -HealthBarSize.X / 2;
			const float OffsetXOutside = -HealthBarSize.X / 2 - BorderWidth - 64.0f;
			const float OffsetX = bInside ? OffsetXInside : OffsetXOutside;
			const float OffsetY = -8.0f;
			LeftTextSlot->SetPosition(FVector2D(OffsetX, OffsetY) + SetupData.Offset);
		}
		if (bInside)
		{
			LeftTextBlock->SetJustification(ETextJustify::Left);
		}

		if (SetupData.Format == EHealthBarTextFormat::CustomText)
		{
			SetLeftCustomText(SetupData.CustomText);
		}

		SetTextBlockBinding(LeftTextBlock, SetupData);
	}
}

void UHealthBarWidget::SetMiddleTextBlockAppearance(FHealthBarTextSetup& SetupData, FVector2D& HealthBarSize, int32 BorderWidth)
{
	if (MiddleTextBlock)
	{
		MiddleTextBlock->SetVisibility(ESlateVisibility::Visible);
		MiddleTextBlock->SetColorAndOpacity(SetupData.Color);
		FSlateFontInfo MiddleTextBlockFont = MiddleTextBlock->GetFont();
		MiddleTextBlockFont.Size = SetupData.Size;
		MiddleTextBlockFont.OutlineSettings.OutlineSize = SetupData.Outline ? 1 : 0;
		MiddleTextBlock->SetFont(MiddleTextBlockFont);

		/*
		* Calculate Position
		* Inside example:      [=======50%      ]
		* 
		* Outside example:             50% 
		*                      [========        ]
		*/
		if (UCanvasPanelSlot* MiddleTextSlot = Cast<UCanvasPanelSlot>(MiddleTextBlock->Slot))
		{
			MiddleTextSlot->SetSize(FVector2D(HealthBarSize.X, 16.0f));

			const bool bInside = SetupData.Position == EHealthBarTextPosition::Inside;
			const float OffsetX = -HealthBarSize.X / 2;
			const float OffsetYInside = -8.0f;
			const float OffsetYOutside = (-HealthBarSize.Y / 2) - BorderWidth - 16.0f;
			const float OffsetY = bInside ? OffsetYInside : OffsetYOutside;
			MiddleTextSlot->SetPosition(FVector2D(OffsetX, OffsetY) + SetupData.Offset);
		}

		if (SetupData.Format == EHealthBarTextFormat::CustomText)
		{
			SetMiddleCustomText(SetupData.CustomText);
		}

		SetTextBlockBinding(MiddleTextBlock, SetupData);
	}
}

void UHealthBarWidget::SetRightTextBlockAppearance(FHealthBarTextSetup& SetupData, FVector2D& HealthBarSize, int32 BorderWidth)
{
	if (RightTextBlock)
	{
		RightTextBlock->SetVisibility(ESlateVisibility::Visible);
		RightTextBlock->SetColorAndOpacity(SetupData.Color);
		FSlateFontInfo RightTextBlockFont = RightTextBlock->GetFont();
		RightTextBlockFont.Size = SetupData.Size;
		RightTextBlockFont.OutlineSettings.OutlineSize = SetupData.Outline ? 1 : 0;
		RightTextBlock->SetFont(RightTextBlockFont);

		/*
		* Calculate Position
		* Inside example:  [========    50% ]
		* Outside example: [========        ] 50%
		*/
		const bool bInside = SetupData.Position == EHealthBarTextPosition::Inside;
		if (UCanvasPanelSlot* RightTextSlot = Cast<UCanvasPanelSlot>(RightTextBlock->Slot))
		{
			const float OffsetXInside = HealthBarSize.X / 2 - 64.0f;
			const float OffsetXOutside = (HealthBarSize.X / 2) + BorderWidth;
			const float OffsetX = bInside ? OffsetXInside : OffsetXOutside;
			const float OffsetY = -8.0f;
			RightTextSlot->SetPosition(FVector2D(OffsetX, OffsetY) + SetupData.Offset);
		}
		if (bInside)
		{
			RightTextBlock->SetJustification(ETextJustify::Right);
		}

		if (SetupData.Format == EHealthBarTextFormat::CustomText)
		{
			SetRightCustomText(SetupData.CustomText);
		}

		SetTextBlockBinding(RightTextBlock, SetupData);
	}
}

void UHealthBarWidget::SetTextBlockBinding(UTextBlock* TextBlock, FHealthBarTextSetup SetupData)
{
	if (SetupData.Format == EHealthBarTextFormat::CustomText)
	{
		return;
	}
	if (SetupData.Format == EHealthBarTextFormat::Current)
	{
		TextBlock->TextDelegate.BindUFunction(this, "CurrentValueBinding");
	}
	else if (SetupData.Format == EHealthBarTextFormat::Max)
	{
		TextBlock->TextDelegate.BindUFunction(this, "MaxValueBinding");
	}
	else if (SetupData.Format == EHealthBarTextFormat::CurrentMax)
	{
		TextBlock->TextDelegate.BindUFunction(this, "CurrentMaxValueBinding");
	}
	else if (SetupData.Format == EHealthBarTextFormat::Percent)
	{
		TextBlock->TextDelegate.BindUFunction(this, "PercentBinding");
	}
}

void UHealthBarWidget::SetupAdditionalBars(const TArray<FAdditionalHealthBarSetup>& SetupData)
{
	float AdditionalBorderHeight = 0.0f;
	/**
	* Loop through additional progress bars from setup data
	* For each progress bar create a widget, setup it in the similar way as the main progress bar
	* Store new widgets to access them later
	*/
	for (FAdditionalHealthBarSetup BarSetup : SetupData)
	{
		FAdditionalHealthBar HealthBar = FAdditionalHealthBar();

		/** Create a container */
		USizeBox* AdditionalSizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		if (AdditionalSizeBox)
		{
			if (VerticalBox)
			{
				VerticalBox->AddChild(AdditionalSizeBox);
			}
			AdditionalSizeBox->SetHeightOverride(BarSetup.Height);
			AdditionalBorderHeight += BarSetup.Height;

			/** Create a widget */
			UProgressBar* AdditionalProgressBar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass());
			if (AdditionalProgressBar)
			{
				FProgressBarStyle ProgressBarStyle = AdditionalProgressBar->GetWidgetStyle();

				/* Create background material */
				UMaterialInstanceDynamic* AdditionalBackgroundMaterial = UMaterialInstanceDynamic::Create(DefaultMaterial, this);
				if (AdditionalBackgroundMaterial)
				{
					if (BarSetup.Texture)
					{
						AdditionalBackgroundMaterial->SetTextureParameterValue("Texture", BarSetup.Texture);
					}
					AdditionalBackgroundMaterial->SetVectorParameterValue("Tint", FLinearColor(0.1f, 0.1f, 0.1f, 1.0f));
					ProgressBarStyle.BackgroundImage.SetResourceObject(AdditionalBackgroundMaterial);
					ProgressBarStyle.BackgroundImage.DrawAs = ESlateBrushDrawType::Image;
				}

				/* Create fill material */
				UMaterialInstanceDynamic* AdditionalFillMaterial = UMaterialInstanceDynamic::Create(DefaultMaterial, this);
				if (AdditionalFillMaterial)
				{
					if (BarSetup.Texture)
					{
						AdditionalFillMaterial->SetTextureParameterValue("Texture", BarSetup.Texture);
					}
					AdditionalFillMaterial->SetVectorParameterValue("Tint", BarSetup.Color);
					ProgressBarStyle.FillImage.SetResourceObject(AdditionalFillMaterial);
					ProgressBarStyle.FillImage.DrawAs = ESlateBrushDrawType::Image;
				}
				AdditionalProgressBar->SetWidgetStyle(ProgressBarStyle);

				HealthBar.ProgressBar = AdditionalProgressBar;
				AdditionalSizeBox->AddChild(AdditionalProgressBar);
			}
		}
		const int NewBarIndex = AdditionalBars.Add(HealthBar);
		AdditionalBarSetMaxAndCurrentValues(BarSetup.MaxValue, BarSetup.CurrentValue, NewBarIndex);
	}

	/** 
	* With addition of new progress bars widget height is now larger
	* Add combined height of all new progress bars to the border height
	*/
	if (UCanvasPanelSlot* BorderSlot = Cast<UCanvasPanelSlot>(Overlay->Slot))
	{
		FVector2D BorderSize = BorderSlot->GetSize();
		BorderSize.Y += AdditionalBorderHeight;
		BorderSlot->SetSize(BorderSize);
	}
}

void UHealthBarWidget::SetupRunnerAdditionalBars(const TArray<FRunnerAdditionalHealthBarSetup>& SetupData)
{
	float AdditionalBorderHeight = 0.0f;
	/**
	* Loop through additional progress bars from setup data
	* For each progress bar create a widget, setup it in the similar way as the main progress bar
	* Store new widgets to access them later
	*/
	for (auto BarSetup : SetupData)
	{
		FAdditionalHealthBar HealthBar = FAdditionalHealthBar();

		/** Create a container */
		USizeBox* AdditionalSizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		if (AdditionalSizeBox)
		{
			if (VerticalBox)
			{
				VerticalBox->AddChild(AdditionalSizeBox);
			}
			AdditionalSizeBox->SetHeightOverride(BarSetup.Height);
			AdditionalBorderHeight += BarSetup.Height;

			/** Create a widget */
			UProgressBar* AdditionalProgressBar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass());
			if (AdditionalProgressBar)
			{
				FProgressBarStyle ProgressBarStyle = AdditionalProgressBar->GetWidgetStyle();

				/* Create background material */
				UMaterialInstanceDynamic* AdditionalBackgroundMaterial = UMaterialInstanceDynamic::Create(GetHealthBarMaterialAsset(BarSetup.DamageAnimationType), this);
				if (AdditionalBackgroundMaterial)
				{
					if (BarSetup.Texture)
					{
						AdditionalBackgroundMaterial->SetTextureParameterValue("Texture", BarSetup.Texture);
					}
					AdditionalBackgroundMaterial->SetVectorParameterValue("Tint", FLinearColor(0.1f, 0.1f, 0.1f, 1.0f));
					AdditionalBackgroundMaterial->SetScalarParameterValue("Animation Progress", 1.0f);
					const FLinearColor DamageAnimationColor = BarSetup.DamageAnimationType.Type == EHealthBarAnimationType::Fade ? BarSetup.Color : BarSetup.DamageAnimationType.Color;
					AdditionalBackgroundMaterial->SetVectorParameterValue("Animation Color", DamageAnimationColor);

					ProgressBarStyle.BackgroundImage.SetResourceObject(AdditionalBackgroundMaterial);
					// ProgressBarStyle.BackgroundImage.DrawAs = ESlateBrushDrawType::Image;
				}

				/* Create fill material */
				UMaterialInstanceDynamic* AdditionalFillMaterial = UMaterialInstanceDynamic::Create(GetHealthBarMaterialAsset(BarSetup.HealAnimationType), this);
				if (AdditionalFillMaterial)
				{
					if (BarSetup.Texture)
					{
						AdditionalFillMaterial->SetTextureParameterValue("Texture", BarSetup.Texture);
					}
					AdditionalFillMaterial->SetVectorParameterValue("Tint", BarSetup.Color);
					AdditionalFillMaterial->SetScalarParameterValue("Animation Progress", 1.0f);
					AdditionalFillMaterial->SetScalarParameterValue("Is Progress Inversed", 1.0f);
					const FLinearColor HealAnimationColor = BarSetup.HealAnimationType.Type == EHealthBarAnimationType::Fade ? FLinearColor(0.1f, 0.1f, 0.1f, 1.0f) : BarSetup.HealAnimationType.Color;
					AdditionalFillMaterial->SetVectorParameterValue("Animation Color", HealAnimationColor);
					ProgressBarStyle.FillImage.SetResourceObject(AdditionalFillMaterial);
					// ProgressBarStyle.FillImage.DrawAs = ESlateBrushDrawType::Image;
				}
				AdditionalProgressBar->SetWidgetStyle(ProgressBarStyle);

				HealthBar.ProgressBar = AdditionalProgressBar;
				AdditionalSizeBox->AddChild(AdditionalProgressBar);
			}
		}
		const int NewBarIndex = AdditionalBars.Add(HealthBar);
		AdditionalBarsDamageAnimations.Add(BarSetup.DamageAnimationType);
		AdditionalBarsHealAnimations.Add(BarSetup.HealAnimationType);
		AdditionalBarSetMaxAndCurrentValues(BarSetup.MaxValue, BarSetup.CurrentValue, NewBarIndex);
	}

	/** 
	* With addition of new progress bars widget height is now larger
	* Add combined height of all new progress bars to the border height
	*/
	if (UCanvasPanelSlot* BorderSlot = Cast<UCanvasPanelSlot>(Overlay->Slot))
	{
		FVector2D BorderSize = BorderSlot->GetSize();
		BorderSize.Y += AdditionalBorderHeight;
		BorderSlot->SetSize(BorderSize);
	}
}


void UHealthBarWidget::SetMaxValue(const float MaxValue)
{
	Max = MaxValue;
	UpdateProgressBarValue();
}

void UHealthBarWidget::AdditionalBarSetMaxValue(const float MaxValue, const int AdditionalBarIndex)
{
	if (AdditionalBars.IsValidIndex(AdditionalBarIndex))
	{
		FAdditionalHealthBar& AdditionalBar = AdditionalBars[AdditionalBarIndex];
		AdditionalBar.Max = MaxValue;
		UpdateAdditionalProgressBarValue(AdditionalBar);
	}
}

void UHealthBarWidget::SetCurrentValue(const float CurrentValue)
{
	Current = FMath::Clamp(CurrentValue, 0, Max);
	UpdateProgressBarValue();
}

void UHealthBarWidget::AdditionalBarSetCurrentValue(const float CurrentValue, const int AdditionalBarIndex)
{
	if (AdditionalBars.IsValidIndex(AdditionalBarIndex))
	{
		FAdditionalHealthBar& AdditionalBar = AdditionalBars[AdditionalBarIndex];
		AdditionalBar.Current = FMath::Clamp(CurrentValue, 0, AdditionalBar.Max);
		UpdateAdditionalProgressBarValue(AdditionalBar);
	}
}

// TODO: 我原本是想给这个插件添加一个的功能，使得 additional bars 也能够播动画，试着写了一下发现不对
// 主要是 HealthBarWidget 的 widget animation 这里绑定死了
// 最好的办法是在 WBP HealthBarWidget 中新增加 Mana Bar 和与它相关的动画，但我现在懒得这样做了
void UHealthBarWidget::RunnerAdditionalBarSetCurrentValue(const float CurrentValue, const int AdditionalBarIndex, const bool bAnimate)
{
	if (AdditionalBars.IsValidIndex(AdditionalBarIndex))
	{
		FAdditionalHealthBar& AdditionalBar = AdditionalBars[AdditionalBarIndex];
		auto PreviousPercentage = AdditionalBar.ProgressBar->GetPercent();
		AdditionalBar.Current = FMath::Clamp(CurrentValue, 0, AdditionalBar.Max);
		UpdateAdditionalProgressBarValue(AdditionalBar);
		auto Percentage = AdditionalBar.ProgressBar->GetPercent();

		/**
		* After progress bar value is updated play corresponding animation if needed
		* Play damage animation if new value is less than previous
		* Play heal animation if new value is greater than previous
		*/
		if (bAnimate)
		{
			if (Percentage < PreviousPercentage && DamageAnimation.Type != EHealthBarAnimationType::None)
			{
				PlayDamageAnimation(PreviousPercentage, Percentage);
			}
			if (Percentage > PreviousPercentage && HealAnimation.Type != EHealthBarAnimationType::None)
			{
				PlayHealAnimation(PreviousPercentage, Percentage);
			}
		}
	}
}

void UHealthBarWidget::SetMaxAndCurrentValues(const float MaxValue, const float CurrentValue, bool bAnimation)
{
	Max = MaxValue;
	Current = FMath::Clamp(CurrentValue, 0, Max);
	UpdateProgressBarValue(bAnimation);
}

void UHealthBarWidget::AdditionalBarSetMaxAndCurrentValues(const float MaxValue, const float CurrentValue, const int AdditionalBarIndex)
{
	if (AdditionalBars.IsValidIndex(AdditionalBarIndex))
	{
		FAdditionalHealthBar& AdditionalBar = AdditionalBars[AdditionalBarIndex];
		AdditionalBar.Max = MaxValue;
		AdditionalBar.Current = FMath::Clamp(CurrentValue, 0, AdditionalBar.Max);
		UpdateAdditionalProgressBarValue(AdditionalBar);
	}
}

void UHealthBarWidget::Add(const float Amount)
{
	const float NewValue = Current + Amount;
	SetCurrentValue(NewValue);
}

void UHealthBarWidget::AdditionalBarAdd(const float Amount, const int AdditionalBarIndex)
{
	if (AdditionalBars.IsValidIndex(AdditionalBarIndex))
	{
		const float NewValue = AdditionalBars[AdditionalBarIndex].Current + Amount;
		AdditionalBarSetCurrentValue(NewValue, AdditionalBarIndex);
	}
}

void UHealthBarWidget::Subtract(const float Amount)
{
	const float NewValue = Current - Amount;
	SetCurrentValue(NewValue);
}

void UHealthBarWidget::AdditionalBarSubtract(const float Amount, const int AdditionalBarIndex)
{
	if (AdditionalBars.IsValidIndex(AdditionalBarIndex))
	{
		const float NewValue = AdditionalBars[AdditionalBarIndex].Current - Amount;
		AdditionalBarSetCurrentValue(NewValue, AdditionalBarIndex);
	}
}


void UHealthBarWidget::SetLeftCustomText(FText Text)
{
	if (LeftTextBlock && !LeftTextBlock->TextDelegate.IsBound())
	{
		LeftTextBlock->SetText(Text);
	}
}

void UHealthBarWidget::SetMiddleCustomText(FText Text)
{
	if (MiddleTextBlock && !MiddleTextBlock->TextDelegate.IsBound())
	{
		MiddleTextBlock->SetText(Text);
	}
}

void UHealthBarWidget::SetRightCustomText(FText Text)
{
	if (RightTextBlock && !RightTextBlock->TextDelegate.IsBound())
	{
		RightTextBlock->SetText(Text);
	}
}

void UHealthBarWidget::SetOpacity(float InOpacity)
{
	SetRenderOpacity(InOpacity);
}

void UHealthBarWidget::SetFillColor(FLinearColor& InColor)
{
	if (FillMaterial)
	{
		FillMaterial->SetVectorParameterValue("Tint", InColor);
		if (DamageAnimation.Type == EHealthBarAnimationType::Fade && BackgroundMaterial)
		{
			BackgroundMaterial->SetVectorParameterValue("Animation Color", InColor);
		}
	}
}

void UHealthBarWidget::SetStripes(EHealthBarStripesType& InStripesType, int32 InNumberOfStripedSections, float InValuePerStripedSection)
{
	if (FillMaterial)
	{
		if (InStripesType == EHealthBarStripesType::None)
		{
			FillMaterial->SetScalarParameterValue("Sections Amount", 1.0f);
		}
		else if (InStripesType == EHealthBarStripesType::ExactNumber)
		{
			FillMaterial->SetScalarParameterValue("Sections Amount", InNumberOfStripedSections);
		}
		else if (InStripesType == EHealthBarStripesType::ValueBased)
		{
			const float Sections = InValuePerStripedSection > 0 ? FMath::CeilToInt(Max / InValuePerStripedSection) : 1;
			FillMaterial->SetScalarParameterValue("Sections Amount", Sections);
		}
	}
}







void UHealthBarWidget::UpdateProgressBarValue(bool bAnimate)
{
	float Percentage = Max > 0.0f ? Current / Max : 0.0f;
	if (ProgressBar)
	{
		const float PreviousPercentage = ProgressBar->GetPercent();
		ProgressBar->SetPercent(Percentage);
		
		/**
		* After progress bar value is updated play corresponding animation if needed
		* Play damage animation if new value is less than previous
		* Play heal animation if new value is greater than previous
		*/
		if (bAnimate)
		{
			if (Percentage < PreviousPercentage && DamageAnimation.Type != EHealthBarAnimationType::None)
			{
				PlayDamageAnimation(PreviousPercentage, Percentage);
			}
			if (Percentage > PreviousPercentage && HealAnimation.Type != EHealthBarAnimationType::None)
			{
				PlayHealAnimation(PreviousPercentage, Percentage);
			}
		}
	}
}

void UHealthBarWidget::UpdateAdditionalProgressBarValue(FAdditionalHealthBar& AdditionalBar)
{
	if (AdditionalBar.ProgressBar)
	{
		const float Percentage = AdditionalBar.Max > 0.0f ? AdditionalBar.Current / AdditionalBar.Max : 0.0f;
		AdditionalBar.ProgressBar->SetPercent(Percentage);
	}
}

void UHealthBarWidget::PlayDamageAnimation(float PreviousPercentage, float NewPercentage)
{
	if (BackgroundMaterial)
	{
		BackgroundMaterial->SetScalarParameterValue("Animated Amount", PreviousPercentage);
		BackgroundMaterial->SetScalarParameterValue("CatchUp Limit", NewPercentage);
		PlayAnimationForward(OnDamageAnim);
	}
}

void UHealthBarWidget::PlayHealAnimation(float PreviousPercentage, float NewPercentage)
{
	if (FillMaterial)
	{
		const float AnimatedAmount = 1.0f - PreviousPercentage;
		const float AnimatedLimit = 1.0f - NewPercentage;
		FillMaterial->SetScalarParameterValue("Animated Amount", AnimatedAmount);
		FillMaterial->SetScalarParameterValue("CatchUp Limit", AnimatedLimit);
		PlayAnimationForward(OnHealingAnim);
	}
}

UMaterialInterface* UHealthBarWidget::GetHealthBarMaterialAsset(FHealthBarAnimationSetup SetupData)
{
	UMaterialInterface* Asset = nullptr;
	if (SetupData.Type == EHealthBarAnimationType::None)
	{
		Asset = DefaultMaterial;
	}
	else if (SetupData.Type == EHealthBarAnimationType::Flash)
	{
		Asset = FlashMaterial;
	}
	else if (SetupData.Type == EHealthBarAnimationType::CatchUp)
	{
		Asset = CatchUpMaterial;
	}
	else if (SetupData.Type == EHealthBarAnimationType::Fade)
	{
		Asset = FadeMaterial;
	}

	return Asset;
}



FText UHealthBarWidget::CurrentValueBinding()
{
	FString CurrentValueText = FString::Printf(TEXT("%i"), FMath::RoundToInt32(Current));
	return FText::FromString(CurrentValueText);
}

FText UHealthBarWidget::MaxValueBinding()
{
	FString MaxValueText = FString::Printf(TEXT("%i"), FMath::RoundToInt32(Max));
	return FText::FromString(MaxValueText);
}

FText UHealthBarWidget::CurrentMaxValueBinding()
{
	FString CurrentMaxValueText = FString::Printf(TEXT("%i/%i"), FMath::RoundToInt32(Current), FMath::RoundToInt32(Max));
	return FText::FromString(CurrentMaxValueText);
}

FText UHealthBarWidget::PercentBinding()
{
	int Percentage = Max > 0.0f ? FMath::RoundToInt32(Current / Max * 100.0f) : 0.0f;
	FString PercentageText = FString::Printf(TEXT("%i%%"), Percentage);
	return FText::FromString(PercentageText);
}
