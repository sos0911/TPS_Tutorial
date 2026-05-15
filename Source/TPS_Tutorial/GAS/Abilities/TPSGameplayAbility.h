// TPS GameplayAbility 베이스 — 프로젝트 공용


#pragma once


#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "TPSGameplayAbility.generated.h"


UCLASS( Abstract )
class TPS_TUTORIAL_API UTPSGameplayAbility : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UTPSGameplayAbility();
};
