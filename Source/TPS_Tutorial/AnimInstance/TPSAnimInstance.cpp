// TPS 애님 인스턴스 소스 파일


#include "TPSAnimInstance.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "GAS/TPSGameplayTags.h"


// 틱당 애니메이션 업데이트 용으로 호출한다. 
void UTPSAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);
	
	if ( UAbilitySystemComponent* asc = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor( GetOwningActor() ) )
	{
		bIsSprinting = asc->HasMatchingGameplayTag( TAG_State_Sprinting );
	}
}
