// TPS 장착용 무기 액터 클래스 헤더 파일


#pragma once


#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Logic/ITPSInteractionActorInterface.h"
#include "TPSEquipBase.generated.h"


UCLASS()
class TPS_TUTORIAL_API ATPSEquipBase : public AActor, public ITPSEquipInteractionActorInterface
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	ATPSEquipBase();

protected:
	// 무기를 발사하는 상호작용을 실행한다.
	// UFUNCTION( BlueprintNativeEvent, BlueprintCallable, Category = "Interaction Control" )
	UFUNCTION( BlueprintCallable, Category = "Interaction Control" )
	virtual bool HandleFireWeaponInteract() override;

	UFUNCTION( BlueprintImplementableEvent, Category = "Interaction Control" )
	void Fire();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;
};
