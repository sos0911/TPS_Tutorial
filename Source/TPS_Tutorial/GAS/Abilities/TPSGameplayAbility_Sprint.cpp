// TPS Sprint 어빌리티 구현


#include "GAS/Abilities/TPSGameplayAbility_Sprint.h"
#include "AbilitySystemComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GAS/TPSAttributeSet.h"
#include "GAS/TPSGameplayTags.h"
#include "Log/TPSLog.h"


// Sprint 어빌리티 태그와 상태/차단 태그를 설정한다.
UTPSGameplayAbility_Sprint::UTPSGameplayAbility_Sprint()
{
	// 어빌리티 자체에 부여되는 식별 태그
	AbilityTags.AddTag( TAG_Ability_Sprint );

	// Sprint 활성 동안 보유할 상태 태그
	ActivationOwnedTags.AddTag( TAG_State_Sprinting );

	// 사망 상태에서는 활성화 불가
	ActivationBlockedTags.AddTag( TAG_State_Dead );
}

// 스태미나가 최소치 이하이면 발동을 막는다 (고갈 직후 키 재입력으로 즉시 재발동 방지).
bool UTPSGameplayAbility_Sprint::CanActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayTagContainer* SourceTags,
	const FGameplayTagContainer* TargetTags,
	FGameplayTagContainer* OptionalRelevantTags ) const
{
	if ( !Super::CanActivateAbility( Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags ) )
	{
		return false;
	}

	const UAbilitySystemComponent* asc = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	if ( !asc ) return false;

	// 어트리뷰트 세트가 없으면 게이트하지 않음
	const UTPSAttributeSet* attrSet = asc->GetSet< UTPSAttributeSet >();
	if ( !attrSet ) return true;

	return attrSet->GetStamina() > MinStaminaToSprint;
}

// 이동 속도를 스프린트 속도로 올리고 스태미나 드레인 GE를 적용한다.
void UTPSGameplayAbility_Sprint::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData )
{
	if ( !CommitAbility( Handle, ActorInfo, ActivationInfo ) )
	{
		EndAbility( Handle, ActorInfo, ActivationInfo, true, true );
		return;
	}

	ACharacter* character = ActorInfo ? Cast< ACharacter >( ActorInfo->AvatarActor.Get() ) : nullptr;
	if ( !character )
	{
		EndAbility( Handle, ActorInfo, ActivationInfo, true, true );
		return;
	}

	UCharacterMovementComponent* moveComp = character->GetCharacterMovement();
	if ( !moveComp )
	{
		EndAbility( Handle, ActorInfo, ActivationInfo, true, true );
		return;
	}

	// 평상시 속도 캐시 후 스프린트 속도로 변경
	CachedWalkSpeed = moveComp->MaxWalkSpeed;
	moveComp->MaxWalkSpeed = SprintSpeed;

	// Stamina drain GE 적용 (Duration GE — EndAbility에서 즉시 Remove)
	if ( UAbilitySystemComponent* asc = ActorInfo->AbilitySystemComponent.Get() )
	{
		if ( StaminaDrainEffect )
		{
			FGameplayEffectContextHandle ctx = asc->MakeEffectContext();
			ctx.AddSourceObject( this );

			const FGameplayEffectSpecHandle specHandle = asc->MakeOutgoingSpec( StaminaDrainEffect, GetAbilityLevel(), ctx );
			if ( specHandle.IsValid() )
			{
				ActiveStaminaDrainHandle = asc->ApplyGameplayEffectSpecToSelf( *specHandle.Data.Get() );
			}
		}
	}

	UE_LOG( LogGameplay, Log, TEXT( "[TPS] GAS Sprint Activated" ) );
}

// 스태미나 드레인 GE를 제거하고 이동 속도를 원복한다.
void UTPSGameplayAbility_Sprint::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled )
{
	// Stamina drain GE 즉시 Remove (종료 후 State.Sprinting 해제 → StaminaRegen GE가 자동 회복 재개)
	if ( ActiveStaminaDrainHandle.IsValid() )
	{
		if ( UAbilitySystemComponent* asc = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr )
		{
			asc->RemoveActiveGameplayEffect( ActiveStaminaDrainHandle );
		}
		ActiveStaminaDrainHandle = FActiveGameplayEffectHandle();
	}

	// 이동 속도 원복
	if ( ACharacter* character = ActorInfo ? Cast< ACharacter >( ActorInfo->AvatarActor.Get() ) : nullptr )
	{
		if ( UCharacterMovementComponent* moveComp = character->GetCharacterMovement() )
		{
			moveComp->MaxWalkSpeed = CachedWalkSpeed;
		}
	}

	UE_LOG( LogGameplay, Log, TEXT( "[TPS] GAS Sprint Ended (cancelled=%d)" ), bWasCancelled ? 1 : 0 );

	Super::EndAbility( Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled );
}
