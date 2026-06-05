// TPS 캐릭터 클래스 소스 파일


#include "TPSCharacter.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "EnhancedInputComponent.h"
#include "InputActionValue.h"
#include "KismetAnimationLibrary.h"
#include "Actors/TPSPickUpBase.h"
#include "Actors/TPSShotImpactField.h"
#include "Actors/Components/TPSDataComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Consts/TPSConsts.h"
#include "Engine/SkeletalMeshSocket.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameInstance/TPSGameInstance.h"
#include "GAS/TPSAbilitySystemComponent.h"
#include "GAS/TPSAttributeSet.h"
#include "GAS/TPSGameplayTags.h"
#include "GAS/Abilities/TPSGameplayAbilityBase.h"
#include "GAS/Effects/TPSGameplayEffect_StaminaRegen.h"
#include "Kismet/KismetMathLibrary.h"
#include "Log/TPSLog.h"
#include "Logic/ITPSInteractionActorInterface.h"
#include "Manager/TPSUIManager.h"
#include "Net/UnrealNetwork.h"
#include "UI/TPSHUD.h"
#include "Actors/TPSEquipSniperRifle.h"
#include "Util/TPSUtil.h"


// 캐릭터 기본값과 GAS 서브오브젝트(ASC/AttributeSet)를 생성한다.
ATPSCharacter::ATPSCharacter()
{
 	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	// GAS — ASC / AttributeSet 서브오브젝트
	AbilitySystemComponent = CreateDefaultSubobject< UTPSAbilitySystemComponent >( TEXT( "AbilitySystemComponent" ) );
	AbilitySystemComponent->SetIsReplicated( true );

	AttributeSet = CreateDefaultSubobject< UTPSAttributeSet >( TEXT( "AttributeSet" ) );

	// 스태미나 자동 회복 GE 기본 클래스 지정 (BP에서 오버라이드 가능)
	StaminaRegenEffect = UTPSGameplayEffect_StaminaRegen::StaticClass();
}

// ASC를 반환한다 (IAbilitySystemInterface 구현).
UAbilitySystemComponent* ATPSCharacter::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

// 게임 시작/스폰 시 컴포넌트를 캐싱하고 GAS를 초기화한다.
void ATPSCharacter::BeginPlay()
{
	Super::BeginPlay();

	TArray< UChildActorComponent* > childActorComponents;
	GetComponents( UChildActorComponent::StaticClass(), childActorComponents );

	for ( UChildActorComponent* childActorComponent : childActorComponents )
	{
		if ( !childActorComponent ) continue;

		if ( childActorComponent->GetName().Equals( TPSCameraCompName ) )
		{
			TPSCameraComp = childActorComponent;
		}
		else if ( childActorComponent->GetName().Equals( TPSZoomCameraCompName ) )
		{
			TPSZoomCameraComp = childActorComponent;
		}
		else if ( childActorComponent->GetName().Equals( FPSCameraCompName ) )
		{
			FPSCameraComp = childActorComponent;
		}
	}

	TArray< USkeletalMeshComponent* > meshComponents;
	GetComponents( USkeletalMeshComponent::StaticClass(), meshComponents );

	for ( USkeletalMeshComponent* meshComponent : meshComponents )
	{
		if ( !meshComponent ) continue;

		if ( meshComponent->GetName().Equals( "Face" ) )
		{
			FaceComp = meshComponent;
		}
		else if ( meshComponent->GetName().Equals( "Body" ) )
		{
			BodyComp = meshComponent;
		}
	}

	IsTPSMode  = true;
	IsZoomMode = false;

	_ToggleHUDUI( false );

	if ( UCapsuleComponent* capsuleComp = GetCapsuleComponent() )
	{
		capsuleComp->OnComponentBeginOverlap.AddDynamic( this, &ATPSCharacter::OnBeginOverlap );
	}

	CurrentCameraComp = IsTPSMode ? TPSCameraComp : FPSCameraComp;

	_InitAbilitySystem();
}

// 종료 시 어트리뷰트 변경 구독을 해제한다 (댕글링 방지).
void ATPSCharacter::EndPlay( const EEndPlayReason::Type EndPlayReason )
{
	// 어트리뷰트 변경 구독 해제 (댕글링 방지)
	if ( AttributeSet )
	{
		AttributeSet->OnHealthChanged.RemoveAll( this );
		AttributeSet->OnStaminaChanged.RemoveAll( this );
	}

	Super::EndPlay( EndPlayReason );
}

// 복제 프로퍼티를 등록한다.
void ATPSCharacter::GetLifetimeReplicatedProps( TArray< FLifetimeProperty >& OutLifetimeProps ) const
{
	Super::GetLifetimeReplicatedProps( OutLifetimeProps );

	DOREPLIFETIME( ATPSCharacter, CurrentWeapon );
	DOREPLIFETIME( ATPSCharacter, CurrentWeaponType );
	DOREPLIFETIME( ATPSCharacter, CurrentBullet );
}

