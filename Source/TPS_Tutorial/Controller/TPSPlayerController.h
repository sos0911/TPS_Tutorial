// 커스텀 테스트용 플레이어 컨트롤러 헤더


#pragma once


#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "TPSPlayerController.generated.h"


class ATPSPlayerCameraManager;


UCLASS()
class TPS_TUTORIAL_API ATPSPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	ATPSPlayerController();
	
	// 플레이 시작한다.
	virtual void BeginPlay() override;
};
