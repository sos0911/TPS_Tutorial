#pragma once


#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Logic/ITPSInteractionActorInterface.h"
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
	virtual bool HandlePickUpWeaponInteract( AActor* OtherActor ) override;

	// 오버랩이 시작되었음을 알리는 이벤트를 처리한다.
	UFUNCTION()
	void OnBeginOverlap( UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult );

public:
	// Called every frame
	virtual void Tick(float DeltaTime) override;
};
