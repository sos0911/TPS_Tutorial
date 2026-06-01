// TPS 캐릭터 클래스 헤더 파일


#pragma once


#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "Consts/TPSConsts.h"
#include "GameFramework/Character.h"
#include "GameplayEffectTypes.h"
#include "Logic/ITPSInteractionActorInterface.h"
#include "Logic/Common/TPSTypes.h"
#include "TPSCharacter.generated.h"


class UGameplayAbility;
class UGameplayEffect;
class UInputAction;
class UChildActorComponent;
class USkeletalMeshComponent;
class UTPSAbilitySystemComponent;
class UTPSAttributeSet;
class UTPSGameplayAbility;
struct FInputActionValue;


// 사망 시 브로드캐스트되는 델리게이트
DECLARE_DYNAMIC_MULTICAST_DELEGATE( FOnTPSCharacterDeath );


UCLASS()
class TPS_TUTORIAL_API ATPSCharacter : public ACharacter, public IAbilitySystemInterface, public ITPSPickUpInteractionActorInterface, public ITPSEquipInteractionActorInterface
{
	GENERATED_BODY()

protected:
	// 이동 IA
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Input" )
	TObjectPtr< UInputAction > MoveAction;

	// pitch 회전값
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Input" )
	float Pitch = 0.0f;

	// Roll 값
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Input" )
	float Roll = 0.0f;

	// TPS 모드인가 여부
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Input" )
	bool IsTPSMode = true;

	// zoom 모드인가 여부
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Input" )
	bool IsZoomMode = false;

	// 현재 무기 타입
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "State" )
	EWeaponType CurrentWeaponType = EWeaponType::Max;

	// 현재 무기 발사 중인가 여부
	UPROPERTY( EditAnywhere, BlueprintReadWrite, Category = "State" )
	bool IsFiring = false;

	// 현재 어느 방향으로 이동 중인지?
	UPROPERTY( EditAnywhere, BlueprintReadWrite, Category = "State" )
	ECharacterMoveDirection MovingDirection = ECharacterMoveDirection::Max;

	// GAS — 어빌리티 시스템 컴포넌트
	UPROPERTY( VisibleAnywhere, BlueprintReadOnly, Category = "GAS" )
	TObjectPtr< UTPSAbilitySystemComponent > AbilitySystemComponent;

	// GAS — 어트리뷰트 세트
	UPROPERTY()
	TObjectPtr< UTPSAttributeSet > AttributeSet;

	// 캐릭터 스폰 시 부여되는 기본 어빌리티 목록
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "GAS" )
	TArray< TSubclassOf< UTPSGameplayAbility > > DefaultAbilities;

	// 캐릭터 스폰 시 적용되는 기본 GE 목록 (예: 어트리뷰트 초기화)
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "GAS" )
	TArray< TSubclassOf< UGameplayEffect > > DefaultEffects;

	// Sprint 입력 액션
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Input" )
	TObjectPtr< UInputAction > SprintAction;

	// 사망 멀티캐스트 델리게이트
	UPROPERTY( BlueprintAssignable, Category = "GAS" )
	FOnTPSCharacterDeath OnDeath;

private:
	enum class ERotationType
	{
		Pitch, // pitch
		Roll,  // roll
		Yaw    // yaw
	};

private:
	const FString TPSCameraCompName     = TEXT( "TPSCamera" ); // TPS 카메라 컴포넌트 이름
	const FString TPSZoomCameraCompName = TEXT( "TPSZoomCamera" ); // TPS 줌 카메라 컴포넌트 이름
	const FString FPSCameraCompName     = TEXT( "FPSCamera" ); // FPS 카메라 컴포넌트 이름

	UChildActorComponent*   TPSCameraComp     = nullptr; // TPS 카메라 컴포넌트 객체
	UChildActorComponent*   TPSZoomCameraComp = nullptr; // TPS 줌 카메라 컴포넌트 객체
	UChildActorComponent*   FPSCameraComp     = nullptr; // FPS 카메라 컴포넌트 객체
	USkeletalMeshComponent* FaceComp		  = nullptr; // 얼굴 컴포넌트 객체
	USkeletalMeshComponent* BodyComp		  = nullptr; // 몸통 컴포넌트 객체

	UChildActorComponent*   CurrentCameraComp = nullptr; // 현재 사용 중인 카메라 컴포넌트 객체
	TPSActorPtr				CurrentWeapon	  = nullptr; // 현재 장착중인 무기 객체

	bool IsLeaning  = false; // 기울이고 있는가 여부
	bool IsJumping  = false; // 점프하고 있는가 여부

	float TargetRollValue = 0.0f; // 목표 기울이기 값

