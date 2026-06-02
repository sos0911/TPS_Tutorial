// HUD 클래스 헤더 파일


#pragma once


#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Consts/TPSConsts.h"
#include "TPSHUD.generated.h"


class UProgressBar;
class UTextBlock;
class UWidgetSwitcher;


UCLASS()
class TPS_TUTORIAL_API UTPSHUD : public UUserWidget
{
	GENERATED_BODY()

public:
	enum class EWeaponState
	{
		Empty = 0, // 빈 손
		Gun,       // 총 장착
		Max
	};

private:
	UPROPERTY( meta = ( BindWidget ) )
	UTextBlock* TextBullet; // 총알 안내 텍스트

	UPROPERTY( meta = ( BindWidget ) )
	UWidget* CrossHairPanel; // 크로스헤어 패널 위젯

	UPROPERTY( meta = ( BindWidget ) )
	UWidgetSwitcher* SwitcherWeaponState; // 무기 상태 스위쳐 위젯
	
	UPROPERTY( meta = ( BindWidget ) )
	UProgressBar* ProgressBarStamina; // 스태미너 프로그래스 바

public:
	// 생성한다.
	static UTPSHUD* Create();

	// 파일 경로를 반환한다.
	static FString GetFilePath();
	
	// 초기화한다.
	void Init();

	// 크로스헤어 가시성을 토글한다.
	void ToggleCrosshair( const bool bOn ) const;

	// 스태미너 프로그래스 바를 갱신한다. (Percent: 0.0 ~ 1.0)
	void SetStaminaPercent( const float Percent ) const;

	// 무기 정보(무기 상태 스위쳐 + 장탄수 텍스트)를 갱신한다. (크로스헤어는 건드리지 않음)
	void RefreshWeaponInfo( const EWeaponType WeaponType, const int32 LeftBullet ) const;

	// 갱신한다.
	void Refresh( const bool bAim, const EWeaponType WeaponType, const int32 LeftBullet ) const;
};
