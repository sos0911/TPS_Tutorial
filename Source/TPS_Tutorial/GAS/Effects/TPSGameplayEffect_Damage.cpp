// TPS Damage GameplayEffect 구현


#include "GAS/Effects/TPSGameplayEffect_Damage.h"
#include "AbilitySystemComponent.h"
#include "GAS/TPSAttributeSet.h"
#include "GAS/TPSGameplayTags.h"


// Instant 데미지 GE의 Health SetByCaller 모디파이어를 구성한다.
UTPSGameplayEffect_Damage::UTPSGameplayEffect_Damage()
{
	// Instant: 즉시 BaseValue를 변경
	DurationPolicy = EGameplayEffectDurationType::Instant;

	// Health += SetByCaller(TAG_Data_Damage)
	// 컨벤션: 호출자는 음수 값을 전달한다 (예: -50). 양수 전달 시 회복으로 동작하므로 주의.
	// 권장: ApplyDamage() 정적 헬퍼를 사용해 양수 데미지를 음수로 자동 변환.
	FGameplayModifierInfo modifier;
	modifier.Attribute  = UTPSAttributeSet::GetHealthAttribute();
	modifier.ModifierOp = EGameplayModOp::Additive;

	FSetByCallerFloat setByCaller;
	setByCaller.DataTag = TAG_Data_Damage;

	modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude( setByCaller );

	Modifiers.Add( modifier );
}

// 양수 데미지를 음수로 변환해 대상에게 데미지 GE를 적용한다.
FActiveGameplayEffectHandle UTPSGameplayEffect_Damage::ApplyDamage(
	UAbilitySystemComponent* Target,
	UAbilitySystemComponent* Source,
	float                    DamageAmount )
{
	if ( !Target || DamageAmount <= 0.0f )
	{
		return FActiveGameplayEffectHandle();
	}

	UAbilitySystemComponent* effectiveSource = Source ? Source : Target;

	FGameplayEffectContextHandle ctx = effectiveSource->MakeEffectContext();
	const FGameplayEffectSpecHandle specHandle = effectiveSource->MakeOutgoingSpec(
		UTPSGameplayEffect_Damage::StaticClass(), 1.0f, ctx );

	if ( !specHandle.IsValid() )
	{
		return FActiveGameplayEffectHandle();
	}

	// 양수 → 음수로 변환하여 Health 차감되도록 SetByCaller 등록.
	specHandle.Data->SetSetByCallerMagnitude( TAG_Data_Damage, -DamageAmount );

	return effectiveSource->ApplyGameplayEffectSpecToTarget( *specHandle.Data.Get(), Target );
}
