// HUD 클래스 헤더 파일


#pragma once


#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "TPSHUD.generated.h"


class UTextBlock;


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
	
public:
	// 생성한다.
	static UTPSHUD* Create();
	
	// 파일 경로를 반환한다.
	static FString GetFilePath();
	
	// 크로스헤어 가시성을 토글한다.
	void ToggleCrosshair( const bool bOn ) const;
	
	// 갱신한다.
	void Refresh( const bool bAim, const EWeaponType WeaponType, const int32 LeftBullet );
};
