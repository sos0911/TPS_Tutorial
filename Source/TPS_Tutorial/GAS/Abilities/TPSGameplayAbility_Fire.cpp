// TPS Fire 어빌리티 스텁 — 기존 HandleFireWeaponInteract 경로를 어빌리티로 래핑할 자리


#include "GAS/Abilities/TPSGameplayAbility_Fire.h"
#include "GAS/TPSGameplayTags.h"
#include "Log/TPSLog.h"


// Fire 어빌리티 태그와 활성화 차단 태그를 설정한다.
UTPSGameplayAbility_Fire::UTPSGameplayAbility_Fire()
{
	AbilityTags.AddTag( TAG_Ability_Fire );

	ActivationBlockedTags.AddTag( TAG_State_Dead );
}

// 어빌리티를 커밋하고 발사 처리를 수행한다 (현재는 스텁).
void UTPSGameplayAbility_Fire::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData )
{
	if ( !CommitAbility( Handle, ActorInfo, ActivationInfo ) )
	{
		EndAbility( Handle, ActorInfo, ActivationInfo, true, true );
		return;
	}

	// TODO: 추후 PR에서 ATPSCharacter::HandleFireWeaponInteract 본체를 이 어빌리티로 이동.
	UE_LOG( LogGameplay, Log, TEXT( "[TPS] GAS Fire Ability triggered (stub)" ) );

	EndAbility( Handle, ActorInfo, ActivationInfo, true, false );
}
