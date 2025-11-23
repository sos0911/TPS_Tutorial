// UI 매니저 헤더 파일


#pragma once


#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/Widget.h"
#include "TPSUIManager.generated.h"


UCLASS()
class TPS_TUTORIAL_API UTPSUIManager : public UObject
{
	GENERATED_BODY()
	
private:
	TMap< UClass*, UUserWidget* > WidgetMap; // UI 맵 
	
public:
	// 생성자
	UTPSUIManager();
	
	// 소멸자
	~UTPSUIManager();
	
	// 인스턴스를 생성하여 반환한다.
	static UTPSUIManager* Create( UObject* Owner );
	
	// 위젯을 생성 및 추가한다.
	template< typename T >
	T* CreateAndAddViewport( UClass* WidgetClass, const int32 Zorder = 0 )
	{
		if ( !WidgetClass ) return nullptr;
	
		UUserWidget* widget = CreateWidget< UUserWidget >( GetWorld(), WidgetClass );
		if ( !widget ) return nullptr;
	
		WidgetMap.Add( WidgetClass, widget );
		widget->AddToViewport( Zorder );
	
		return Cast< T >( widget );
	}
};
