// HUD 클래스 헤더 파일


#pragma once


#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "TPSHUD.generated.h"


UCLASS()
class TPS_TUTORIAL_API UTPSHUD : public UUserWidget
{
	GENERATED_BODY()
	
public:
	// 생성한다.
	static UTPSHUD* Create();
	
	// 파일 경로를 반환한다.
	static FString GetFilePath();
};
