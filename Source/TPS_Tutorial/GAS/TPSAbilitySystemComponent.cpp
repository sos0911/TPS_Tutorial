// TPS AbilitySystemComponent 구현


#include "GAS/TPSAbilitySystemComponent.h"
#include "AbilitySystemBlueprintLibrary.h"


UTPSAbilitySystemComponent::UTPSAbilitySystemComponent()
{
	// 단일 플레이어 전제 — Minimal 모드로 시작 (멀티 진입 시 Mixed로 변경 권장)
	ReplicationMode = EGameplayEffectReplicationMode::Minimal;
}

bool UTPSAbilitySystemComponent::TryActivateAbilityByTag( const FGameplayTag& AbilityTag )
{
	if ( !AbilityTag.IsValid() ) return false;

	FGameplayTagContainer container;
	container.AddTag( AbilityTag );

	return TryActivateAbilitiesByTag( container );
}

void UTPSAbilitySystemComponent::CancelAbilityByTag( const FGameplayTag& AbilityTag )
{
	if ( !AbilityTag.IsValid() ) return;

	FGameplayTagContainer container;
	container.AddTag( AbilityTag );

	CancelAbilities( &container );
}
