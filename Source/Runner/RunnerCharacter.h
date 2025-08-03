// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Public/RunnerMovementComponent.h"
#include "Logging/LogMacros.h"
#include "Templates/SubclassOf.h"
#include "RunnerCharacter.generated.h"

class USpringArmComponent;
class UCameraComponent;
class UInputMappingContext;
class UInputAction;
struct FInputActionValue;

DECLARE_LOG_CATEGORY_EXTERN(LogTemplateCharacter, Log, All);

UCLASS(config=Game)
class ARunnerCharacter : public ACharacter
{
	GENERATED_BODY()

	/** Camera boom positioning the camera behind the character */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Camera, meta = (AllowPrivateAccess = "true"))
	USpringArmComponent* CameraBoom;

	/** Follow camera */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Camera, meta = (AllowPrivateAccess = "true"))
	UCameraComponent* FollowCamera;
	
	/** MappingContext */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	UInputMappingContext* DefaultMappingContext;

	/** Jump Input Action */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	UInputAction* JumpAction;

	/** Move Input Action */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	UInputAction* MoveAction;

	/** Look Input Action */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	UInputAction* LookAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input", meta = (AllowPrivateAccess = "true"))
	UInputAction* DownAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input", meta = (AllowPrivateAccess = "true"))
	UInputAction* ThrustAction;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug", meta = (AllowPrivateAccess = "true"))
	bool bAutoMove = false;

	float YawTurnInput = 0.0f;

public:
	ARunnerCharacter(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	void TakeHitImpact();
	void NotifyActorBeginOverlap(AActor* OtherActor) override;
	void NotifyHit(class UPrimitiveComponent* MyComp, AActor* Other, class UPrimitiveComponent* OtherComp, bool bSelfMoved, FVector HitLocation, FVector HitNormal, FVector NormalImpulse, const FHitResult& Hit) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skate Controller")
	float TurnSpeedScale = 0.2f;

	UFUNCTION(BlueprintCallable, Category = "Skate Input")
	float GetDownValue() const;
	
	UFUNCTION(BlueprintCallable, Category = "Skate Input")
	float GetTurnValue() const;

protected:

	/** Called for movement input */
	void Move(const FInputActionValue& Value);

	/** Called for looking input */
	void Look(const FInputActionValue& Value);
	
	void Tick(float Delta) override;
	void TurnDirection(float Delta);

	void StartThrust();
	void StopThrust();

	void BeginPlay() override;

protected:

	virtual void NotifyControllerChanged() override;

	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

public:
	/** Returns CameraBoom subobject **/
	FORCEINLINE class USpringArmComponent* GetCameraBoom() const { return CameraBoom; }
	/** Returns FollowCamera subobject **/
	FORCEINLINE class UCameraComponent* GetFollowCamera() const { return FollowCamera; }

// 生命值和氮气
	UPROPERTY(BlueprintReadWrite, Category = "Health")
	float Health;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Health")
	float MaxHealth = 100.0f;

	UPROPERTY(BlueprintReadWrite, Category = "Health")
	float Mana;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Health")
	float MinimumThrustMana = 20.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Health")
	float MaxMana = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Health")
	float ManaReduceSpeed = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Health")
	float HitDamage = 15.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Health")
	float HitSlomo = 0.1f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Health")
	float HitSlomoTime = 0.5f;
	
	bool bThrusting = false;

// UI Settings
	UFUNCTION(BlueprintCallable, BlueprintImplementableEvent, Category = "UI")
	void ShowLandUI(ELevel LandLevel);

	UFUNCTION(BlueprintCallable, BlueprintImplementableEvent, Category = "UI")
	void ShowDamageUI();

	void InitUI();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI")
	TSubclassOf<class UHealthBarWidget> HealthBarWidgetClass;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI")
	TSubclassOf<class UHealthBarWidget> ManaBarWidgetClass;

	// UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI")
	

	UPROPERTY()
	class UHealthBarWidget* HealthBarWidget = nullptr;
	UPROPERTY()
	class UHealthBarWidget* ManaBarWidget = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI")
	class UNiagaraComponent* LThrust;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI")
	class UNiagaraComponent* RThrust;
};

