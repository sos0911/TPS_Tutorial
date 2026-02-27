// UI 매니저 헤더 파일


#pragma once


#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/Widget.h"
#include "GameInstance/TPSGameInstance.h"
#include "Util/TPSUtilEngine.h"
#include "TPSUIManager.generated.h"


UCLASS()
class TPS_TUTORIAL_API UTPSUIManager : public UObject
{
	GENERATED_BODY()
	
private:
	UPROPERTY()
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
		
		UClass* widgetClass = TPSUtilEngine::LoadUserWidgetClass( FilePath );
		if ( !widgetClass ) return nullptr;
	
		// T* widget = CreateWidget< T >( GetWorld(), widgetClass );
		T* widget = CreateWidget< T >( UTPSGameInstance::GetGameInstance(), widgetClass );
		if ( !widget ) return nullptr;
	
		// NOTE : blueprint class로 Uclass* 를 저장하면 안됨. 쿼리 시 StaticClass() 로 찾을 것이기 때문.
		WidgetMap.Add( widgetClass->GetSuperClass(), widget );
		widget->AddToViewport( Zorder );
	
		return widget;
	}
	
	// 데이터를 정리한다.
	void Clear();
	
	// 생성된 UI 중 조건에 맞는 UI를 찾아서 반환한다
	UUserWidget* FindWidget( UClass* WidgetClass ) const;
};

// UI 매니저를 반환한다.
inline UTPSUIManager* GetTPSUIManager()
{
	UTPSGameInstance* gameInstance = UTPSGameInstance::GetGameInstance();
	if ( !gameInstance ) return nullptr;
	
	return gameInstance->GetUIManager();
}
