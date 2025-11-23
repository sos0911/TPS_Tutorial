// 커스텀 테스트용 플레이어 컨트롤러 소스 파일


#include "TPSPlayerController.h"
#include "GameInstance/TPSGameInstance.h"
#include "Manager/TPSPlayerCameraManager.h"
#include "Manager/TPSUIManager.h"
#include "UI/TPSHUD.h"


ATPSPlayerController::ATPSPlayerController()
{
	PlayerCameraManagerClass = ATPSPlayerCameraManager::StaticClass();
}

// 플레이 시작한다.
void ATPSPlayerController::BeginPlay()
{
	Super::BeginPlay();
	
	UTPSGameInstance* gameInstance = UTPSGameInstance::GetGameInstance( GetWorld() );
	if ( !gameInstance ) return;
	
	UTPSUIManager* uiManager = gameInstance->GetUIManager();
	if ( !uiManager ) return;
	
	uiManager->CreateAndAddViewport< UTPSHUD >( UTPSHUD::StaticClass() );
	
	// UTPSHUD::Create();
}

