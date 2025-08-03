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
#include "Materials/MaterialInstanceDynamic.h"
#include "Math/UnrealMathUtility.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "Public/GameUIUserWidget.h"
#include "Public/RunnerControllerBase.h"
#include "Public/RunnerMovementComponent.h"
#include "Public/WorldGenerator.h"
#include "TimerManager.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/WeakObjectPtrTemplates.h"

DEFINE_LOG_CATEGORY(LogRunnerCharacter);

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
			LThrust->Deactivate();
		}
		if (Component->GetName().Contains(TEXT("RightThrust")))
		{
			RThrust = Component;
			RThrust->Deactivate();
		}
		else if (Component->GetName().Contains(TEXT("SlowDown")))
		{
			SlowDownComp = Component;
			SlowDownComp->Deactivate();
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
	GetCapsuleComponent()->OnComponentEndOverlap.AddDynamic(this, &ARunnerCharacter::DealBarrierOverlapEnd);
	InitUI();

	GetMesh()->CreateDynamicMaterialInstance(0);
	TActorIterator<AWorldGenerator> It(GetWorld());
	if (It)
	{
		WorldGenerator = *It;
	}
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

		EnhancedInputComponent->BindAction(ThrustAction, ETriggerEvent::Started, this, &ARunnerCharacter::StartThrust, EThrustSource::UserThrust, 0.0f);
		EnhancedInputComponent->BindAction(ThrustAction, ETriggerEvent::Completed, this, &ARunnerCharacter::StopThrust, EThrustSource::UserThrust);
		EnhancedInputComponent->BindAction(ThrustAction, ETriggerEvent::Canceled, this, &ARunnerCharacter::StopThrust, EThrustSource::UserThrust);

		auto* RunnerMoveComp = Cast<URunnerMovementComponent>(GetMovementComponent());
		EnhancedInputComponent->BindAction(DownAction, ETriggerEvent::Started, RunnerMoveComp, &URunnerMovementComponent::StartDown);
		EnhancedInputComponent->BindAction(DownAction, ETriggerEvent::Completed, RunnerMoveComp, &URunnerMovementComponent::StopDown);
		EnhancedInputComponent->BindAction(DownAction, ETriggerEvent::Canceled, RunnerMoveComp, &URunnerMovementComponent::StopDown);

		EnhancedInputComponent->BindActionValue(DownAction);
		EnhancedInputComponent->BindActionValue(MoveAction);
	}
	else
	{
		// UE_LOG(LogRunnerCharacter, Error, TEXT("'%s' Failed to find an Enhanced Input component! This template is built to use the Enhanced Input system. If you intend to use the legacy system, then you will need to update this C++ file."), *GetNameSafe(this));
	}
}

void ARunnerCharacter::Tick(float Delta)
{
	Super::Tick(Delta);
	// ARunnerCharacter::Move(FInputActionValue(FVector2D(0.0f, 0.0f))); // Call Move with default values to ensure movement logic is processed
	// Add any character-specific tick logic here
	TurnDirection(Delta);
	// if (bAutoMove)
	// {
	// 	AddMovementInput(GetActorForwardVector(), 1.0f);
	// 	// UE_LOG(LogRunnerCharacter, Log, TEXT("ARunnerCharacter::Tick called with Delta: %s"), *GetVelocity().ToString());
	// }

	if (Thrusting & static_cast<int8>(EThrustSource::FreeThrust))
	{
		FreeThrustTime -= Delta;
		if (FreeThrustTime <= 0.0f)
		{
			StopThrust(EThrustSource::FreeThrust);
			FreeThrustTime = 0.0f;
		}
	}
	else if (Thrusting & static_cast<int8>(EThrustSource::UserThrust))
	{
		Mana -= ManaReduceSpeed * Delta;
		if (Mana <= 0.0f)
		{
			Mana = 0.0f;
			StopThrust(EThrustSource::UserThrust);
		}
		GameUIWidget->UpdateGas(Mana / MaxMana);
	}

	CheckEvilChase(Delta);
	UpdateFlicker(Delta);
}

