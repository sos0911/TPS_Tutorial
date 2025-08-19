// TPS 캐릭터 클래스 소스 파일


#include "TPSCharacter.h"
#include "EnhancedInputSubsystems.h"
#include "Kismet/KismetMathLibrary.h"


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

	if ( IsLeaning )
	{
		if ( !FMath::IsNearlyEqual( Roll, TargetRollValue ) ) Roll = FMath::FInterpTo( Roll, TargetRollValue, DeltaTime, 5.0f );
	}
	else 
	{
		if ( !FMath::IsNearlyEqual( Roll, 0.0f ) ) Roll = FMath::FInterpTo( Roll, 0.0f, DeltaTime, 5.0f );
	}
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

	FRotator rotator = GetControlRotation();
	// TODO : 아래 계산 공식에서 Roll 이 RightVector 뽑는 데 필요한가? wasd 모두 yaw 만 필요하지 않나 싶은데..
	AddMovementInput( UKismetMathLibrary::GetRightVector( FRotator( rotator.Pitch, rotator.Yaw, 0.0f ) ), Value.Get< FVector2D >().X );
	AddMovementInput( UKismetMathLibrary::GetForwardVector( FRotator( 0.0f, rotator.Yaw, 0.0f ) ), Value.Get< FVector2D >().Y );
}

// 시점을 이동한다.
void ATPSCharacter::LookAround( const FInputActionValue& Value )
{
	if ( Value.GetValueType() != EInputActionValueType::Axis2D ) return;

	Pitch = FMath::Clamp( Pitch + Value.Get< FVector2D >().Y, -30.0f, 30.0f );

	AddControllerYawInput( Value.Get< FVector2D >().X );
	AddControllerPitchInput( Value.Get< FVector2D >().Y );
}

// 상체를 기울인다.
void ATPSCharacter::Lean(const FInputActionValue& Value)
{
	if ( Value.GetValueType() != EInputActionValueType::Axis1D ) return;

	const float axisValue = Value.Get< float >();
	IsLeaning = FMath::Abs( axisValue ) > KINDA_SMALL_NUMBER;
	
	TargetRollValue = FMath::GetMappedRangeValueClamped( FVector2f( -1.0f, 1.0f ), FVector2f( -10.0f, 10.0f ), axisValue );
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

