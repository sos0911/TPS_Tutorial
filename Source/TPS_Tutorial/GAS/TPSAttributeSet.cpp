// TPS AttributeSet 구현


#include "GAS/TPSAttributeSet.h"
#include "GameplayEffectExtension.h"


// 어트리뷰트 디폴트 초기값을 설정한다.
UTPSAttributeSet::UTPSAttributeSet()
{
	// 디폴트 초기값 — 캐릭터 BP에서 InitAttributes GE로 다시 세팅
	InitHealth     ( 100.0f );
	InitMaxHealth  ( 100.0f );
	InitStamina    ( 100.0f );
	InitMaxStamina ( 100.0f );
}

// 어트리뷰트 값 변경 직전에 범위를 클램프한다.
void UTPSAttributeSet::PreAttributeChange( const FGameplayAttribute& Attribute, float& NewValue )
{
	Super::PreAttributeChange( Attribute, NewValue );

	if ( Attribute == GetHealthAttribute() )
	{
		NewValue = FMath::Clamp( NewValue, 0.0f, GetMaxHealth() );
	}
	else if ( Attribute == GetStaminaAttribute() )
	{
		NewValue = FMath::Clamp( NewValue, 0.0f, GetMaxStamina() );
	}
	else if ( Attribute == GetMaxHealthAttribute() )
	{
		// MaxHealth 변경 시 Health 비율 유지
		const float oldMaxHealth = GetMaxHealth();
		if ( oldMaxHealth > KINDA_SMALL_NUMBER && NewValue > KINDA_SMALL_NUMBER )
		{
			const float ratio = GetHealth() / oldMaxHealth;
			SetHealth( FMath::Clamp( ratio * NewValue, 0.0f, NewValue ) );
		}
	}
	else if ( Attribute == GetMaxStaminaAttribute() )
	{
		const float oldMaxStamina = GetMaxStamina();
		if ( oldMaxStamina > KINDA_SMALL_NUMBER && NewValue > KINDA_SMALL_NUMBER )
		{
			const float ratio = GetStamina() / oldMaxStamina;
			SetStamina( FMath::Clamp( ratio * NewValue, 0.0f, NewValue ) );
		}
	}
}

// GE 적용 후 값을 클램프하고 변경 델리게이트를 브로드캐스트한다.
void UTPSAttributeSet::PostGameplayEffectExecute( const FGameplayEffectModCallbackData& Data )
{
	Super::PostGameplayEffectExecute( Data );

	const FGameplayAttribute& attribute = Data.EvaluatedData.Attribute;

	if ( attribute == GetHealthAttribute() )
	{
		const float newValue = GetHealth();
		const float oldValue = newValue - Data.EvaluatedData.Magnitude;

		// 클램프 (Instant GE는 BaseValue를 직접 변경하므로 여기서 한 번 더 보장)
		const float clamped = FMath::Clamp( newValue, 0.0f, GetMaxHealth() );
		SetHealth( clamped );

		OnHealthChanged.Broadcast( clamped, GetMaxHealth(), oldValue );
	}
	else if ( attribute == GetStaminaAttribute() )
	{
		const float newValue = GetStamina();
		const float oldValue = newValue - Data.EvaluatedData.Magnitude;

		const float clamped = FMath::Clamp( newValue, 0.0f, GetMaxStamina() );
		SetStamina( clamped );

		OnStaminaChanged.Broadcast( clamped, GetMaxStamina(), oldValue );
	}
}
