// TPS 캐릭터 클래스 소스 파일


#include "TPSCharacter.h"
#include "TPS_Tutorial/TPSPlayerController.h"
#include "EnhancedInputSubsystems.h"


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

	// ATPSPlayerController* playerController = Cast< ATPSPlayerController >( GetController() );
	// if ( !playerController ) return;
	//
	// ULocalPlayer* localPlayer = playerController->GetLocalPlayer();
	// if ( !localPlayer ) return;
	//
	// UEnhancedInputLocalPlayerSubsystem* subsystem = ULocalPlayer::GetSubsystem< UEnhancedInputLocalPlayerSubsystem >( localPlayer );
	// if ( !subsystem ) return;
	//
	// subsystem->AddMappingContext(  )

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
			break;
		}
	}

	IsTPSMode  = true;
	IsZoomMode = false;
}

// Called every frame
void ATPSCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

// Called to bind functionality to input
void ATPSCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

}

// 이동한다.
void ATPSCharacter::Move( const FInputActionValue& Value )
{
	if ( Value.GetValueType() != EInputActionValueType::Axis2D ) return;
	
	AddMovementInput( GetActorRightVector(),   Value.Get< FVector2D >().X );
	AddMovementInput( GetActorForwardVector(), Value.Get< FVector2D >().Y );
}

// 시점을 이동한다.
void ATPSCharacter::LookAround( const FInputActionValue& Value )
{
	if ( Value.GetValueType() != EInputActionValueType::Axis2D ) return;

	AddControllerYawInput  ( Value.Get< FVector2D >().X );
	AddControllerPitchInput( Value.Get< FVector2D >().Y );
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
	playerController->SetViewTargetWithBlend( IsTPSMode ? TPSCameraComp->GetChildActor() : FPSCameraComp->GetChildActor(), cameraBlendTime );

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

	IsZoomMode = !IsZoomMode;

	float cameraBlendTime = 0.2f;
	playerController->SetViewTargetWithBlend( IsZoomMode ? TPSZoomCameraComp->GetChildActor() : TPSCameraComp->GetChildActor(), cameraBlendTime );
}

