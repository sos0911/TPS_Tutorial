// UI 매니저 헤더 파일


#pragma once


#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/Widget.h"
#include "GameInstance/TPSGameInstance.h"
#include "Util/TPSEngineUtil.h"
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
	T* CreateAndAddViewport( FString FilePath, const int32 Zorder = 0 )
	{
		if ( FilePath.IsEmpty() ) return nullptr;
		if ( !FilePath.Contains( TEXT( "_C" ) ) ) FilePath.Append( TEXT( "_C" ) );
		
		UClass* widgetClass = TPSEngineUtil::LoadUserWidgetClass( FilePath );
		if ( !widgetClass ) return nullptr;
	
		// T* widget = CreateWidget< T >( GetWorld(), widgetClass );
		T* widget = CreateWidget< T >( UTPSGameInstance::GetGameInstance(), widgetClass );
		if ( !widget ) return nullptr;
	
		WidgetMap.Add( widgetClass, widget );
		widget->AddToViewport( Zorder );
	
		return widget;
	}
};

// UI 매니저를 반환한다.
inline UTPSUIManager* GetTPSUIManager()
{
	UTPSGameInstance* gameInstance = UTPSGameInstance::GetGameInstance();
	if ( !gameInstance ) return nullptr;
	
	return gameInstance->GetUIManager();
}
