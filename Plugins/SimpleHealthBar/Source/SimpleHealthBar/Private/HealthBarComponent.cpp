// Copyright 2023, Dumitru Zahvatov, All rights reserved.


#include "HealthBarComponent.h"
#include "Kismet/KismetMathLibrary.h"

UHealthBarComponent::UHealthBarComponent(const FObjectInitializer& PCIP) : Super(PCIP)
{
	SetWidgetSpace(EWidgetSpace::Screen);
	SetWidgetClass(UHealthBarWidget::StaticClass());

	Size = FVector2D(160.0f, 10.0f);
	Max = 100.0f;
	Current = 100.0f;

	BorderWidth = 0;
	BorderColor = FLinearColor(1.0f, 1.0f, 1.0f, 1.0f);

	ProgressBarColor = FLinearColor(1.0f, 0.0f, 0.0f, 1.0f);
	StripesType = EHealthBarStripesType::None;
	NumberOfStripedSections = 1;
	ValuePerStripedSection = 0.f;

	MaxDistance = 0.0f;
	BaseOpacity = 1.0f;
	OpacityFalloffDistance = FVector2D(0.0f, 0.0f);
	bOcclusion = false;

	AdditionalBars = TArray<FAdditionalHealthBarSetup>();

	// Will be recalculated to true if needed, but fixes a bug with the widget being visible for a frame if in fact occluded
	SetVisibility(false);
}

void UHealthBarComponent::InitWidget()
{
	Super::InitWidget();

	/**
	* Setup widget
	* Pass all data from setup settings to corresponding methods
	*/
	HealthBarWidget = Cast<UHealthBarWidget>(GetWidget());
	if (HealthBarWidget)
	{
		HealthBarWidget->SetWidgetSize(Size, BorderWidth);
		HealthBarWidget->SetProgressBarAppearance(ProgressBarTexture, ProgressBarColor, DamageAnimationSetup, HealAnimationSetup);
		HealthBarWidget->SetBorderColor(BorderColor);
		
		if (BaseOpacity < 1.0f)
		{
			HealthBarWidget->SetOpacity(BaseOpacity);
		}
		if (LeftTextBlockSetup.bVisible)
		{
			HealthBarWidget->SetLeftTextBlockAppearance(LeftTextBlockSetup, Size, BorderWidth);
		}
		if (MiddleTextBlockSetup.bVisible)
		{
			HealthBarWidget->SetMiddleTextBlockAppearance(MiddleTextBlockSetup, Size, BorderWidth);
		}
		if (RightTextBlockSetup.bVisible)
		{
			HealthBarWidget->SetRightTextBlockAppearance(RightTextBlockSetup, Size, BorderWidth);
		}
		if (!AdditionalBars.IsEmpty())
		{
			HealthBarWidget->SetupAdditionalBars(AdditionalBars);
		}

		HealthBarWidget->SetMaxAndCurrentValues(Max, Current, false);
		HealthBarWidget->SetStripes(StripesType, NumberOfStripedSections, ValuePerStripedSection);
	}
}


void UHealthBarComponent::SetMaxValue_Implementation(const float MaxValue)
{
	if (HealthBarWidget)
	{
		HealthBarWidget->SetMaxValue(MaxValue);
	}
}

void UHealthBarComponent::AdditionalBarSetMaxValue_Implementation(const float MaxValue, const int AdditionalBarIndex)
{
	if (HealthBarWidget)
	{
		HealthBarWidget->AdditionalBarSetMaxValue(MaxValue, AdditionalBarIndex);
	}
}

void UHealthBarComponent::SetCurrentValue_Implementation(const float CurrentValue)
{
	if (HealthBarWidget)
	{
		HealthBarWidget->SetCurrentValue(CurrentValue);
	}
}

void UHealthBarComponent::AdditionalBarSetCurrentValue_Implementation(const float CurrentValue, const int AdditionalBarIndex)
{
	if (HealthBarWidget)
	{
		HealthBarWidget->AdditionalBarSetCurrentValue(CurrentValue, AdditionalBarIndex);
	}
}

void UHealthBarComponent::SetMaxAndCurrentValues_Implementation(const float MaxValue, const float CurrentValue)
{
	if (HealthBarWidget)
	{
		HealthBarWidget->SetMaxAndCurrentValues(MaxValue, CurrentValue);
	}
}

void UHealthBarComponent::AdditionalBarSetMaxAndCurrentValues_Implementation(const float MaxValue, const float CurrentValue, const int AdditionalBarIndex)
{
	if (HealthBarWidget)
	{
		HealthBarWidget->AdditionalBarSetMaxAndCurrentValues(MaxValue, CurrentValue, AdditionalBarIndex);
	}
}

void UHealthBarComponent::Add_Implementation(const float Amount)
{
	if (HealthBarWidget)
	{
		HealthBarWidget->Add(Amount);
	}
}

void UHealthBarComponent::AdditionalBarAdd_Implementation(const float Amount, const int AdditionalBarIndex)
{
	if (HealthBarWidget)
	{
		HealthBarWidget->AdditionalBarAdd(Amount, AdditionalBarIndex);
	}
}

void UHealthBarComponent::Subtract_Implementation(const float Amount)
{
	if (HealthBarWidget)
	{
		HealthBarWidget->Subtract(Amount);
	}
}

