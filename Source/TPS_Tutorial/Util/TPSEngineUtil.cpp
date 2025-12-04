// 엔진단 유틸함수 모음 소스


#include "TPSEngineUtil.h"
#include "Blueprint/UserWidget.h"


// 유저위젯 클래스를 파일 경로로 로드한다.
UClass* TPSEngineUtil::LoadUserWidgetClass( FString& FilePath )
{
	UClass* userWidgetClass = LoadClass< UUserWidget >(nullptr, *FilePath);
	if ( !userWidgetClass ) return nullptr;
	
	return userWidgetClass;
}