// 무기를 줍는 상호작용을 실행한다.
bool ATPSCharacter::HandlePickUpWeaponInteract( AActor* OtherActor )
{
	// 무기를 이미 장착 중이라면 추가로 주울 수 없다.
	if ( IsValid( CurrentWeapon ) ) return false;

	ATPSPickUpBase* pickUpActor = Cast< ATPSPickUpBase >( OtherActor );
	if ( !pickUpActor ) return false;

	UTPSDataComponent* dataComponent = TPSUtil::GetValueForObjProp< UTPSDataComponent >( pickUpActor );
	if ( !dataComponent ) return false;

	const FWeaponTableData* weaponData = dataComponent->GetData< FWeaponTableData >();
	if ( !weaponData ) return false;

	if ( !BodyComp ) return false;

	FName socketName = TEXT( "" );

	switch ( weaponData->WeaponType )
	{
	case EWeaponType::Pistol:
		{
			socketName = TEXT( "Pistol" );
		}
		break;
	case EWeaponType::AssaultRifle:
		{
			socketName = TEXT( "AssaultRifle" );
		}
		break;
	case EWeaponType::SniperRifle:
		{
			socketName = TEXT( "SniperRifle" );
		}
		break;
	case EWeaponType::RocketLauncher:
		{
			socketName = TEXT( "RocketLauncher" );
		}
		break;
	case EWeaponType::GrenadeLauncher:
		{
			socketName = TEXT( "GrenadeLauncher" );
		}
		break;
	case EWeaponType::Shotgun:
		{
			socketName = TEXT( "Shotgun" );
		}
		break;
	case EWeaponType::Knife:
		{
			socketName = TEXT( "Knife" );
		}
		break;
	}

	if ( !BodyComp->DoesSocketExist( socketName ) ) return false;

	UWorld* world = GetWorld();
	if ( !world ) return false;

	FActorSpawnParameters spawnParams;
	spawnParams.Owner                          = this;
	spawnParams.Instigator                     = GetInstigator();
	spawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AActor* weapon = world->SpawnActor< AActor >( weaponData->EquipWeapon, FTransform::Identity, spawnParams );
	if ( !weapon ) return false;

	FAttachmentTransformRules attachmentRules( EAttachmentRule::SnapToTarget, true );
	weapon->AttachToComponent( BodyComp, attachmentRules, socketName );

	CurrentWeapon = weapon;
	CurrentWeaponType = weaponData->WeaponType;

	// 장착 시 탄창을 가득 채우고 HUD 장탄수를 동기화한다.
	CurrentBullet = weaponData->MagazineSize;

	if ( IsTPSMode || !IsZoomMode ) _ToggleHUDUI( true );

	_RefreshWeaponHUD();

	return true;
}

// 컨트롤러 인풋을 더한다.
void ATPSCharacter::_AddControllerInput( const ERotationType RotationType, const float Value )
{
	switch ( RotationType )
	{
	case ERotationType::Pitch:
		{
			const float afPitch = FMath::Clamp( Pitch + Value, -25.0f, 25.0f );
			AddControllerPitchInput( afPitch - Pitch );
			Pitch = afPitch;
		}
		break;
	case ERotationType::Yaw:
		{
			AddControllerYawInput( Value );
		}
		break;
	}
}

// HUD UI를 토글한다.
void ATPSCharacter::_ToggleHUDUI( const bool bOn )
{
	if ( !IsLocallyControlled() ) return;
	
	UTPSHUD* hudUI = _GetHUDUI();
	if ( !hudUI ) return;

	hudUI->ToggleCrosshair( bOn );
}

// 현재 HUD 위젯을 반환한다.
UTPSHUD* ATPSCharacter::_GetHUDUI() const
{
	if ( !IsLocallyControlled() ) return nullptr;
	
	UTPSGameInstance* gameInstance = UTPSGameInstance::GetGameInstance();
	if ( !gameInstance ) return nullptr;

	UTPSUIManager* uiManager = gameInstance->GetUIManager();
	if ( !uiManager ) return nullptr;

	return Cast< UTPSHUD >( uiManager->FindWidget( UTPSHUD::StaticClass() ) );
}

