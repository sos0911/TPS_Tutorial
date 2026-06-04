// TPS 애님 인스턴스 헤더 파일


#pragma once


#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "TPSAnimInstance.generated.h"


UCLASS()
class TPS_TUTORIAL_API UTPSAnimInstance : public UAnimInstance
{
	GENERATED_BODY()
	
public:
	UPROPERTY( BlueprintReadOnly, Category = "TPS|Locomotion" )
	bool bIsSprinting = false; // 현재 스프린팅 중인지 여부 ( 뛰는 중 )
	
public:
	// 틱당 애니메이션 업데이트 용으로 호출한다. 
	virtual void NativeUpdateAnimation( float DeltaSeconds ) override;
};
