// 커스텀 게임 인스터스 소스 파일


#include "GameInstance/TPSGameInstance.h"
#include "Manager/TPSUIManager.h"


// 싱글턴 객체를 얻는다.
UTPSGameInstance* UTPSGameInstance::GetGameInstance()
{
	if ( !GEngine ) return nullptr;
	
#if WITH_EDITOR
	for ( const FWorldContext& context : GEngine->GetWorldContexts() )
	{
		if ( context.WorldType == EWorldType::Game || context.WorldType == EWorldType::PIE )
		{
			UWorld* world = context.World();
			if ( !world ) continue;
			
			return world->GetGameInstance< UTPSGameInstance >();
		}
	}
#else
	if ( UWorld* world = GEngine->GetWorld() )
	{
		return world->GetGameInstance< UTPSGameInstance >();
	}
#endif
	
	return nullptr;
}

// 초기화한다.
void UTPSGameInstance::Init()
{
	Super::Init();
	
	UIManager = UTPSUIManager::Create( this );
}