// 매 프레임 기울이기(Roll)와 점프 상태를 갱신한다.
void ATPSCharacter::Tick( float DeltaTime )
{
	Super::Tick( DeltaTime );

	if ( IsLeaning )
	{
		if ( !FMath::IsNearlyEqual( Roll, TargetRollValue ) ) Roll = FMath::FInterpTo( Roll, TargetRollValue, DeltaTime, 5.0f );
	}
	else
	{
		if ( !FMath::IsNearlyEqual( Roll, 0.0f ) ) Roll = FMath::FInterpTo( Roll, 0.0f, DeltaTime, 5.0f );
	}

	if ( IsJumping && GetCharacterMovement() && !GetCharacterMovement()->IsFalling() ) IsJumping = false;
}

// 스프린트 입력 시작 시 Sprint 어빌리티를 활성화한다.
void ATPSCharacter::OnSprintPressed( const FInputActionValue& /*Value*/ )
{
	if ( !AbilitySystemComponent ) return;
	AbilitySystemComponent->TryActivateAbilityByTag( TAG_Ability_Sprint );
}

// 스프린트 입력 해제 시 Sprint 어빌리티를 취소한다.
void ATPSCharacter::OnSprintReleased( const FInputActionValue& /*Value*/ )
{
	if ( !AbilitySystemComponent ) return;
	AbilitySystemComponent->CancelAbilityByTag( TAG_Ability_Sprint );
}

// ASC ActorInfo를 초기화하고 기본 GE와 어빌리티를 부여한다.
void ATPSCharacter::_InitAbilitySystem()
{
	if ( !AbilitySystemComponent || !AttributeSet ) return;

	// ASC ActorInfo 초기화 (Owner = Avatar = this; 단일 플레이어 전제)
	AbilitySystemComponent->InitAbilityActorInfo( this, this );

	// Health 변경 구독 (사망 트리거).
	// AddUObject를 사용하여 EndPlay의 RemoveAll(this)와 정확히 매칭되도록 한다.
	AttributeSet->OnHealthChanged.AddUObject( this, &ATPSCharacter::_HandleHealthChanged );

	// Stamina 변경 구독 (HUD 스태미너 바 갱신).
	AttributeSet->OnStaminaChanged.AddUObject( this, &ATPSCharacter::_HandleStaminaChanged );

	// 기본 GE (어트리뷰트 초기화) 적용
	for ( const TSubclassOf< UGameplayEffect >& effectClass : DefaultEffects )
	{
		if ( !effectClass ) continue;

		FGameplayEffectContextHandle ctx = AbilitySystemComponent->MakeEffectContext();
		ctx.AddSourceObject( this );

		const FGameplayEffectSpecHandle specHandle = AbilitySystemComponent->MakeOutgoingSpec( effectClass, 1.0f, ctx );
		if ( specHandle.IsValid() )
		{
			AbilitySystemComponent->ApplyGameplayEffectSpecToSelf( *specHandle.Data.Get() );
		}
	}

	// 스태미나 자동 회복 GE 적용 (Infinite — 스프린트 중엔 GE가 스스로 억제됨)
	if ( StaminaRegenEffect )
	{
		FGameplayEffectContextHandle ctx = AbilitySystemComponent->MakeEffectContext();
		ctx.AddSourceObject( this );

		const FGameplayEffectSpecHandle specHandle = AbilitySystemComponent->MakeOutgoingSpec( StaminaRegenEffect, 1.0f, ctx );
		if ( specHandle.IsValid() )
		{
			AbilitySystemComponent->ApplyGameplayEffectSpecToSelf( *specHandle.Data.Get() );
		}
	}

	// 기본 어빌리티 부여
	for ( const TSubclassOf< UTPSGameplayAbilityBase >& abilityClass : DefaultAbilities )
	{
		if ( !abilityClass ) continue;

		AbilitySystemComponent->GiveAbility( FGameplayAbilitySpec( abilityClass, 1, INDEX_NONE, this ) );
	}

	UE_LOG( LogGameplay, Log, TEXT( "[TPS] GAS Initialized — Health=%.1f / MaxHealth=%.1f, Stamina=%.1f / MaxStamina=%.1f" ),
		AttributeSet->GetHealth(),    AttributeSet->GetMaxHealth(),
		AttributeSet->GetStamina(),   AttributeSet->GetMaxStamina() );
}

// 체력 변경을 받아 0 이하이면 사망 처리한다.
void ATPSCharacter::_HandleHealthChanged( float NewValue, float MaxValue, float /*OldValue*/ )
{
	if ( NewValue <= 0.0f )
	{
		_HandleOnDeath();
	}
}

// 스태미나 변경을 받아 HUD 스태미너 바를 갱신한다.
void ATPSCharacter::_HandleStaminaChanged( float NewValue, float MaxValue, float /*OldValue*/ )
{
	if ( !AttributeSet ) return;

	const float percent = MaxValue > KINDA_SMALL_NUMBER ? NewValue / MaxValue : 0.0f;

	if ( UTPSHUD* hudUI = _GetHUDUI() )
	{
		hudUI->SetStaminaPercent( percent );
	}

	// 스태미나 고갈 시 스프린트 강제 종료 (드레인 GE 제거 + 속도 원복은 EndAbility가 처리)
	if ( NewValue <= 0.0f && AbilitySystemComponent )
	{
		AbilitySystemComponent->CancelAbilityByTag( TAG_Ability_Sprint );
	}
}

