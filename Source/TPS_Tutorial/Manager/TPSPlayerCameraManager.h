// FPS 모드에서 머리 Roll 값을 카메라 POV 에 합성하는 사용자 PlayerCameraManager 헤더


#pragma once


#include "CoreMinimal.h"
#include "Camera/PlayerCameraManager.h"
#include "TPSPlayerCameraManager.generated.h"


class ATPSCharacter;


UCLASS()
class TPS_TUTORIAL_API ATPSPlayerCameraManager : public APlayerCameraManager
{
	GENERATED_BODY()

protected:
	// 머리 소켓 이름 (우선)
	UPROPERTY(EditDefaultsOnly, Category = "TPS|Camera")
	FName HeadSocketNamePrimary = TEXT( "head" );

	// 머리 소켓 이름 (대체)
	UPROPERTY(EditDefaultsOnly, Category = "TPS|Camera")
	FName HeadSocketNameFallback = TEXT( "Head" );

public:
	// 기본 생성자
	ATPSPlayerCameraManager();

	// 뷰 타깃을 갱신한다.
	virtual void UpdateViewTarget( FTViewTarget& OutVT, float DeltaTime ) override;

private:
	// 현재 뷰 타깃이 폰의 FPS 카메라인지 확인한다.
	bool IsFirstPersonViewTargetFor( const APawn* Pawn ) const;

    // 폰의 Lean Roll 값을 얻는다. 성공 시 true 반환
	bool TryGetLeanRollFor( const APawn* Pawn, float& OutLeanRollDeg ) const;
};
