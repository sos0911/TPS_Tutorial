// TPS Fire 어빌리티 헤더 — 골격(스텁). 향후 HandleFireWeaponInteract 마이그레이션 진입점.


#pragma once


#include "CoreMinimal.h"
#include "GAS/Abilities/TPSGameplayAbilityBase.h"
#include "TPSGameplayAbility_Fire.generated.h"


UCLASS()
class TPS_TUTORIAL_API UTPSGameplayAbility_Fire : public UTPSGameplayAbilityBase
{
	GENERATED_BODY()

public:
	UTPSGameplayAbility_Fire();

	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData ) override;
};