// 사망 태그 부여, 어빌리티 취소, 입력 차단, 라그돌 처리를 한다.
void ATPSCharacter::_HandleOnDeath()
{
	// 이미 사망 태그가 있으면 중복 처리 방지
	if ( AbilitySystemComponent && AbilitySystemComponent->HasMatchingGameplayTag( TAG_State_Dead ) )
	{
		return;
	}

	if ( AbilitySystemComponent )
	{
		// 사망 상태 태그 부여 (loose tag — GE 없이 즉시 부여)
		AbilitySystemComponent->AddLooseGameplayTag( TAG_State_Dead );

		// 활성 어빌리티 모두 취소
		AbilitySystemComponent->CancelAllAbilities();
	}

	// 입력 비활성
	if ( APlayerController* pc = GetController< APlayerController >() )
	{
		DisableInput( pc );
	}

	// 라그돌
	if ( USkeletalMeshComponent* mesh = GetMesh() )
	{
		mesh->SetCollisionProfileName( TEXT( "Ragdoll" ) );
		mesh->SetSimulatePhysics( true );
	}

	// 5초 뒤 자동 정리
	SetLifeSpan( 5.0f );

	OnDeath.Broadcast();

	UE_LOG( LogGameplay, Log, TEXT( "[TPS] OnDeath Broadcast" ) );
}

// 오버랩이 시작되었음을 알리는 이벤트를 처리한다.
void ATPSCharacter::OnBeginOverlap( UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult )
{
	ITPSPickUpInteractionActorInterface* interactionPickUpActorInterface = Cast< ITPSPickUpInteractionActorInterface >( OtherActor );
	if ( !interactionPickUpActorInterface ) return;

	if ( HandlePickUpWeaponInteract( OtherActor ) ) interactionPickUpActorInterface->HandlePickUpWeaponInteract( this );
}

// 현재 무기 데이터를 반환한다.
FWeaponTableData ATPSCharacter::GetWeaponData() const
{
	UTPSDataComponent* dataComponent = TPSUtil::GetValueForObjProp< UTPSDataComponent >( CurrentWeapon.Get() );
	if ( !dataComponent ) return FWeaponTableData();

	const FWeaponTableData* weaponData = dataComponent->GetData< FWeaponTableData >();
	if ( !weaponData ) return FWeaponTableData();

	return *weaponData;
}

// 이동한다.
void ATPSCharacter::Move( const FInputActionValue& Value )
{
	if ( Value.GetValueType() != EInputActionValueType::Axis2D ) return;

	UE_LOG( LogGameplay, Log, TEXT( "move value : { %f %f }" ), Value.Get< FVector2D >().X, Value.Get< FVector2D >().Y );

	double xValue = Value.Get< FVector2D >().X;
	double yValue = Value.Get< FVector2D >().Y;

	FRotator rotator = GetControlRotation();

	UE_LOG( LogGameplay, Log, TEXT( "rotate value : { %f %f }" ), rotator.Pitch, rotator.Yaw );

	// TODO : 아래 계산 공식에서 Roll 이 RightVector 뽑는 데 필요한가? wasd 모두 yaw 만 필요하지 않나 싶은데..
	if ( FMath::Abs( xValue ) > 0 )
	{
		FVector movementVector = UKismetMathLibrary::GetRightVector( FRotator( rotator.Pitch, rotator.Yaw, 0.0f ) );
		UE_LOG( LogGameplay, Log, TEXT( "Add Movement Input : [%f, %f, %f]" ), movementVector.X, movementVector.Y, movementVector.Z );

		AddMovementInput( movementVector, xValue );
	}
	if ( FMath::Abs( yValue ) > 0 )
	{
		FVector movementVector = UKismetMathLibrary::GetForwardVector( FRotator( 0.0f, rotator.Yaw, 0.0f  ) );
		UE_LOG( LogGameplay, Log, TEXT( "Add Movement Input : [%f, %f, %f]" ), movementVector.X, movementVector.Y, movementVector.Z );

		AddMovementInput( movementVector, yValue );
	}

	// NOTE : 방법 1 - 실제 움직이는 방향으로 애니메이션 결정.
	float directionValue = UKismetAnimationLibrary::CalculateDirection( GetVelocity(), GetActorRotation() );
	UE_LOG( LogGameplay, Log, TEXT( "directionValue : %f" ), directionValue );
	if ( directionValue >= -45.0f && directionValue < 45.0f )      MovingDirection = ECharacterMoveDirection::Forward;
	else if ( directionValue >= 45.0f && directionValue < 135.0f ) MovingDirection = ECharacterMoveDirection::Right;
	else if ( FMath::Abs( directionValue ) >= 135.0f )             MovingDirection = ECharacterMoveDirection::Backward;
	else											               MovingDirection = ECharacterMoveDirection::Left;

	// NOTE : 방법 2 - 누르는 방향키로 애니메이션 결정.
}

