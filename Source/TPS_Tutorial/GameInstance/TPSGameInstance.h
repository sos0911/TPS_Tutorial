// 커스텀 게임 인스터스 헤더 파일


#pragma once


#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "TPSGameInstance.generated.h"


class UTPSUIManager;


UCLASS()
class TPS_TUTORIAL_API UTPSGameInstance : public UGameInstance
{
	GENERATED_BODY()
	
public:
	UPROPERTY()
	TWeakObjectPtr< UTPSUIManager > UIManager;
	
public:
	// 싱글턴 객체를 얻는다.
	static UTPSGameInstance* GetGameInstance( UWorld* InWorld = nullptr );
	
	// UI 관리자 객체를 얻는다.
	UTPSUIManager* GetUIManager() const { return UIManager.Get(); }
	
protected:
	// 초기화한다.
	virtual void Init() override;
};
