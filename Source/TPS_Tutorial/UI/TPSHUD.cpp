// HUD 클래스 소스 파일


#include "UI/TPSHUD.h"
#include "GameInstance/TPSGameInstance.h"
#include "Manager/TPSUIManager.h"


// 생성한다.
UTPSHUD* UTPSHUD::Create()
{
	UTPSGameInstance* gameInstance = UTPSGameInstance::GetGameInstance();
	if ( !gameInstance ) return nullptr;
	
	UTPSUIManager* uiManager = gameInstance->GetUIManager();
	if ( !uiManager ) return nullptr;
	
	return uiManager->CreateAndAddViewport< UTPSHUD >( ( StaticClass() ) );
}
