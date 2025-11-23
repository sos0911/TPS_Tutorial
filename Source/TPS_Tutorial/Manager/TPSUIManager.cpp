// UI 매니저 소스 파일


#include "Manager/TPSUIManager.h"
#include "Blueprint/UserWidget.h"


// 생성자
UTPSUIManager::UTPSUIManager()
{
}

// 소멸자
UTPSUIManager::~UTPSUIManager()
{
}

// 인스턴스를 생성하여 반환한다.
UTPSUIManager* UTPSUIManager::Create( UObject* Owner )
{
	return NewObject< UTPSUIManager >( Owner, UTPSUIManager::StaticClass() );
}