// 시점을 이동한다.
void ATPSCharacter::LookAround( const FInputActionValue& Value )
{
	if ( Value.GetValueType() != EInputActionValueType::Axis2D ) return;

	// NOTE : 회전값 정규화 형식이 컨트롤러 피치값에서 다르게 이루어져 이 방식은 주석 처리한다.
	// APlayerController* playerController = Cast< APlayerController >( GetController() );
	// if ( !playerController ) return;
	//
	// FRotator ctrlRot = playerController->GetControlRotation();
	//
	// // UE_LOG( LogGameplay, Log, TEXT( "mouse input Y : [ %f ], bef pitch : [ %f ]" ), Value.Get< FVector2D >().Y, Pitch );
	//
	// const float desiredPitch = FMath::Clamp( ctrlRot.Pitch + Value.Get< FVector2D >().Y, -60.0f, 60.0f );
	// const float deltaPitch   = FMath::FindDeltaAngleDegrees( ctrlRot.Pitch, desiredPitch );
	//
	// UE_LOG( LogGameplay, Log, TEXT( "ctrlRot.Pitch : [ %f ],  mouse input Y : [ %f ], desiredPitch : [ %f ], deltaPitch : [ %f ]" ), ctrlRot.Pitch, Value.Get< FVector2D >().Y, desiredPitch, deltaPitch );
	//
	// AddControllerYawInput( Value.Get< FVector2D >().X );
	// AddControllerPitchInput( deltaPitch );
	//
	// Pitch = desiredPitch;

	_AddControllerInput( ERotationType::Yaw,   Value.Get< FVector2D >().X );
	_AddControllerInput( ERotationType::Pitch, Value.Get< FVector2D >().Y );
}

// 상체를 기울인다.
void ATPSCharacter::Lean( const FInputActionValue& Value )
{
	if ( Value.GetValueType() != EInputActionValueType::Axis1D ) return;
	if ( IsJumping ) return;

	const float axisValue = Value.Get< float >();
	IsLeaning = FMath::Abs( axisValue ) > KINDA_SMALL_NUMBER;

	TargetRollValue = FMath::GetMappedRangeValueClamped( FVector2f( -1.0f, 1.0f ), FVector2f( -10.0f, 10.0f ), axisValue );
}

// 점프한다.
void ATPSCharacter::DoJump( const FInputActionValue& Value )
{
	if ( Value.GetValueType() != EInputActionValueType::Boolean ) return;
	if ( !GetCharacterMovement() ) return;
	// 공중에 있는 동안은 점프할 수 없다.
	// if ( GetCharacterMovement()->IsFalling() ) return;
	if ( IsJumping ) return;

	Jump();

	IsJumping = true;
}

// 무기를 드랍한다.
void ATPSCharacter::Drop( const FInputActionValue& Value )
{
	if ( Value.GetValueType() != EInputActionValueType::Boolean ) return;
	if ( !IsValid( CurrentWeapon ) ) return;

	UTPSDataComponent* dataComponent = TPSUtil::GetValueForObjProp< UTPSDataComponent >( CurrentWeapon.Get() );
	if ( !dataComponent ) return;

	const FWeaponTableData* weaponData = dataComponent->GetData< FWeaponTableData >();
	if ( !weaponData ) return;

	const FVector  dropLocation = GetActorLocation() + GetActorForwardVector() * 200.0f;
	const FRotator dropRotation = GetActorRotation();

	FActorSpawnParameters spawnParams;
	// spawnParams.Owner                          = nullptr;
	// spawnParams.Instigator                     = GetInstigator();
	spawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	UWorld* world = GetWorld();
	if ( !world ) return;

	world->SpawnActor< AActor >( weaponData->PickUpWeapon, FTransform( dropRotation, dropLocation ), spawnParams );

	// 줌 상태에서 드랍하는 경우 카메라와 줌 상태를 원복한다.
	if ( IsZoomMode )
	{
		IsZoomMode = false;
		CurrentCameraComp = IsTPSMode ? TPSCameraComp : FPSCameraComp;

		if ( APlayerController* playerController = Cast< APlayerController >( GetController() ) )
		{
			AActor* viewTarget = CurrentCameraComp ? CurrentCameraComp->GetChildActor() : nullptr;
			if ( viewTarget ) playerController->SetViewTargetWithBlend( viewTarget, 0.2f );
		}
	}

	CurrentWeapon->Destroy();
	CurrentWeapon = nullptr;
	CurrentWeaponType = EWeaponType::None;

	// 장탄수/재장전 상태 초기화 (진행 중이던 재장전 타이머 취소)
	CurrentBullet = 0;
	IsReloading   = false;
	GetWorldTimerManager().ClearTimer( ReloadTimerHandle );

	_ToggleHUDUI( false );
	_RefreshWeaponHUD();
}

