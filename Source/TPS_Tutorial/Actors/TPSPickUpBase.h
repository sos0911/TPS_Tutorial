#pragma once


#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Logic/UTPSInteractionActorInterface.h"
#include "TPSPickUpBase.generated.h"


UCLASS()
class TPS_TUTORIAL_API ATPSPickUpBase : public AActor, public ITPSPickUpInteractionActorInterface
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	ATPSPickUpBase();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	// 무기를 줍는 상호작용을 실행한다.
	virtual void HandlePickUpWeaponInteract() override;

public:
	// Called every frame
	virtual void Tick(float DeltaTime) override;
};
