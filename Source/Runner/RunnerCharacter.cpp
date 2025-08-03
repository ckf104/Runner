// Copyright Epic Games, Inc. All Rights Reserved.

#include "RunnerCharacter.h"
#include "Blueprint/UserWidget.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/LocalPlayer.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/Controller.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "HealthBarWidget.h"
#include "InputActionValue.h"
#include "Kismet/GameplayStatics.h"
#include "Math/UnrealMathUtility.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "Public/RunnerMovementComponent.h"
#include "Public/WorldGenerator.h"
#include "TimerManager.h"
#include "UObject/UObjectGlobals.h"

DEFINE_LOG_CATEGORY(LogTemplateCharacter);

//////////////////////////////////////////////////////////////////////////
// ARunnerCharacter

ARunnerCharacter::ARunnerCharacter(const FObjectInitializer& ObjInitializer)
		: Super(ObjInitializer.SetDefaultSubobjectClass<URunnerMovementComponent>(ACharacter::CharacterMovementComponentName))
{
	// Set size for collision capsule
	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.0f);

	// Don't rotate when the controller rotates. Let that just affect the camera.
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	// Configure character movement
	GetCharacterMovement()->bOrientRotationToMovement = true;						 // Character moves in the direction of input...
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 500.0f, 0.0f); // ...at this rotation rate

	// Note: For faster iteration times these variables, and many more, can be tweaked in the Character Blueprint
	// instead of recompiling to adjust them
	GetCharacterMovement()->JumpZVelocity = 700.f;
	GetCharacterMovement()->AirControl = 0.35f;
	GetCharacterMovement()->MaxWalkSpeed = 500.f;
	GetCharacterMovement()->MinAnalogWalkSpeed = 20.f;
	GetCharacterMovement()->BrakingDecelerationWalking = 2000.f;
	GetCharacterMovement()->BrakingDecelerationFalling = 1500.0f;

	// Create a camera boom (pulls in towards the player if there is a collision)
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 400.0f;				// The camera follows at this distance behind the character
	CameraBoom->bUsePawnControlRotation = true; // Rotate the arm based on the controller

	// Create a follow camera
	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName); // Attach the camera to the end of the boom and let the boom adjust to match the controller orientation
	FollowCamera->bUsePawnControlRotation = false;															// Camera does not rotate relative to arm

	// Note: The skeletal mesh and anim blueprint references on the Mesh component (inherited from Character)
	// are set in the derived blueprint asset named ThirdPersonCharacter (to avoid direct content references in C++)
}

void ARunnerCharacter::BeginPlay()
{
	Super::BeginPlay();
	ForEachComponent<UNiagaraComponent>(false, [this](UNiagaraComponent* Component) {
		if (Component->GetName().Contains(TEXT("LeftThrust")))
		{
			LThrust = Component;
			LThrust->SetHiddenInGame(true);
			LThrust->Deactivate();
			return false; // Stop iterating once we find the LeftThrust component
		}

		return true; // Continue iterating
	});
	ForEachComponent<UNiagaraComponent>(false, [this](UNiagaraComponent* Component) {
		if (Component->GetName().Contains(TEXT("RightThrust")))
		{
			RThrust = Component;
			RThrust->SetHiddenInGame(true);
			RThrust->Deactivate();
			return false; // Stop iterating once we find the RightThrust component
		}

		return true; // Continue iterating
	});
	ForEachComponent<UPrimitiveComponent>(false, [this](UPrimitiveComponent* Component) {
		if (Component->ComponentHasTag("Absorb"))
		{
			Component->OnComponentBeginOverlap.AddDynamic(this, &ARunnerCharacter::DealBarrierOverlap);
			
		}
	});
	GetCapsuleComponent()->OnComponentBeginOverlap.AddDynamic(this, &ARunnerCharacter::DealBarrierOverlap);
	InitUI();
}

//////////////////////////////////////////////////////////////////////////
// Input

void ARunnerCharacter::NotifyControllerChanged()
{
	Super::NotifyControllerChanged();

	// Add Input Mapping Context
	if (APlayerController* PlayerController = Cast<APlayerController>(Controller))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PlayerController->GetLocalPlayer()))
		{
			Subsystem->AddMappingContext(DefaultMappingContext, 0);
		}
	}
}

void ARunnerCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	// Set up action bindings
	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{

		// Jumping
		// EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Started, this, &ACharacter::Jump);
		// EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Completed, this, &ACharacter::StopJumping);

		// Moving
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &ARunnerCharacter::Move);

		// Looking
		EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &ARunnerCharacter::Look);

		EnhancedInputComponent->BindAction(ThrustAction, ETriggerEvent::Started, this, &ARunnerCharacter::StartThrust);
		EnhancedInputComponent->BindAction(ThrustAction, ETriggerEvent::Completed, this, &ARunnerCharacter::StopThrust);

		EnhancedInputComponent->BindActionValue(DownAction);
		EnhancedInputComponent->BindActionValue(MoveAction);
	}
	else
	{
		UE_LOG(LogTemplateCharacter, Error, TEXT("'%s' Failed to find an Enhanced Input component! This template is built to use the Enhanced Input system. If you intend to use the legacy system, then you will need to update this C++ file."), *GetNameSafe(this));
	}
}

void ARunnerCharacter::Tick(float Delta)
{
	Super::Tick(Delta);
	// ARunnerCharacter::Move(FInputActionValue(FVector2D(0.0f, 0.0f))); // Call Move with default values to ensure movement logic is processed
	// Add any character-specific tick logic here
	TurnDirection(Delta);
	if (bAutoMove)
	{
		AddMovementInput(GetActorForwardVector(), 1.0f);
		// UE_LOG(LogTemplateCharacter, Log, TEXT("ARunnerCharacter::Tick called with Delta: %s"), *GetVelocity().ToString());
	}
	if (bThrusting)
	{
		Mana -= ManaReduceSpeed * Delta;
		if (Mana <= 0.0f)
		{
			Mana = 0.0f;
			StopThrust();
		}
		ManaBarWidget->SetCurrentValue(Mana);
	}
}

float ARunnerCharacter::GetDownValue() const
{
	auto* InputComp = Cast<UEnhancedInputComponent>(InputComponent);
	if (InputComp)
	{
		FInputActionValue DownValue = InputComp->GetBoundActionValue(DownAction);
		return DownValue.Get<float>();
	}
	return 0.0f; // Default value if no input is found
}

float ARunnerCharacter::GetTurnValue() const
{
	auto* InputComp = Cast<UEnhancedInputComponent>(InputComponent);
	if (InputComp)
	{
		FInputActionValue MoveValue = InputComp->GetBoundActionValue(MoveAction);
		return MoveValue.Get<FVector2D>().X;
	}
	return 0.0f; // Default value if no input is found
}

void ARunnerCharacter::TurnDirection(float Delta)
{
	YawTurnInput = FMath::Clamp(YawTurnInput, -1.0f * TurnSpeedScale, 1.0f * TurnSpeedScale);
	auto YawAngle = YawTurnInput * Delta * 360.0f; // Adjust the multiplier to control turn speed
	YawTurnInput = 0.0f;													 // Reset after applying the turn input
	if (FMath::IsNearlyZero(YawAngle))
	{
		return; // No turn input, skip processing
	}
	auto Velocity = GetVelocity();
	auto ZVelocity = Velocity.Z;

	auto GroundSpeed = Velocity.Size2D();
	auto ActorRotation = GetActorRotation();
	ActorRotation.Yaw += YawAngle;
	SetActorRotation(ActorRotation);
	auto NewGroundVelocity = GetActorForwardVector().GetSafeNormal2D() * GroundSpeed;
	GetCharacterMovement()->Velocity = FVector(NewGroundVelocity.X, NewGroundVelocity.Y, ZVelocity);
}

void ARunnerCharacter::Move(const FInputActionValue& Value)
{
	// input is a Vector2D
	FVector2D MovementVector = Value.Get<FVector2D>();

	if (Controller != nullptr)
	{
		// find out which way is forward
		// const FRotator Rotation = Controller->GetControlRotation();
		// const FRotator YawRotation(0, Rotation.Yaw, 0);

		// // get forward vector
		// const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
		// // get right vector
		// const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

		// 滑板控制器应该使用角色的前向和右向向量
		auto ForwardDirection = GetActorForwardVector();
		YawTurnInput += MovementVector.X;

		// add movement
		AddMovementInput(ForwardDirection, FMath::Clamp(MovementVector.Y, 0.0f, 1.0f));
		// AddMovementInput(ForwardDirection, -1.0f);
		// AddMovementInput(RightDirection, MovementVector.X);
	}
}

void ARunnerCharacter::Look(const FInputActionValue& Value)
{
	// input is a Vector2D
	FVector2D LookAxisVector = Value.Get<FVector2D>();

	if (Controller != nullptr)
	{
		// add yaw and pitch input to controller
		AddControllerYawInput(LookAxisVector.X);
		AddControllerPitchInput(LookAxisVector.Y);
	}
}

