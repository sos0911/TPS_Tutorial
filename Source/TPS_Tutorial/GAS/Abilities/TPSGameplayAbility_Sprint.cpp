// TPS Sprint 어빌리티 구현


#include "GAS/Abilities/TPSGameplayAbility_Sprint.h"
#include "AbilitySystemComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GAS/TPSGameplayTags.h"
#include "Log/TPSLog.h"


UTPSGameplayAbility_Sprint::UTPSGameplayAbility_Sprint()
{
	// 어빌리티 자체에 부여되는 식별 태그
	AbilityTags.AddTag( TAG_Ability_Sprint );

	// Sprint 활성 동안 보유할 상태 태그
	ActivationOwnedTags.AddTag( TAG_State_Sprinting );

	// 사망 상태에서는 활성화 불가
	ActivationBlockedTags.AddTag( TAG_State_Dead );
}

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

	ACharacter* character = ActorInfo ? Cast<ACharacter>( ActorInfo->AvatarActor.Get() ) : nullptr;
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

	UE_LOG( LogGameplay, Log, TEXT("[TPS] GAS Sprint Activated") );
}

void UTPSGameplayAbility_Sprint::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled )
{
	// Stamina drain GE 즉시 Remove (자동 회복 정책은 후속 PR)
	if ( ActiveStaminaDrainHandle.IsValid() )
	{
		if ( UAbilitySystemComponent* asc = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr )
		{
			asc->RemoveActiveGameplayEffect( ActiveStaminaDrainHandle );
		}
		ActiveStaminaDrainHandle = FActiveGameplayEffectHandle();
	}

	// 이동 속도 원복
	if ( ACharacter* character = ActorInfo ? Cast<ACharacter>( ActorInfo->AvatarActor.Get() ) : nullptr )
	{
		if ( UCharacterMovementComponent* moveComp = character->GetCharacterMovement() )
		{
			moveComp->MaxWalkSpeed = CachedWalkSpeed;
		}
	}

	UE_LOG( LogGameplay, Log, TEXT("[TPS] GAS Sprint Ended (cancelled=%d)"), bWasCancelled ? 1 : 0 );

	Super::EndAbility( Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled );
}
