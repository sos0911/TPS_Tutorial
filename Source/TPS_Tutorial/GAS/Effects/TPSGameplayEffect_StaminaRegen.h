// TPS Stamina 회복 GameplayEffect — Infinite + 주기적 회복.
// State.Sprinting 태그 보유 중(스프린트 중)에는 OngoingTagRequirements로 회복이 억제된다.


#pragma once


#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "TPSGameplayEffect_StaminaRegen.generated.h"


UCLASS()
class TPS_TUTORIAL_API UTPSGameplayEffect_StaminaRegen : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UTPSGameplayEffect_StaminaRegen();

	// GE 컴포넌트 생성(NewObject)은 생성자에서 불가하므로 여기서 구성한다.
	virtual void PostInitProperties() override;
};