// 카메라 시점을 변경한다.
void ATPSCharacter::ToggleCameraMode( const FInputActionValue& Value )
{
	if ( Value.GetValueType() != EInputActionValueType::Boolean ) return;

	APlayerController* playerController = Cast< APlayerController >( GetController() );
	if ( !playerController ) return;

	if ( !TPSCameraComp || !TPSZoomCameraComp || !FPSCameraComp ) return;

	IsTPSMode = !IsTPSMode;

	float cameraBlendTime = 0.2f;
	playerController->SetViewTargetWithBlend( IsTPSMode ? TPSCameraComp->GetChildActor() : FPSCameraComp->GetChildActor(), cameraBlendTime, VTBlend_Linear, 0, true );
	CurrentCameraComp = IsTPSMode ? TPSCameraComp : FPSCameraComp;

	TWeakObjectPtr< ATPSCharacter > thisPtr = this;
	auto ftrToggleFaceCompVisibility = [ this, thisPtr ] ()
	{
		if ( !thisPtr.IsValid() ) return;

		if ( FaceComp ) FaceComp->SetHiddenInGame( !IsTPSMode, true );
	};

	if ( IsTPSMode )
	{
		ftrToggleFaceCompVisibility();
	}
	else
	{
		FTimerHandle timerHandle;
		GetWorldTimerManager().SetTimer( timerHandle, ftrToggleFaceCompVisibility, cameraBlendTime, false );
	}
}

// 줌 시점을 변경한다.
void ATPSCharacter::ToggleZoomMode( const FInputActionValue& Value )
{
	if ( Value.GetValueType() != EInputActionValueType::Boolean ) return;

	APlayerController* playerController = Cast< APlayerController >( GetController() );
	if ( !playerController ) return;
	// 1인칭인데 무기가 없는 경우에는 줌을 허용하지 않는다.
	if ( !IsTPSMode && !IsValid( CurrentWeapon ) ) return;

	IsZoomMode = !IsZoomMode;

	float cameraBlendTime = 0.2f;
	if ( IsTPSMode )
	{
		playerController->SetViewTargetWithBlend( IsZoomMode ? TPSZoomCameraComp->GetChildActor() : TPSCameraComp->GetChildActor(), cameraBlendTime );
		CurrentCameraComp = IsZoomMode ? TPSZoomCameraComp : TPSCameraComp;
	}
	else
	{
		if ( IsZoomMode )
		{
			TArray< AActor* > childActors;
			USpringArmComponent* springArmComp = CurrentWeapon->GetComponentByClass< USpringArmComponent >( );
			if ( !springArmComp ) return;

			UChildActorComponent* childActorComp = Cast< UChildActorComponent >( springArmComp->GetChildComponent( 0 ) );
			if ( !childActorComp ) return;

			playerController->SetViewTargetWithBlend( childActorComp->GetChildActor(), cameraBlendTime, VTBlend_Linear, 0, true );
			CurrentCameraComp = childActorComp;
		}
		else
		{
			playerController->SetViewTargetWithBlend( FPSCameraComp->GetChildActor(), cameraBlendTime, VTBlend_Linear, 0, true );
			CurrentCameraComp = FPSCameraComp;
		}
	}

	// 스나이퍼 라이플의 경우 줌 시에만 씬 캡쳐를 활성화한다.
	if ( ATPSEquipSniperRifle* sniperRifle = Cast< ATPSEquipSniperRifle >( CurrentWeapon.Get() ) )
	{
		sniperRifle->SetSceneCaptureEnabled( IsZoomMode );
	}

	if ( !IsValid( CurrentWeapon ) )     _ToggleHUDUI( false );
	else if ( !IsTPSMode && IsZoomMode ) _ToggleHUDUI( false );
	else                                 _ToggleHUDUI( true  );
}

