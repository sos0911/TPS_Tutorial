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

