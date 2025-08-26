#pragma once


#include "CoreMinimal.h"
#include "Runtime/CoreUObject/Public/UObject/Interface.h"
#include "UTPSInteractionActorInterface.generated.h"


// UINTERFACE( MinimalAPI, meta = ( CannotImplementInterfaceInBlueprint ) )
UINTERFACE( MinimalAPI )
class UTPSInteractionActorInterface : public UInterface
{
	GENERATED_BODY()

// public:
// 	// 상호작용한다.
// 	virtual void Interact() = 0;
};

UINTERFACE( MinimalAPI )
class UTPSPickUpInteractionActorInterface : public UTPSInteractionActorInterface
{
	GENERATED_BODY()

// public:
// 	// 무기를 줍는 상호작용을 실행한다.
// 	virtual void HandlePickUpWeaponInteract() = 0;
};

class ITPSInteractionActorInterface
{
	GENERATED_BODY()

public:
	// 상호작용한다.
	virtual void Interact() = 0;
};

class ITPSPickUpInteractionActorInterface : public ITPSInteractionActorInterface
{
	GENERATED_BODY()

public:
	// 무기를 줍는 상호작용을 실행한다.
	virtual void HandlePickUpWeaponInteract() = 0;

	// 상호작용한다.
	virtual void Interact() override;
};