// 무기를 발사하는 상호작용을 실행한다.
bool ATPSCharacter::HandleFireWeaponInteract()
{
	if ( !IsValid( CurrentWeapon ) || CurrentWeaponType == EWeaponType::Max ) return false;

	// 재장전 중에는 발사 불가
	if ( IsReloading ) return false;

	// 탄창이 비었으면 발사 불가 (재장전 필요)
	if ( CurrentBullet <= 0 )
	{
		UE_LOG( LogGameplay, Log, TEXT( "[TPS] Fire blocked — magazine empty (need reload)" ) );
		return false;
	}

	// 탄약 1발 소모 후 HUD 장탄수 동기화
	--CurrentBullet;
	_RefreshWeaponHUD();

	if ( ITPSEquipInteractionActorInterface* interactionEquipActorInterface = Cast< ITPSEquipInteractionActorInterface >( CurrentWeapon.Get() ) )
	{
		interactionEquipActorInterface->HandleFireWeaponInteract();
	}

	FString FireMontagePath = TEXT( "/Game/CustomContents/Animations/" );

	// NOTE : PlayMontage 구현부까지 여기로 옮길 것.
	switch ( CurrentWeaponType )
	{
	case EWeaponType::Pistol:
		{
			FireMontagePath += TEXT( "Pistol/MT_Pistol_Fire.MT_Pistol_Fire" );
		}
		break;
	case EWeaponType::SniperRifle:
		{
			FireMontagePath += TEXT( "SniperRifle/MT_SniperRifle_Fire.MT_SniperRifle_Fire" );
		}
		break;
	}

	UAnimMontage* fireMontage = LoadObject< UAnimMontage >( nullptr, *FireMontagePath );
	if ( !fireMontage ) return false;

	UAnimInstance* animInstance = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr;
	if ( !animInstance ) return false;

	TWeakObjectPtr< ATPSCharacter > thisPtr = this;
	auto ftrFireOff = [ this, thisPtr ] ( UAnimMontage* Montage, bool bInterrupted )
	{
		if ( !thisPtr.IsValid() ) return;

		IsFiring = false;
	};

	PlayAnimMontage( fireMontage );

	if ( FOnMontageBlendingOutStarted* interruptDelegate = animInstance->Montage_GetBlendingOutDelegate( fireMontage ) )
	{
		interruptDelegate->BindLambda( ftrFireOff );
	}
	if ( FOnMontageEnded* endDelegate = animInstance->Montage_GetEndedDelegate( fireMontage ) )
	{
		endDelegate->BindLambda( ftrFireOff );
	}

	// NOTE : 발사 시마다 임의 반동 구현
	_AddControllerInput( ERotationType::Yaw,   FMath::RandRange( -1, 1 ) );
	_AddControllerInput( ERotationType::Pitch, FMath::RandRange( -3, -1 ) );

	if ( USkeletalMeshComponent* weaponMeshComp = CurrentWeapon->GetComponentByClass< USkeletalMeshComponent >() )
	{
		if ( const USkeletalMeshSocket* muzzleSocket = weaponMeshComp->GetSocketByName( TEXT( "Muzzle" ) ) )
		{
			if ( !CurrentCameraComp || !GetWorld() ) return false;

			bool bHit = false;
			// Step 1 : 카메라 위치에서 카메라가 바라보는 방향으로 충돌 검출.
			{
				FVector rayStartLoc = CurrentCameraComp->GetComponentLocation();
				FVector rayEndLoc   = rayStartLoc + CurrentCameraComp->GetForwardVector() * 10000.0f;

				FHitResult hitResult;
				TArray< TEnumAsByte< EObjectTypeQuery > > objTypes =
				{
					UEngineTypes::ConvertToObjectType( ECC_WorldStatic  ),
					UEngineTypes::ConvertToObjectType( ECC_WorldDynamic ),
					UEngineTypes::ConvertToObjectType( ECC_Destructible )
				};

				FCollisionQueryParams queryParams;
				queryParams.AddIgnoredActor( this );

				bHit = GetWorld()->LineTraceSingleByObjectType( hitResult, rayStartLoc, rayEndLoc, FCollisionObjectQueryParams( objTypes ), queryParams );
				if ( bHit )
				{
					if ( GetNetMode() != NM_DedicatedServer )
					{
						FActorSpawnParameters spawnParams;
						spawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

						ATPSShotImpactField* fieldActor = GetWorld()->SpawnActor< ATPSShotImpactField >(
							LoadClass< ATPSShotImpactField >( nullptr, *ATPSShotImpactField::GetPath() ),
							FVector( hitResult.ImpactPoint ), FRotator(), spawnParams );

						FTimerHandle removeImpactFieldTimerHandle;
						TWeakObjectPtr< ATPSShotImpactField > weakFieldActor = fieldActor;
						GetWorldTimerManager().SetTimer( removeImpactFieldTimerHandle, [ weakFieldActor ] ()
						{
							if ( !weakFieldActor.IsValid() ) return;

							weakFieldActor->Destroy();
						}, 0.1f, false );	
					}
				}
			}

			// // Step 2 : 총구에서 충돌 검출된 위치까지 충돌 검출하여 충돌 처리함.
			// // NOTE : 단순히 Step 1처럼 충돌 처리 한번 하고 끝낼 수 있지만 추후 실제 총알 발사 등의 스폰 처리를 위하여 이렇게 일괄 처리한다.
			// {
			// 	FVector rayStartLoc = muzzleSocket->GetSocketLocation( weaponMeshComp );
			// 	FVector rayEndLoc   = bHit ? hitLocation : rayStartLoc + muzzleSocket->GetSocketTransform( weaponMeshComp ).GetUnitAxis( EAxis::X ) * 10000.0f;
			//
			// 	FHitResult hitResult;
			// 	TArray< TEnumAsByte< EObjectTypeQuery > > objTypes =
			// 	{
			// 		UEngineTypes::ConvertToObjectType( ECC_WorldStatic  ),
			// 		UEngineTypes::ConvertToObjectType( ECC_WorldDynamic ),
			// 		UEngineTypes::ConvertToObjectType( ECC_Destructible )
			// 	};
			//
			// 	FCollisionQueryParams queryParams;
			// 	queryParams.AddIgnoredActor( this );
			//
			// 	bool bHit = GetWorld()->LineTraceSingleByObjectType( hitResult, rayStartLoc, rayEndLoc, FCollisionObjectQueryParams( objTypes ), queryParams );
			//
			// 	if ( bHit )
			// 	{
			// 		FActorSpawnParameters spawnParams;
			// 		spawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			//
			// 		ATPSShotImpactField* fieldActor = GetWorld()->SpawnActor< ATPSShotImpactField >(
			// 			LoadClass< ATPSShotImpactField >( nullptr, *ATPSShotImpactField::GetPath() ),
			// 			FVector( hitResult.ImpactPoint ), FRotator(), spawnParams );
			// 	}
			// }
		}
	}

	return true;
}