void ARunnerCharacter::StartThrust()
{
	UE_LOG(LogTemplateCharacter, Log, TEXT("ARunnerCharacter::StartThrust called, Mana: %f"), Mana);
	if (Mana < MinimumThrustMana)
	{
		return; // Not enough mana to thrust
	}
	bThrusting = true;
	auto* MovementComp = Cast<URunnerMovementComponent>(GetCharacterMovement());
	MovementComp->StartThrust();
	if (LThrust)
	{
		LThrust->SetHiddenInGame(false);
		LThrust->Activate();
	}
	if (RThrust)
	{
		RThrust->SetHiddenInGame(false);
		RThrust->Activate();
	}
}

void ARunnerCharacter::StopThrust()
{
	UE_LOG(LogTemplateCharacter, Log, TEXT("ARunnerCharacter::StopThrust called, Mana: %f"), Mana);
	if (bThrusting)
	{
		bThrusting = false;
		auto* MovementComp = Cast<URunnerMovementComponent>(GetCharacterMovement());
		MovementComp->StopThrust();
		if (LThrust)
		{
			LThrust->SetHiddenInGame(true);
			LThrust->Deactivate();
		}
		if (RThrust)
		{
			RThrust->SetHiddenInGame(true);
			RThrust->Deactivate();
		}
	}
}

void ARunnerCharacter::InitUI()
{
	if (HealthBarWidgetClass)
	{
		HealthBarWidget = CreateWidget<UHealthBarWidget>(GetWorld(), HealthBarWidgetClass);
		if (HealthBarWidget)
		{
			HealthBarWidget->AddToViewport();
			Health = MaxHealth;
			HealthBarWidget->SetMaxAndCurrentValues(MaxHealth, Health);
		}
	}

	if (ManaBarWidgetClass)
	{
		ManaBarWidget = CreateWidget<UHealthBarWidget>(GetWorld(), ManaBarWidgetClass);
		if (ManaBarWidget)
		{
			ManaBarWidget->AddToViewport();
			Mana = MaxMana;
			ManaBarWidget->SetMaxAndCurrentValues(MaxMana, Mana);
		}
	}
}

void ARunnerCharacter::TakeHitImpact()
{
	if (HealthBarWidget)
	{
		Health -= HitDamage;
		if (Health <= 0.0f)
		{
			Health = 0.0f;
			// Handle character death
			UE_LOG(LogTemplateCharacter, Warning, TEXT("Character has died!"));
		}
		HealthBarWidget->SetCurrentValue(Health);
	}
	ShowDamageUI();
	// UGameplayStatics::SetGlobalTimeDilation(this, HitSlomo);
	CustomTimeDilation = HitSlomo;
	FTimerHandle TimerHandle;
	// auto RealHitSlomoTime = HitSlomoTime * HitSlomo;
	// GetWorld()->GetTimerManager().SetTimer(TimerHandle, [this]() { UGameplayStatics::SetGlobalTimeDilation(this, 1.0f); }, RealHitSlomoTime, false);
	GetWorld()->GetTimerManager().SetTimer(TimerHandle, [this]() { CustomTimeDilation = 1.0f; }, HitSlomoTime, false);
}

// defered movement update 中会检查 collision channel 是否仍然为 block!
void ARunnerCharacter::NotifyHit(class UPrimitiveComponent* MyComp, AActor* Other, class UPrimitiveComponent* OtherComp, bool bSelfMoved, FVector HitLocation, FVector HitNormal, FVector NormalImpulse, const FHitResult& Hit)
{
	Super::NotifyHit(MyComp, Other, OtherComp, bSelfMoved, HitLocation, HitNormal, NormalImpulse, Hit);
	// Handle hit impact
	// auto bBlockingHit = Cast<URunnerMovementComponent>(GetMovementComponent())->HandleBlockingHit(Hit);
	// if (bBlockingHit)
	// {
	// 	UE_LOG(LogTemplateCharacter, Warning, TEXT("ARunnerCharacter::NotifyHit: %s Blocking Hit %s"), *MyComp->GetName(), *OtherComp->GetName());
	// 	TakeHitImpact();
	// }
}

void ARunnerCharacter::NotifyActorBeginOverlap(AActor* OtherActor)
{
	Super::NotifyActorBeginOverlap(OtherActor);
	// TakeHitImpact();
	// UE_LOG(LogTemplateCharacter, Warning, TEXT("ARunnerCharacter::NotifyActorBeginOverlap: %s"), *OtherActor->GetName());
}

void ARunnerCharacter::DealBarrierOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep,
		FHitResult const& SweepResult)
{
	if (OtherComp->ComponentHasTag("GoldCoin"))
	{
		UE_LOG(LogTemp, Warning, TEXT("ARunnerCharacter::DealBarrierOverlap: Get Coin %d"), OtherBodyIndex);
	}
	else
	{
		TakeHitImpact();
	}
}