public:
	// 현재 Lean Roll 값을 반환한다.
	UFUNCTION( BlueprintPure, Category = "Camera Control" )
	float GetLeanRoll() const { return Roll; }

protected:
	// 이동한다.
	UFUNCTION( BlueprintCallable, Category = "Character Control" )
	void Move( const FInputActionValue& Value );

	// 시점을 이동한다.
	UFUNCTION( BlueprintCallable, Category = "Character Control" )
	void LookAround( const FInputActionValue& Value );

	// 상체를 기울인다.
	UFUNCTION( BlueprintCallable, Category = "Character Control" )
	void Lean( const FInputActionValue& Value );

	// 점프한다.
	UFUNCTION( BlueprintCallable, Category = "Character Control" )
	void DoJump( const FInputActionValue& Value );

	// 무기를 드랍한다.
	UFUNCTION( BlueprintCallable, Category = "Character Control" )
	void Drop( const FInputActionValue& Value );

	// 카메라 시점을 변경한다.
	UFUNCTION( BlueprintCallable, Category = "Camera Control" )
	void ToggleCameraMode( const FInputActionValue& Value );

	// 줌 시점을 변경한다.
	UFUNCTION( BlueprintCallable, Category = "Camera Control" )
	void ToggleZoomMode( const FInputActionValue& Value );

	// 무기를 발사하는 상호작용을 실행한다.
	UFUNCTION( BlueprintCallable, Category = "Interaction Control" )
	virtual bool HandleFireWeaponInteract() override;

	// 무기를 발사한다.
	UFUNCTION( BlueprintCallable, Category = "Interaction Control" )
	void Fire( const bool InIsFiring );

public:
	// Sets default values for this character's properties
	ATPSCharacter();

	// Called every frame
	virtual void Tick( float DeltaTime ) override;

	// Called to bind functionality to input
	virtual void SetupPlayerInputComponent( class UInputComponent* PlayerInputComponent ) override;

	// IAbilitySystemInterface
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	// 오버랩이 시작되었음을 알리는 이벤트를 처리한다.
	UFUNCTION()
	void OnBeginOverlap( UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult );

	// 현재 무기 데이터를 반환한다.
	FWeaponTableData GetWeaponData() const;

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	// Called when the actor is being removed from the level / destroyed
	virtual void EndPlay( const EEndPlayReason::Type EndPlayReason ) override;

	// 무기를 줍는 상호작용을 실행한다.
	virtual bool HandlePickUpWeaponInteract( AActor* OtherActor ) override;

	// Sprint 입력 처리 (Started)
	UFUNCTION( BlueprintCallable, Category = "GAS" )
	void OnSprintPressed( const FInputActionValue& Value );

	// Sprint 입력 처리 (Completed/Canceled)
	UFUNCTION( BlueprintCallable, Category = "GAS" )
	void OnSprintReleased( const FInputActionValue& Value );

private:
	// 컨트롤러 인풋을 더한다.
	void _AddControllerInput( const ERotationType RotationType, const float Value );

	// HUD UI를 토글한다.
	void _ToggleHUDUI( const bool bOn );

	// GAS — ASC ActorInfo 초기화 + 기본 어빌리티/GE 부여
	void _InitAbilitySystem();

	// GAS — Health 변경 핸들러 (사망 트리거)
	void _HandleHealthChanged( float NewValue, float OldValue );

	// GAS — 사망 처리 (라그돌, 입력 비활성)
	void _HandleOnDeath();
};
