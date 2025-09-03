// TPS 캐릭터 클래스 소스 파일


#include "TPSCharacter.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/KismetMathLibrary.h"
#include "Log/TPSLog.h"
#include "InputActionValue.h"
#include "Actors/TPSPickUpBase.h"
#include "Actors/Components/TPSDataComponent.h"
#include "Components/CapsuleComponent.h"
#include "Consts/TPSConsts.h"
#include "Logic/ITPSInteractionActorInterface.h"
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

	if ( UCapsuleComponent* capsuleComp = GetCapsuleComponent() )
	{
		capsuleComp->OnComponentBeginOverlap.AddDynamic( this, &ATPSCharacter::OnBeginOverlap );
	}
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

	return true;
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

	FRotator rotator = GetControlRotation();
	// TODO : 아래 계산 공식에서 Roll 이 RightVector 뽑는 데 필요한가? wasd 모두 yaw 만 필요하지 않나 싶은데..
	AddMovementInput( UKismetMathLibrary::GetRightVector( FRotator( rotator.Pitch, rotator.Yaw, 0.0f ) ), Value.Get< FVector2D >().X );
	AddMovementInput( UKismetMathLibrary::GetForwardVector( FRotator( 0.0f, rotator.Yaw, 0.0f ) ), Value.Get< FVector2D >().Y );
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

	const float befPitch = Pitch;
	const float afPitch  = FMath::Clamp( Pitch + Value.Get< FVector2D >().Y, -25.0f, 25.0f );

	AddControllerYawInput( Value.Get< FVector2D >().X );
	AddControllerPitchInput( afPitch - befPitch );

	UE_LOG( LogGameplay, Log, TEXT( "mouse input Y : [ %f ], afPitch : [ %f ], befPitch : [ %f ]" ), Value.Get< FVector2D >().Y, afPitch, befPitch );

	Pitch = afPitch;
}

// 상체를 기울인다.
void ATPSCharacter::Lean(const FInputActionValue& Value)
{
	if ( Value.GetValueType() != EInputActionValueType::Axis1D ) return;
	if ( IsJumping ) return;

	const float axisValue = Value.Get< float >();
	IsLeaning = FMath::Abs( axisValue ) > KINDA_SMALL_NUMBER;
	
	TargetRollValue = FMath::GetMappedRangeValueClamped( FVector2f( -1.0f, 1.0f ), FVector2f( -10.0f, 10.0f ), axisValue );
}

// 점프한다.
void ATPSCharacter::DoJump(const FInputActionValue& Value)
{
	if ( Value.GetValueType() != EInputActionValueType::Boolean ) return;
	if ( !GetCharacterMovement() ) return;
	// 공중에 있는 동안은 점프할 수 없다.
	// if ( GetCharacterMovement()->IsFalling() ) return;
	if ( IsJumping ) return;

	const float value = Value.Get< bool >();
	Jump();

	IsJumping = true;
}

// 무기를 드랍한다.
void ATPSCharacter::Drop(const FInputActionValue& Value)
{
	if ( Value.GetValueType() != EInputActionValueType::Boolean ) return;

	
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
	if ( !IsTPSMode ) return;

	APlayerController* playerController = Cast< APlayerController >( GetController() );
	if ( !playerController ) return;

	IsZoomMode = !IsZoomMode;

	float cameraBlendTime = 0.2f;
	playerController->SetViewTargetWithBlend( IsZoomMode ? TPSZoomCameraComp->GetChildActor() : TPSCameraComp->GetChildActor(), cameraBlendTime );
}

