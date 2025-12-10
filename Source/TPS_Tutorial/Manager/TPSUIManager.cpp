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

// 데이터를 정리한다.
void UTPSUIManager::Clear()
{
	WidgetMap.Empty();
}

// 생성된 UI 중 조건에 맞는 UI를 찾아서 반환한다.
UUserWidget* UTPSUIManager::FindWidget( UClass* WidgetClass ) const
{
	if ( !WidgetClass ) return nullptr;
	
	UUserWidget* const* widgetPtr = WidgetMap.Find( WidgetClass );
	return widgetPtr ? *widgetPtr : nullptr;
}
