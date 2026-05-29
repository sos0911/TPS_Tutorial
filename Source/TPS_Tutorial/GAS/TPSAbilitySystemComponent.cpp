// TPS AbilitySystemComponent 구현


#include "GAS/TPSAbilitySystemComponent.h"
#include "AbilitySystemBlueprintLibrary.h"


// ASC 기본값(리플리케이션 모드 등)을 초기화한다.
UTPSAbilitySystemComponent::UTPSAbilitySystemComponent()
{
	// 단일 플레이어 전제 — Minimal 모드로 시작 (멀티 진입 시 Mixed로 변경 권장)
	ReplicationMode = EGameplayEffectReplicationMode::Minimal;
}

// 어빌리티 태그로 어빌리티 활성화를 시도한다.
bool UTPSAbilitySystemComponent::TryActivateAbilityByTag( const FGameplayTag& AbilityTag )
{
	if ( !AbilityTag.IsValid() ) return false;

	FGameplayTagContainer container;
	container.AddTag( AbilityTag );

	return TryActivateAbilitiesByTag( container );
}

// 어빌리티 태그에 해당하는 활성 어빌리티를 취소한다.
void UTPSAbilitySystemComponent::CancelAbilityByTag( const FGameplayTag& AbilityTag )
{
	if ( !AbilityTag.IsValid() ) return;

	FGameplayTagContainer container;
	container.AddTag( AbilityTag );

	CancelAbilities( &container );
}