void ARunnerCharacter::CheckEvilChase(float Delta)
{
	auto TileX = double(WorldGenerator->XCellNumber) * WorldGenerator->CellSize;
	auto PlayerPos = GetActorLocation().X / TileX;
	GetMesh()->SetScalarParameterValueOnMaterials("PlayerPos", PlayerPos);
	auto EvilPos = WorldGenerator->GetEvilPos();

	if (EvilPos / TileX - PlayerPos < DeltaTakeDamge)
	{
		CurrentDamageInterval = 0.5f;
	}
	else
	{
		CurrentDamageInterval += Delta;
		if (CurrentDamageInterval >= DamageInterval)
		{
			UE_LOG(LogRunnerCharacter, Log, TEXT("ARunnerCharacter::Tick - Taking damage, Evil Pos %f, Player Pos %f"), EvilPos, PlayerPos);
			CurrentDamageInterval -= DamageInterval;
			TakeHitImpact(true);
		}
	}
}

void ARunnerCharacter::UpdateFlicker(float Delta)
{
	// 在顿帧结束后才会触发 flicker，因此这里的 delta 是世界时间
	if (bFlicker)
	{
		int32 CurrentFlickCount = FMath::FloorToInt(CurrentFlickerTime / FlipInterval);
		CurrentFlickerTime += Delta;
		int32 NewFlickCount = FMath::FloorToInt(CurrentFlickerTime / FlipInterval);
		auto Diff = NewFlickCount - CurrentFlickCount;
		if ((Diff & 1) == 1)
		{
			ExchangeMaterial();
			if (NewFlickCount >= FlipTime - 1)
			{
				bFlicker = false;
			}
		}
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
		// AddMovementInput(ForwardDirection, FMath::Clamp(MovementVector.Y, 0.0f, 1.0f));
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

void ARunnerCharacter::StartThrust(EThrustSource ThrustStatus, float ThrustTime)
{
	// UE_LOG(LogRunnerCharacter, Log, TEXT("ARunnerCharacter::StartThrust called, Mana: %f"), Mana);
	auto OldStatus = Thrusting;
	int8 ThrustStatusInt = static_cast<int8>(ThrustStatus);
	if (ThrustStatus == EThrustSource::FreeThrust)
	{
		FreeThrustTime = FMath::Max(FreeThrustTime, ThrustTime);
		Thrusting |= ThrustStatusInt; // Set the thrusting status
	}
	else if (Mana > MinimumThrustMana)
	{
		Thrusting |= ThrustStatusInt; // Set the thrusting status
	}

	if (OldStatus == 0 && Thrusting > 0)
	{
		auto* MovementComp = Cast<URunnerMovementComponent>(GetCharacterMovement());
		MovementComp->StartThrust();

		// 从 non thrust 切换到 thrust 状态
		LThrust->Activate();
		RThrust->Activate();
	}
}

void ARunnerCharacter::StopThrust(EThrustSource ThrustStatus)
{
	// UE_LOG(LogRunnerCharacter, Log, TEXT("ARunnerCharacter::StopThrust called, Mana: %f"), Mana);
	auto OldStatus = Thrusting;
	int8 ThrustStatusInt = static_cast<int8>(ThrustStatus);
	Thrusting &= ~ThrustStatusInt; // Clear the thrusting status

	if (OldStatus > 0 && Thrusting == 0)
	{
		auto* MovementComp = Cast<URunnerMovementComponent>(GetCharacterMovement());
		MovementComp->StopThrust();

		LThrust->Deactivate();
		RThrust->Deactivate();
	}
}

void ARunnerCharacter::StartSlowDown()
{
	SlowDown++;
	auto* MovementComp = Cast<URunnerMovementComponent>(GetCharacterMovement());
	MovementComp->StartSlowDown();
	if (SlowDown == 1)
	{
		SlowDownComp->Activate();
	}
}

void ARunnerCharacter::StopSlowDown()
{
	SlowDown--;
	ensure(SlowDown >= 0);
	auto* MovementComp = Cast<URunnerMovementComponent>(GetCharacterMovement());
	MovementComp->StopSlowDown();
	if (SlowDown == 0)
	{
		SlowDownComp->Deactivate();
	}
}

void ARunnerCharacter::InitUI()
{
	if (GameUIWidgetClass)
	{
		GameUIWidget = CreateWidget<UGameUIUserWidget>(GetWorld(), GameUIWidgetClass);
		if (GameUIWidget)
		{
			GameUIWidget->AddToViewport();
			Health = MaxHealth;
			Mana = MaxMana * InitialManaScale;
		}
		GameUIWidget->UpdateLifeImmediately(1.0f);
		GameUIWidget->UpdateGasImmediately(InitialManaScale);
	}
}

void ARunnerCharacter::TakeHitImpact(bool bOnlyUI)
{
	// 当处于顿帧或者闪烁状态时，不处理邪气以外的伤害
	if (!bOnlyUI && (bFlicker || CustomTimeDilation == HitSlomo))
	{
		return;
	}

	if (GameUIWidget)
	{
		Health -= HitDamage;
		if (Health <= 0.0f)
		{
			Health = 0.0f;
			// Handle character death
			// UE_LOG(LogRunnerCharacter, Warning, TEXT("Character has died!"));
		}
		UE_LOG(LogRunnerCharacter, Log, TEXT("ARunnerCharacter::TakeHitImpact: Health %f"), Health);
		GameUIWidget->AddLife(-HitDamage / MaxHealth);
	}
	ShowDamageUI(bOnlyUI);
	// UGameplayStatics::SetGlobalTimeDilation(this, HitSlomo);
	if (!bOnlyUI)
	{
		CustomTimeDilation = HitSlomo;
		FTimerHandle TimerHandle;

		// auto RealHitSlomoTime = HitSlomoTime * HitSlomo;
		// GetWorld()->GetTimerManager().SetTimer(TimerHandle, [this]() { UGameplayStatics::SetGlobalTimeDilation(this, 1.0f); }, RealHitSlomoTime, false);
		GetWorld()->GetTimerManager().SetTimer(TimerHandle, [WeakCharacter = TWeakObjectPtr<ARunnerCharacter>(this)]() { 
			auto Char = WeakCharacter.Get();
			if (Char)
			{
				Char->CustomTimeDilation = 1.0f; 
				Char->bFlicker = true;
				Char->CurrentFlickerTime = 0.0f;
				Char->ExchangeMaterial();
			} }, HitSlomoTime, false);
	}
	// TODO：播放死亡动画
	if (Health <= 0.0f && !bInfiniteHealth)
	{
		auto* RunnerPC = Cast<ARunnerControllerBase>(GetController());
		UE_LOG(LogRunnerCharacter, Log, TEXT("ARunnerCharacter:: Average Speed %f"), GetActorLocation().X / WorldGenerator->WorldTime);
		RunnerPC->GameOver(GameUIWidget->GetTotalScore());
	}
}

// defered movement update 中会检查 collision channel 是否仍然为 block!
void ARunnerCharacter::NotifyHit(class UPrimitiveComponent* MyComp, AActor* Other, class UPrimitiveComponent* OtherComp, bool bSelfMoved, FVector HitLocation, FVector HitNormal, FVector NormalImpulse, const FHitResult& Hit)
{
	Super::NotifyHit(MyComp, Other, OtherComp, bSelfMoved, HitLocation, HitNormal, NormalImpulse, Hit);
	// Handle hit impact
	// auto bBlockingHit = Cast<URunnerMovementComponent>(GetMovementComponent())->HandleBlockingHit(Hit);
	// if (bBlockingHit)
	// {
	// 	UE_LOG(LogRunnerCharacter, Warning, TEXT("ARunnerCharacter::NotifyHit: %s Blocking Hit %s"), *MyComp->GetName(), *OtherComp->GetName());
	// 	TakeHitImpact();
	// }
}

void ARunnerCharacter::NotifyActorBeginOverlap(AActor* OtherActor)
{
	Super::NotifyActorBeginOverlap(OtherActor);
	// TakeHitImpact();
	// UE_LOG(LogRunnerCharacter, Warning, TEXT("ARunnerCharacter::NotifyActorBeginOverlap: %s"), *OtherActor->GetName());
}

void ARunnerCharacter::DealBarrierOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep,
		FHitResult const& SweepResult)
{
	if (OtherComp->ComponentHasTag("GoldCoin"))
	{
		// UE_LOG(LogRunnerCharacter, Warning, TEXT("ARunnerCharacter::DealBarrierOverlap: Get Coin %d"), OtherBodyIndex);
		Health += CoinHealHealth;
		Health = FMath::Min(Health, MaxHealth);
		GameUIWidget->AddOneCoin(CoinHealHealth / MaxHealth);
	}
	else if (OtherComp->ComponentHasTag("Mud"))
	{
		EnterMud();
		// UE_LOG(LogRunnerCharacter, Warning, TEXT("ARunnerCharacter::DealBarrierOverlap: Hit Mud %s"), *OtherActor->GetName());
	}
	else
	{
		TakeHitImpact(false);
	}
}

void ARunnerCharacter::DealBarrierOverlapEnd(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	if (OtherComp->ComponentHasTag("Mud"))
	{
		// UE_LOG(LogRunnerCharacter, Warning, TEXT("Mud %s overlap end"), *OtherActor->GetName());
		OutOfMud();
	}
}

void ARunnerCharacter::EnterMud()
{
	InMud++;

	// 当处于地面上时，进入泥潭触发 slow down
	auto* RunnerMoveComp = Cast<URunnerMovementComponent>(GetCharacterMovement());
	auto* FloorActor = RunnerMoveComp->CurrentFloor.HitResult.GetActor();
	if (FloorActor == WorldGenerator && InMud == 1)
	{
		StartSlowDown();
	}
}

void ARunnerCharacter::OutOfMud()
{
	InMud--;
	ensure(InMud >= 0);

	// 当离开泥潭时，停止 slow down
	auto* RunnerMoveComp = Cast<URunnerMovementComponent>(GetCharacterMovement());
	auto* FloorActor = RunnerMoveComp->CurrentFloor.HitResult.GetActor();
	if (FloorActor == WorldGenerator && InMud == 0)
	{
		StopSlowDown();
	}
}

void ARunnerCharacter::FellOutOfWorld(const class UDamageType& dmgType)
{
	auto* RunnerPC = Cast<ARunnerControllerBase>(GetController());
	Super::FellOutOfWorld(dmgType);
	// TODO：播放一个死亡特效？
	if (RunnerPC)
	{
		RunnerPC->GameOver(GameUIWidget->GetTotalScore());
	}
}

void ARunnerCharacter::ExchangeMaterial()
{
	auto MatOpacity = bMaterialFlipped ? 1.0f : Opacity;
	bMaterialFlipped = !bMaterialFlipped;
	GetMesh()->SetScalarParameterValueOnMaterials("Opacity", MatOpacity);

	// auto* Mat = GetMesh()->GetMaterial(0);
	// GetMesh()->SetMaterial(0, TmpMaterialSlot);
	// TmpMaterialSlot = Mat;
}

void ARunnerCharacter::AddGas(float Percentage)
{
	if (GameUIWidget)
	{
		GameUIWidget->AddGas(Percentage);
		Mana += Percentage * MaxMana;
		if (Mana > MaxMana)
		{
			Mana = MaxMana;
		}
	}
}