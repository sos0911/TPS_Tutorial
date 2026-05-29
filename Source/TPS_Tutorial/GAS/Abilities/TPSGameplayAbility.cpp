// TPS GameplayAbility 베이스 구현


#include "GAS/Abilities/TPSGameplayAbility.h"


// 어빌리티 공통 인스턴싱/네트 실행 정책 기본값을 설정한다.
UTPSGameplayAbility::UTPSGameplayAbility()
{
	// 기본 인스턴싱 정책: 호출자별 인스턴스 (어빌리티 상태를 자유롭게 가질 수 있음)
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;

	// 기본 네트 실행 정책: 로컬 예측 (단일 플레이어 전제)
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
}
