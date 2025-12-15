// TPS 장착용 무기 액터 클래스 헤더 파일


#pragma once


#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Logic/ITPSInteractionActorInterface.h"
#include "TPSEquipBase.generated.h"


class UTPSDataComponent;
class USpringArmComponent;
class UChildActorComponent;
class USkeletalMeshComponent;


UCLASS()
class TPS_TUTORIAL_API ATPSEquipBase : public AActor, public ITPSEquipInteractionActorInterface
{
	GENERATED_BODY()
	
protected:
	UPROPERTY( EditAnywhere, BlueprintReadWrite, Category = "Components" )
	USkeletalMeshComponent* WeaponComp = nullptr; // 무기 스켈레톤 메쉬 컴포넌트
	
	UPROPERTY( EditAnywhere, BlueprintReadWrite, Category = "Components" )
	USpringArmComponent* SpringArmComp = nullptr; // 스프링암 컴포넌트
	
	UPROPERTY( EditAnywhere, BlueprintReadWrite, Category = "Components" )
	UChildActorComponent* SpringArmChildComp = nullptr; // 스프링암 자식 컴포넌트
	
	UPROPERTY( EditAnywhere, BlueprintReadWrite, Category = "Components" )
	UTPSDataComponent* DataComp = nullptr; // 데이터 컴포넌트

public:
	// Sets default values for this actor's properties
	ATPSEquipBase();

protected:
	UFUNCTION( BlueprintCallable, Category = "Interaction Control" )
	virtual bool HandleFireWeaponInteract() override;

	UFUNCTION( BlueprintImplementableEvent, Category = "Interaction Control" )
	void Fire();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;
};
