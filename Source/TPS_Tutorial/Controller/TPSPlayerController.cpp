// 커스텀 테스트용 플레이어 컨트롤러 소스 파일


#include "TPSPlayerController.h"
#include "Manager/TPSPlayerCameraManager.h"
#include "UI/TPSHUD.h"


ATPSPlayerController::ATPSPlayerController()
{
	PlayerCameraManagerClass = ATPSPlayerCameraManager::StaticClass();
}

// 플레이 시작한다.
void ATPSPlayerController::BeginPlay()
{
	Super::BeginPlay();
	
	UTPSHUD::Create();
}

