// FPS 모드에서 머리 Roll 값을 카메라 POV 에 합성하는 사용자 PlayerCameraManager 소스


#include "TPSPlayerCameraManager.h"
#include "TPSPlayerController.h"
#include "Components/ChildActorComponent.h"
#include "GameFramework/Character.h"
#include "Components/SkeletalMeshComponent.h"
#include "Character/TPSCharacter.h"


// 기본값 설정
ATPSPlayerCameraManager::ATPSPlayerCameraManager()
{
	// 현재는 별도 설정 없음
}

// 뷰 타깃을 갱신한다.
void ATPSPlayerCameraManager::UpdateViewTarget( FTViewTarget& OutVT, float DeltaTime )
{
	// 우선 부모에서 컨트롤러/뷰 타깃 기반의 기본 POV 를 계산한다.
	Super::UpdateViewTarget( OutVT, DeltaTime );

	const APlayerController* PC = PCOwner;
	const APawn* Pawn = PC ? PC->GetPawn() : nullptr;
	if ( !Pawn )
	{
		return;
	}

	// TPS 등 다른 모드에 영향을 주지 않도록 FPS 카메라 뷰 타깃에서만 적용한다.
	if ( !IsFirstPersonViewTargetFor( Pawn ) )
	{
		return;
	}

	float LeanRollDeg = 0.0f;
	if ( TryGetLeanRollFor( Pawn, LeanRollDeg ) )
	{
		// 기본 계산된 Yaw/Pitch 는 유지하고, Roll 만 Lean 값으로 대체한다.
		FRotator R = OutVT.POV.Rotation;
		R.Roll = LeanRollDeg;
		OutVT.POV.Rotation = R;
	}
}

// 현재 뷰 타깃이 폰의 FPS 카메라인지 확인한다.
bool ATPSPlayerCameraManager::IsFirstPersonViewTargetFor( const APawn* Pawn ) const
{
	if ( !Pawn )
	{
		return false;
	}

	const AActor* CurrentVT = PCOwner ? PCOwner->GetViewTarget() : nullptr;
	if ( !CurrentVT )
	{
		return false;
	}

	// 프로젝트의 FPS 카메라 이름과 일치하는 ChildActorComponent 를 찾아 비교한다.
	TArray< const UChildActorComponent* > ChildComps;
	Pawn->GetComponents( ChildComps );
	for ( const UChildActorComponent* CAC : ChildComps )
	{
		if ( !CAC ) continue;
		const FString Name = CAC->GetName();
		if ( Name.Equals( TEXT( "FPSCamera" ) ) )
		{
			if ( const AActor* ChildActor = CAC->GetChildActor() )
			{
				return ChildActor == CurrentVT;
			}
		}
	}
	return false;
}

// 폰의 Lean Roll 값을 얻는다. 성공 시 true 반환
bool ATPSPlayerCameraManager::TryGetLeanRollFor( const APawn* Pawn, float& OutLeanRollDeg ) const
{
	OutLeanRollDeg = 0.0f;

	const ACharacter* Char = Cast< ACharacter >( Pawn );
	if ( !Char )
	{
		return false;
	}

	const ATPSCharacter* TPSChar = Cast< ATPSCharacter >( Char );
	if ( !TPSChar )
	{
		return false;
	}

	OutLeanRollDeg = TPSChar->GetLeanRoll();
	return true;
}
