// HUD 클래스 소스 파일


#include "UI/TPSHUD.h"
#include "GameInstance/TPSGameInstance.h"
#include "Manager/TPSUIManager.h"
#include "Util/TPSUtilWidget.h"
#include <Components/TextBlock.h>


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
