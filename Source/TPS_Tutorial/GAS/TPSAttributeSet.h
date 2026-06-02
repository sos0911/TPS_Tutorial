// TPS AttributeSet 헤더 — Health/MaxHealth/Stamina/MaxStamina


#pragma once


#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "AttributeSet.h"
#include "TPSAttributeSet.generated.h"


// 어트리뷰트 접근자 매크로 단축
#define ATTRIBUTE_ACCESSORS( ClassName, PropertyName ) \
	GAMEPLAYATTRIBUTE_PROPERTY_GETTER ( ClassName, PropertyName ) \
	GAMEPLAYATTRIBUTE_VALUE_GETTER    ( PropertyName ) \
	GAMEPLAYATTRIBUTE_VALUE_SETTER    ( PropertyName ) \
	GAMEPLAYATTRIBUTE_VALUE_INITTER   ( PropertyName )


// 어트리뷰트 변경 시 브로드캐스트되는 델리게이트 (NewValue, OldValue)
DECLARE_MULTICAST_DELEGATE_ThreeParams( FOnAttributeValueChanged, float /*NewValue*/, float /*MaxValue*/, float /*OldValue*/ );


UCLASS()
class TPS_TUTORIAL_API UTPSAttributeSet : public UAttributeSet
{
	GENERATED_BODY()

public:
	UTPSAttributeSet();

	// 어트리뷰트 변경 직전 — 클램프 등 사전 처리
	virtual void PreAttributeChange( const FGameplayAttribute& Attribute, float& NewValue ) override;

	// GE 적용 직후 — 사망 트리거 등 사후 처리
	virtual void PostGameplayEffectExecute( const FGameplayEffectModCallbackData& Data ) override;

public:
	// 현재 체력
	UPROPERTY( BlueprintReadOnly, Category = "Vital" )
	FGameplayAttributeData Health;
	ATTRIBUTE_ACCESSORS( UTPSAttributeSet, Health )

	// 최대 체력
	UPROPERTY( BlueprintReadOnly, Category = "Vital" )
	FGameplayAttributeData MaxHealth;
	ATTRIBUTE_ACCESSORS( UTPSAttributeSet, MaxHealth )

	// 현재 스태미나
	UPROPERTY( BlueprintReadOnly, Category = "Vital" )
	FGameplayAttributeData Stamina;
	ATTRIBUTE_ACCESSORS( UTPSAttributeSet, Stamina )

	// 최대 스태미나
	UPROPERTY( BlueprintReadOnly, Category = "Vital" )
	FGameplayAttributeData MaxStamina;
	ATTRIBUTE_ACCESSORS( UTPSAttributeSet, MaxStamina )

	// 어트리뷰트 변경 알림 델리게이트 (캐릭터/HUD가 구독)
	FOnAttributeValueChanged OnHealthChanged;
	FOnAttributeValueChanged OnStaminaChanged;
};
