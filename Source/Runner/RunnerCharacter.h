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

DECLARE_LOG_CATEGORY_EXTERN(LogRunnerCharacter, Log, All);

UCLASS(config = Game)
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

	void TakeHitImpact(bool bOnlyUI);
	void NotifyActorBeginOverlap(AActor* OtherActor) override;
	void NotifyHit(class UPrimitiveComponent* MyComp, AActor* Other, class UPrimitiveComponent* OtherComp, bool bSelfMoved, FVector HitLocation, FVector HitNormal, FVector NormalImpulse, const FHitResult& Hit) override;

	void FellOutOfWorld(const class UDamageType& dmgType) override;

	UFUNCTION()
	void DealBarrierOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
			UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep,
			FHitResult const& SweepResult);

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
	float InitialManaScale = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Health")
	float ManaReduceSpeed = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Health")
	float HitDamage = 15.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Health")
	float HitSlomo = 0.1f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Health")
	float HitSlomoTime = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Health")
	int32 FlipTime = 4;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Health")
	float FlipInterval = 0.5f;

	void UpdateFlicker(float Delta);

	// 当 evil 领先这个距离时触发伤害逻辑
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Evil Chase")
	float DeltaTakeDamge = 0.1f;

	// 触发伤害的间隔时间
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Evil Chase")
	float DamageInterval = 1.0f; 

	void CheckEvilChase(float Delta);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug")
	bool bInfiniteHealth = false;

	bool bThrusting = false;

	bool bFlicker = false;
	bool bMaterialFlipped = false;

	float CurrentFlickerTime = 0.0f;

	float CurrentDamageInterval = 0.5f;

	// UI Settings
	UFUNCTION(BlueprintCallable, BlueprintImplementableEvent, Category = "UI")
	void ShowLandUI(ELevel LandLevel);

	UFUNCTION(BlueprintCallable, BlueprintImplementableEvent, Category = "UI")
	void ShowDamageUI(bool bOnlyUI);

	void InitUI();

	UPROPERTY(EditAnywhere, Category = "Health")
	float Opacity = 0.3f;

	// UPROPERTY(EditAnywhere, Category = "Health")
	// TObjectPtr<class UMaterialInterface> DamageMaterial;

	// UPROPERTY()
	// TObjectPtr<class UMaterialInterface> TmpMaterialSlot;

	void ExchangeMaterial();

	UPROPERTY(EditAnywhere, Category = "UI")
	class TSubclassOf<class UGameUIUserWidget> GameUIWidgetClass;

	UPROPERTY()
	class UGameUIUserWidget* GameUIWidget = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI")
	class UNiagaraComponent* LThrust;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI")
	class UNiagaraComponent* RThrust;

private:
		class AWorldGenerator* WorldGenerator = nullptr;
};
