// TPS 캐릭터 클래스 소스 파일


#include "TPSCharacter.h"
#include "InputActionValue.h"
#include "KismetAnimationLibrary.h"
#include "Actors/TPSPickUpBase.h"
#include "Actors/TPSShotImpactField.h"
#include "Actors/Components/TPSDataComponent.h"
#include "Components/CapsuleComponent.h"
#include "Consts/TPSConsts.h"
#include "Engine/SkeletalMeshSocket.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameInstance/TPSGameInstance.h"
#include "Kismet/KismetMathLibrary.h"
#include "Log/TPSLog.h"
#include "Logic/ITPSInteractionActorInterface.h"
#include "Manager/TPSUIManager.h"
#include "UI/TPSHUD.h"
#include "Util/TPSUtil.h"


// Sets default values
ATPSCharacter::ATPSCharacter()
{
 	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
}

// Called when the game starts or when spawned
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
}

// 무기를 줍는 상호작용을 실행한다.
bool ATPSCharacter::HandlePickUpWeaponInteract( AActor* OtherActor )
{
	// 무기를 이미 장착 중이라면 추가로 주울 수 없다.
	if ( CurrentWeapon.IsValid() ) return false;

	ATPSPickUpBase* pickUpActor = Cast< ATPSPickUpBase >( OtherActor );
	if ( !pickUpActor ) return false;

	UTPSDataComponent* dataComponent = TPSUtil::GetValueForObjProp< UTPSDataComponent >( pickUpActor );
	if ( !dataComponent ) return false;

	const FWeaponData* weaponData = dataComponent->GetData< FWeaponData >();
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

	FActorSpawnParameters spawnParams;
	spawnParams.Owner                          = this;
	spawnParams.Instigator                     = GetInstigator();
	spawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	
	AActor* weapon = GetWorld()->SpawnActor< AActor >( weaponData->EquipWeapon, FTransform::Identity, spawnParams );
	if ( !weapon ) return false;

	FAttachmentTransformRules attachmentRules( EAttachmentRule::SnapToTarget, true );
	weapon->AttachToComponent( BodyComp, attachmentRules, socketName );

	CurrentWeapon = weapon;
	CurrentWeaponType = weaponData->WeaponType;
	
	if ( IsTPSMode || !IsZoomMode ) _ToggleHUDUI( true );

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
	UTPSUIManager* uiManager = UTPSGameInstance::GetGameInstance()->GetUIManager();
	if ( !uiManager ) return;
	
	UUserWidget* hudUI = uiManager->FindWidget( UTPSHUD::StaticClass() );
	if ( !hudUI ) return;
	
	hudUI->SetVisibility( bOn ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed );
}

// Called every frame
void ATPSCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

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

// Called to bind functionality to input
void ATPSCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

}

// 오버랩이 시작되었음을 알리는 이벤트를 처리한다.
void ATPSCharacter::OnBeginOverlap( UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult )
{
	ITPSPickUpInteractionActorInterface* interactionPickUpActorInterface = Cast< ITPSPickUpInteractionActorInterface >( OtherActor );
	if ( !interactionPickUpActorInterface ) return;

	if ( HandlePickUpWeaponInteract( OtherActor ) ) interactionPickUpActorInterface->HandlePickUpWeaponInteract( this );
}

// 이동한다.
void ATPSCharacter::Move( const FInputActionValue& Value )
{
	if ( Value.GetValueType() != EInputActionValueType::Axis2D ) return;

	UE_LOG( LogGameplay, Log, TEXT("move value : { %f %f }" ), Value.Get< FVector2D >().X, Value.Get< FVector2D >().Y );

	double xValue = Value.Get< FVector2D >().X;
	double yValue = Value.Get< FVector2D >().Y;

	FRotator rotator = GetControlRotation();
	
	UE_LOG( LogGameplay, Log, TEXT("rotate value : { %f %f }" ), rotator.Pitch, rotator.Yaw );
	
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
	if ( !CurrentWeapon.IsValid() ) return;

	UTPSDataComponent* dataComponent = TPSUtil::GetValueForObjProp< UTPSDataComponent >( CurrentWeapon.Get() );
	if ( !dataComponent ) return;

	const FWeaponData* weaponData = dataComponent->GetData< FWeaponData >();
	if ( !weaponData ) return;

	const FVector  dropLocation = GetActorLocation() + GetActorForwardVector() * 200.0f;
	const FRotator dropRotation = GetActorRotation();

	FActorSpawnParameters spawnParams;
	// spawnParams.Owner                          = nullptr;
	// spawnParams.Instigator                     = GetInstigator();
	spawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	
	AActor* weapon = GetWorld()->SpawnActor< AActor >( weaponData->PickUpWeapon, FTransform( dropRotation, dropLocation ), spawnParams );

	CurrentWeapon->Destroy();
	CurrentWeapon = nullptr;
	CurrentWeaponType = EWeaponType::Max;
	
	_ToggleHUDUI( false );
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
	if ( !IsTPSMode && !CurrentWeapon.IsValid() ) return;

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
			
			playerController->SetViewTargetWithBlend( childActorComp->GetChildActor(), cameraBlendTime, VTBlend_Linear, 0, true);
			CurrentCameraComp = childActorComp;
		}
		else
		{
			playerController->SetViewTargetWithBlend( FPSCameraComp->GetChildActor(), cameraBlendTime, VTBlend_Linear, 0, true );
			CurrentCameraComp = FPSCameraComp;
		}
	}
	
	if ( !CurrentWeapon.IsValid() )      _ToggleHUDUI( false );
	else if ( !IsTPSMode && IsZoomMode ) _ToggleHUDUI( false );
	else                                 _ToggleHUDUI( true  );
}

// 무기를 발사하는 상호작용을 실행한다.
bool ATPSCharacter::HandleFireWeaponInteract()
{
	if ( !CurrentWeapon.IsValid() || CurrentWeaponType == EWeaponType::Max ) return false;

	if ( ITPSEquipInteractionActorInterface* interactionEquipActorInterface = Cast< ITPSEquipInteractionActorInterface >( CurrentWeapon ) )
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
					FActorSpawnParameters spawnParams;
					spawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
				
					ATPSShotImpactField* fieldActor = GetWorld()->SpawnActor< ATPSShotImpactField >(
						LoadClass< ATPSShotImpactField >( nullptr, *ATPSShotImpactField::GetPath() ),
						FVector( hitResult.ImpactPoint ), FRotator(), spawnParams );
					
					FTimerHandle removeImpactFieldTimerHandle;
					GetWorldTimerManager().SetTimer( removeImpactFieldTimerHandle, [ fieldActor ] ()
					{
						if ( !fieldActor ) return;
						
						fieldActor->Destroy();
					}, 0.1f, false );
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