// 무기를 발사한다.
void ATPSCharacter::Fire( const bool InIsFiring )
{
	if ( IsFiring == InIsFiring ) return;

	IsFiring = InIsFiring;

	if ( IsFiring ) HandleFireWeaponInteract();
}

// 재장전 입력을 받아 재장전을 시작한다.
void ATPSCharacter::OnReload( const FInputActionValue& /*Value*/ )
{
	// 무기 미장착 / 이미 재장전 중이면 무시
	if ( !IsValid( CurrentWeapon ) || CurrentWeaponType == EWeaponType::Max ) return;
	if ( IsReloading ) return;

	// 이미 가득 찬 경우 재장전 불필요
	const int32 magazineSize = GetWeaponData().MagazineSize;
	if ( CurrentBullet >= magazineSize ) return;

	IsReloading = true;

	// NOTE : 여기서 무기 타입별 재장전 몽타주를 재생하고, ReloadTime을 몽타주 길이에 맞추면 더 자연스럽다.

	// ReloadTime 경과 후 탄창 보충
	GetWorldTimerManager().SetTimer( ReloadTimerHandle, this, &ATPSCharacter::_FinishReload, ReloadTime, false );

	UE_LOG( LogGameplay, Log, TEXT( "[TPS] Reload started (%.1fs)" ), ReloadTime );
}

// 재장전을 완료하여 탄창을 보충하고 HUD를 동기화한다.
void ATPSCharacter::_FinishReload()
{
	IsReloading = false;

	// 재장전 도중 무기를 잃었을 수 있으므로 재확인
	if ( !IsValid( CurrentWeapon ) || CurrentWeaponType == EWeaponType::Max ) return;

	CurrentBullet = GetWeaponData().MagazineSize;

	_RefreshWeaponHUD();

	UE_LOG( LogGameplay, Log, TEXT( "[TPS] Reload finished — %d rounds" ), CurrentBullet );
}

// 현재 무기 타입/장탄수를 HUD에 동기화한다.
void ATPSCharacter::_RefreshWeaponHUD() const
{
	if ( UTPSHUD* hudUI = _GetHUDUI() )
	{
		hudUI->RefreshWeaponInfo( CurrentWeaponType, CurrentBullet );
	}
}

// 무기 복제 도착 시 HUD를 동기화한다.
void ATPSCharacter::OnRep_CurrentWeapon()
{
	if ( IsValid( CurrentWeapon ) )
	{
		if ( IsTPSMode || !IsZoomMode ) _ToggleHUDUI( true );
		_RefreshWeaponHUD();
	}
	else
	{
		_ToggleHUDUI( false );
	}
}

// 탄약 복제 도착 시 HUD를 갱신한다.
void ATPSCharacter::OnRep_CurrentBullet()
{
	_RefreshWeaponHUD();
}

