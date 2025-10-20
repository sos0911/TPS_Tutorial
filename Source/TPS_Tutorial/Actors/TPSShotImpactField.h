// 사격 시 사용하는 임팩트 필드 액터 헤더 파일


#pragma once


#include "CoreMinimal.h"
#include "Field/FieldSystemActor.h"
#include "TPSShotImpactField.generated.h"


UCLASS()
class TPS_TUTORIAL_API ATPSShotImpactField : public AFieldSystemActor
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	ATPSShotImpactField();

protected:
	UFUNCTION( BlueprintImplementableEvent, Category = "Initialize" )
	void OnBeginPlayed();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	// 위젯 경로를 반환한다. 
	static FString GetPath();
};
