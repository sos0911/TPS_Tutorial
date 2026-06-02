// TPS Damage GameplayEffect — Instant 타입.
// 호출 컨벤션: SetByCaller(TAG_Data_Damage)에는 **음수 값**을 전달한다 (예: -50 → Health 50 차감).
// 호출자가 양수 데미지로 사용하기 편하게 ApplyDamage() 정적 헬퍼를 사용하라.


#pragma once


#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "GameplayEffectTypes.h"
#include "TPSGameplayEffect_Damage.generated.h"


class UAbilitySystemComponent;


UCLASS()
class TPS_TUTORIAL_API UTPSGameplayEffect_Damage : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UTPSGameplayEffect_Damage();

	// 양수 데미지 값을 받아 음수로 변환해 Self에게 적용하는 정적 헬퍼.
	// SetByCaller 음수 컨벤션의 호출 측 부담을 제거한다.
	static FActiveGameplayEffectHandle ApplyDamage(
		UAbilitySystemComponent* Target,
		UAbilitySystemComponent* Source,
		float                    DamageAmount /* 양수 */ );
};
