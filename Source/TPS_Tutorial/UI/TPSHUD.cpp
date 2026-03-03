// HUD 클래스 소스 파일


#include "UI/TPSHUD.h"
#include "Character/TPSCharacter.h"
#include "Consts/TPSConsts.h"
#include "GameInstance/TPSGameInstance.h"
#include "Kismet/GameplayStatics.h"
#include "Manager/TPSUIManager.h"
#include "Util/TPSUtilWidget.h"
#include <Components/TextBlock.h>
#include <Components/WidgetSwitcher.h>


// 생성한다.
UTPSHUD* UTPSHUD::Create()
{
	UTPSGameInstance* gameInstance = UTPSGameInstance::GetGameInstance();
	if ( !gameInstance ) return nullptr;
	
	UTPSUIManager* uiManager = gameInstance->GetUIManager();
	if ( !uiManager ) return nullptr;
	
	return uiManager->CreateAndAddViewport< UTPSHUD >( GetFilePath() );
}

// 파일 경로를 반환한다.
FString UTPSHUD::GetFilePath()
{
	return TEXT( "/Game/CustomContents/UI/WBP_TPSHUD.WBP_TPSHUD" );
}

// 크로스헤어 가시성을 토글한다.
void UTPSHUD::ToggleCrosshair( const bool bOn ) const
{
	TPSUtilWidget::SetVisibility( CrossHairPanel, bOn ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed );
}

// 갱신한다.
void UTPSHUD::Refresh( const bool bAim, const EWeaponType WeaponType, const int32 LeftBullet )
{
	if ( !CrossHairPanel      ) return;
	if ( !SwitcherWeaponState ) return;
	
	TPSUtilWidget::SetVisibility( CrossHairPanel, bAim ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed );
	SwitcherWeaponState->SetActiveWidgetIndex( WeaponType == EWeaponType::None ? static_cast< int32 >( EWeaponState::Empty ) : static_cast< int32 >( EWeaponState::Gun ) );
	
	ATPSCharacter* character = Cast< ATPSCharacter >( UGameplayStatics::GetPlayerCharacter( GetWorld(), 0 ) );
	if ( character )
	{
		// TODO : DataManager에서 DataTable 긁어와서 정해진 포맷대로 정보 텍스트 띄우기
	}
}
