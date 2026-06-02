// TPS Stamina 회복 GameplayEffect 구현


#include "GAS/Effects/TPSGameplayEffect_StaminaRegen.h"
#include "GameplayEffectComponents/TargetTagRequirementsGameplayEffectComponent.h"
#include "GAS/TPSAttributeSet.h"
#include "GAS/TPSGameplayTags.h"


// 무한 지속 + 주기적 Stamina 회복 모디파이어를 구성한다.
UTPSGameplayEffect_StaminaRegen::UTPSGameplayEffect_StaminaRegen()
{
	// 무한 지속 (캐릭터 스폰 시 1회 적용해 상시 유지)
	DurationPolicy = EGameplayEffectDurationType::Infinite;

	// 0.1초마다 주기적으로 적용 (= 초당 회복량 = 모디파이어 값 * 10)
	Period.Value = 0.1f;

	// 적용 즉시 1회 실행하지 않고 첫 주기 이후부터 회복 시작
	bExecutePeriodicEffectOnApplication = false;

	// Stamina += 2.0 (0.1초 주기 → 초당 +20 회복). 값은 GE BP에서 오버라이드 가능.
	FGameplayModifierInfo modifier;
	modifier.Attribute         = UTPSAttributeSet::GetStaminaAttribute();
	modifier.ModifierOp        = EGameplayModOp::Additive;
	modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude( FScalableFloat( 2.0f ) );

	Modifiers.Add( modifier );
}

// GE 컴포넌트는 NewObject로 생성되므로 생성자에서 만들 수 없다(AssertIfInConstructor).
// 생성자 완료 후 호출되는 PostInitProperties에서 구성한다.
void UTPSGameplayEffect_StaminaRegen::PostInitProperties()
{
	Super::PostInitProperties();

	// 스프린트 중(State.Sprinting 보유)에는 주기적 회복을 억제(inhibit)한다.
	// UE5.3 신형 API: TargetTagRequirements 컴포넌트의 OngoingTagRequirements 사용.
	// IgnoreTags: 해당 태그를 보유하면 Ongoing 요건 불충족 → GE 비활성(회복 멈춤). GE는 제거되지 않고 유지됨.
	// FindOrAdd라 중복 호출돼도 컴포넌트는 하나만 유지된다.
	UTargetTagRequirementsGameplayEffectComponent& tagReqComponent = FindOrAddComponent< UTargetTagRequirementsGameplayEffectComponent >();
	tagReqComponent.OngoingTagRequirements.IgnoreTags.AddTag( TAG_State_Sprinting );
}
