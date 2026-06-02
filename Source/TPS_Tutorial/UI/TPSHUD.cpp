// HUD 클래스 소스 파일


#include "UI/TPSHUD.h"
#include "Character/TPSCharacter.h"
#include "GameInstance/TPSGameInstance.h"
#include "Manager/TPSDataManager.h"
#include "Manager/TPSUIManager.h"
#include "Util/TPSUtil.h"
#include "Util/TPSUtilWidget.h"
#include <Components/ProgressBar.h>
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

// 초기화한다.
void UTPSHUD::Init()
{
	SetStaminaPercent( 1.0f );
	Refresh( false, EWeaponType::None, 0 );
}

// 크로스헤어 가시성을 토글한다.
void UTPSHUD::ToggleCrosshair( const bool bOn ) const
{
	TPSUtilWidget::SetVisibility( CrossHairPanel, bOn ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed );
}

// 스태미너 프로그래스 바를 갱신한다.
void UTPSHUD::SetStaminaPercent( const float Percent ) const
{
	if ( !ProgressBarStamina ) return;

	ProgressBarStamina->SetPercent( FMath::Clamp( Percent, 0.0f, 1.0f ) );
}

// 무기 상태 스위쳐와 장탄수 텍스트를 갱신한다. (크로스헤어 비관여)
void UTPSHUD::RefreshWeaponInfo( const EWeaponType WeaponType, const int32 LeftBullet ) const
{
	if ( !SwitcherWeaponState ) return;

	SwitcherWeaponState->SetActiveWidgetIndex( WeaponType == EWeaponType::None ? static_cast< int32 >( EWeaponState::Empty ) : static_cast< int32 >( EWeaponState::Gun ) );

	// NOTE : Character actor 찾아서 datacomponent 참조해도 되나, 일단 EWeaponType 인자로 찾아본다.
	// if ( ATPSCharacter* character = Cast< ATPSCharacter >( UGameplayStatics::GetPlayerCharacter( GetWorld(), 0 ) ) )

	if ( UTPSDataManager* dataManager = GetTPSDataManager() )
	{
		const FStringTableData* stringData = dataManager->FindRow< FStringTableData >( TEXT( "DT_String" ), TEXT( "BULLET_DESCRIPTION" )     );
		const FWeaponTableData* weaponData = dataManager->FindRow< FWeaponTableData >( TEXT( "DT_Weapon"  ), *TPSUtil::ToString( WeaponType ) );
		if ( stringData )
		{
			FString resText = stringData->StringValue;
			if ( weaponData )
			{
				resText = resText.Replace( TEXT( "[LeftValue]" ), *TPSUtil::ToString( LeftBullet               ) );
				resText = resText.Replace( TEXT( "[AllValue]"  ), *TPSUtil::ToString( weaponData->MagazineSize ) );	
			}
			else
			{
				resText = resText.Replace( TEXT( "[LeftValue]" ), *TPSUtil::ToString( 0 ) );
				resText = resText.Replace( TEXT( "[AllValue]"  ), *TPSUtil::ToString( 0 ) );
			}

			TPSUtilWidget::SetText( TextBullet, resText );
		}
	}
}

// 갱신한다.
void UTPSHUD::Refresh( const bool bAim, const EWeaponType WeaponType, const int32 LeftBullet ) const
{
	if ( !CrossHairPanel ) return;

	TPSUtilWidget::SetVisibility( CrossHairPanel, bAim ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed );

	RefreshWeaponInfo( WeaponType, LeftBullet );
}
