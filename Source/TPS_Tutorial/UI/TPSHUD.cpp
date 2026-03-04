// HUD 클래스 소스 파일


#include "UI/TPSHUD.h"
#include "Character/TPSCharacter.h"
#include "GameInstance/TPSGameInstance.h"
#include "Manager/TPSDataManager.h"
#include "Manager/TPSUIManager.h"
#include "Util/TPSUtil.h"
#include "Util/TPSUtilWidget.h"
#include <Components/WidgetSwitcher.h>


// 생성한다.
UTPSHUD* UTPSHUD::Create()
{
	UTPSGameInstance* gameInstance = UTPSGameInstance::GetGameInstance();
	if ( !gameInstance ) return nullptr;
	
	UTPSUIManager* uiManager = gameInstance->GetUIManager();
	if ( !uiManager ) return nullptr;
	
	return uiManager->CreateAndAddViewport< UTPSHUD >( GetFilePath() );
}

// 파일 경로를 반환한다.
FString UTPSHUD::GetFilePath()
{
	return TEXT( "/Game/CustomContents/UI/WBP_TPSHUD.WBP_TPSHUD" );
}

// 크로스헤어 가시성을 토글한다.
void UTPSHUD::ToggleCrosshair( const bool bOn ) const
{
	TPSUtilWidget::SetVisibility( CrossHairPanel, bOn ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed );
}

// 갱신한다.
void UTPSHUD::Refresh( const bool bAim, const EWeaponType WeaponType, const int32 LeftBullet ) const
{
	if ( !CrossHairPanel      ) return;
	if ( !SwitcherWeaponState ) return;
	
	TPSUtilWidget::SetVisibility( CrossHairPanel, bAim ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed );
	SwitcherWeaponState->SetActiveWidgetIndex( WeaponType == EWeaponType::None ? static_cast< int32 >( EWeaponState::Empty ) : static_cast< int32 >( EWeaponState::Gun ) );

	// NOTE : Character actor 찾아서 datacomponent 참조해도 되나, 일단 EWeaponType 인자로 찾아본다.
	// if ( ATPSCharacter* character = Cast< ATPSCharacter >( UGameplayStatics::GetPlayerCharacter( GetWorld(), 0 ) ) )
	
	if ( UTPSDataManager* dataManager = GetTPSDataManager() )
	{
		const FStringTableData* stringData = dataManager->FindRow< FStringTableData >( TEXT( "DT_String" ), TEXT( "BULLET_DESCRIPTION" )     );
		const FWeaponTableData* weaponData = dataManager->FindRow< FWeaponTableData >( TEXT( "DT_Weapon" ), *TPSUtil::ToString( WeaponType ) );
		if ( stringData && weaponData )
		{
			FString resText = stringData->StringValue;
			resText = resText.Replace( TEXT( "[LeftValue]" ), *TPSUtil::ToString( LeftBullet               ) );
			resText = resText.Replace( TEXT( "[AllValue]"  ), *TPSUtil::ToString( weaponData->MagazineSize ) );
			
			TPSUtilWidget::SetText( TextBullet, resText );
		}
	}
}