void UHealthBarComponent::AdditionalBarSubtract_Implementation(const float Amount, const int AdditionalBarIndex)
{
	if (HealthBarWidget)
	{
		HealthBarWidget->AdditionalBarSubtract(Amount, AdditionalBarIndex);
	}
}

void UHealthBarComponent::SetLeftCustomText_Implementation(const FText& Text)
{
	if (HealthBarWidget)
	{
		HealthBarWidget->SetLeftCustomText(Text);
	}
}

void UHealthBarComponent::SetMiddleCustomText_Implementation(const FText& Text)
{
	if (HealthBarWidget)
	{
		HealthBarWidget->SetMiddleCustomText(Text);
	}
}

void UHealthBarComponent::SetRightCustomText_Implementation(const FText& Text)
{
	if (HealthBarWidget)
	{
		HealthBarWidget->SetRightCustomText(Text);
	}
}

void UHealthBarComponent::SetBorderColor(FLinearColor InColor)
{
	if (HealthBarWidget)
	{
		HealthBarWidget->SetBorderColor(InColor);
	}
}

void UHealthBarComponent::SetMaxDistance(float InMaxDistance)
{
	if (HealthBarWidget)
	{
		MaxDistance = InMaxDistance;
	}
}

void UHealthBarComponent::SetOpacity(float InOpacity)
{
	if (HealthBarWidget)
	{
		BaseOpacity = InOpacity;
		HealthBarWidget->SetOpacity(InOpacity);
	}
}

void UHealthBarComponent::SetOpacityFalloffDistance(FVector2D InOpacityFalloffDistance)
{
	if (HealthBarWidget)
	{
		OpacityFalloffDistance = InOpacityFalloffDistance;
	}
}

void UHealthBarComponent::SetOcclusion(bool InOcclusion)
{
	if (HealthBarWidget)
	{
		bOcclusion = InOcclusion;
	}
}

void UHealthBarComponent::SetFillColor(FLinearColor InColor)
{
	if (HealthBarWidget)
	{
		HealthBarWidget->SetFillColor(InColor);
	}
}

void UHealthBarComponent::SetStripes(EHealthBarStripesType InStripesType, int32 InNumberOfStripedSections, float InValuePerStripedSection)
{
	if (HealthBarWidget)
	{
		HealthBarWidget->SetStripes(InStripesType, InNumberOfStripedSections, InValuePerStripedSection);
	}
}



void UHealthBarComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

/* HealthBar visibility is for client only */
#if !UE_SERVER
	if (!IsRunningDedicatedServer())
	{
		if ((GetUserWidgetObject() || GetSlateWidget().IsValid()) && Space == EWidgetSpace::Screen)
		{
			bool bNewVisibility = CalculateVisibility();
			if (IsVisible() != bNewVisibility)
			{
				SetVisibility(bNewVisibility);
			}
		}
	}
#endif
}

bool UHealthBarComponent::CalculateVisibility()
{
	const bool bFadeByDistance = MaxDistance > 0;
	const bool bFadeByOpacity = OpacityFalloffDistance.Y > 0;

	if (bFadeByDistance || bFadeByOpacity || bOcclusion)
	{
		FVector ViewportLocation;
		FVector OwnerLocation;

		UWorld* World = GetWorld();
		AActor* Owner = GetOwner();
		if (Owner)
		{
			OwnerLocation = Owner->GetActorLocation();
		}

		/*
		* Occlusion
		* Check if some owner's primitive component was rendered
		*/
		if (bOcclusion)
		{
			bool bOccluded = true;
			TArray<UActorComponent*> Components = TArray<UActorComponent*>();
			Owner->GetComponents(UPrimitiveComponent::StaticClass(), Components);
			for (UActorComponent* Comp : Components)
			{
				const UPrimitiveComponent* Primitive = Cast<UPrimitiveComponent>(Comp);
				bool bPrimitiveComponenetOnScreen = World->GetTimeSeconds() - Primitive->GetLastRenderTimeOnScreen() <= 0.1f;
				if (bPrimitiveComponenetOnScreen)
				{
					bOccluded = false;
				}
			}

			if (bOccluded)
			{
				return false;
			}
		}

		if (World && World->IsGameWorld())
		{
			ULocalPlayer* Player = GetOwnerPlayer();
			if (Player)
			{
				ViewportLocation = Player->LastViewLocation;
			}
		}

		const double DistanceFromViewportToWidget = UKismetMathLibrary::Vector_Distance(OwnerLocation, ViewportLocation);

		/*
		* Distance visibility
		* Check if widget is in acceptable distance
		*/
		if (bFadeByDistance && MaxDistance < DistanceFromViewportToWidget)
		{
			return false;
		}

		/*
		* Opacity falloff
		* Set widget opacity based on distance
		* Hide widget when opaque
		*/
		if (bFadeByOpacity)
		{
			const bool FalloffIsValid = OpacityFalloffDistance.X <= OpacityFalloffDistance.Y;
			if (FalloffIsValid)
			{
				const float DistanceAlpha = FMath::Clamp(OpacityFalloffDistance.Y - DistanceFromViewportToWidget, 0.0f, OpacityFalloffDistance.Y) / (OpacityFalloffDistance.Y - OpacityFalloffDistance.X);
				const float NewOpacity = FMath::Lerp(0.0f, BaseOpacity, DistanceAlpha);
				if (NewOpacity == 0.0f)
				{
					return false;
				}
				else if (HealthBarWidget)
				{
					HealthBarWidget->SetOpacity(NewOpacity);
				}
			}
		}

	}

	return true;
}
