// TPS GAS 네이티브 게임플레이 태그 헤더


#pragma once


#include "CoreMinimal.h"
#include "NativeGameplayTags.h"


// 어빌리티 태그
TPS_TUTORIAL_API UE_DECLARE_GAMEPLAY_TAG_EXTERN( TAG_Ability_Sprint );
TPS_TUTORIAL_API UE_DECLARE_GAMEPLAY_TAG_EXTERN( TAG_Ability_Fire   );

// 상태 태그
TPS_TUTORIAL_API UE_DECLARE_GAMEPLAY_TAG_EXTERN( TAG_State_Dead      );
TPS_TUTORIAL_API UE_DECLARE_GAMEPLAY_TAG_EXTERN( TAG_State_Sprinting );

// SetByCaller 키
TPS_TUTORIAL_API UE_DECLARE_GAMEPLAY_TAG_EXTERN( TAG_Data_Damage );
