// TPS AbilitySystemComponent 헤더 — 프로젝트 전용 ASC 서브클래스 (얇은 래퍼)


#pragma once


#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "TPSAbilitySystemComponent.generated.h"


UCLASS( ClassGroup = ( Custom ), meta = ( BlueprintSpawnableComponent ) )
class TPS_TUTORIAL_API UTPSAbilitySystemComponent : public UAbilitySystemComponent
{
	GENERATED_BODY()

public:
	UTPSAbilitySystemComponent();

	// 태그로 어빌리티 활성화 (편의 함수)
	bool TryActivateAbilityByTag( const FGameplayTag& AbilityTag );

	// 태그로 어빌리티 취소 (편의 함수)
	void CancelAbilityByTag( const FGameplayTag& AbilityTag );
};
