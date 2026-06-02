// TPS Sprint 어빌리티 헤더


#pragma once


#include "CoreMinimal.h"
#include "GAS/Abilities/TPSGameplayAbilityBase.h"
#include "TPSGameplayAbility_Sprint.generated.h"


class UGameplayEffect;


UCLASS()
class TPS_TUTORIAL_API UTPSGameplayAbility_Sprint : public UTPSGameplayAbilityBase
{
	GENERATED_BODY()

public:
	UTPSGameplayAbility_Sprint();

	// 스태미나가 최소치 이하이면 발동 불가 (고갈 직후 재발동 방지)
	virtual bool CanActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayTagContainer* SourceTags,
		const FGameplayTagContainer* TargetTags,
		FGameplayTagContainer* OptionalRelevantTags ) const override;

	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData ) override;

	virtual void EndAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility,
		bool bWasCancelled ) override;

protected:
	// Sprint 활성 동안 적용되는 Stamina 감소 GE (Duration, BP에서 지정)
	UPROPERTY( EditDefaultsOnly, Category = "GAS" )
	TSubclassOf< UGameplayEffect > StaminaDrainEffect;

	// 스프린트 이동 속도 (cm/s)
	UPROPERTY( EditDefaultsOnly, Category = "GAS" )
	float SprintSpeed = 1000.0f;

	// 스프린트 발동에 필요한 최소 스태미나
	UPROPERTY( EditDefaultsOnly, Category = "GAS" )
	float MinStaminaToSprint = 1.0f;

	// 평상시 이동 속도 — EndAbility에서 원복
	float CachedWalkSpeed = 600.0f;

	// 적용 중인 StaminaDrain GE 핸들 (EndAbility에서 즉시 Remove)
	FActiveGameplayEffectHandle ActiveStaminaDrainHandle;
};
