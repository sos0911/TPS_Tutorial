// 장착용 스나이퍼 라이플 액터 헤더 파일


#pragma once


#include "CoreMinimal.h"
#include "Actors/TPSEquipBase.h"
#include "TPSEquipSniperRifle.generated.h"


UCLASS()
class TPS_TUTORIAL_API ATPSEquipSniperRifle : public ATPSEquipBase
{
	GENERATED_BODY()
	
protected:
	UPROPERTY( EditAnywhere, BlueprintReadWrite, Category = "Components" )
	USceneCaptureComponent2D* SceneCaptureComp = nullptr; // 스코프 씬 캡쳐 컴포넌트
	
	UStaticMeshComponent*     LensMeshComp     = nullptr; // 렌즈 스태틱 메쉬 컴포넌트
	UStaticMeshComponent*     ScopeMeshComp    = nullptr; // 스코프 스태틱 메쉬 컴포넌트 
	
public:
	// 생성자
	ATPSEquipSniperRifle();

	// 씬 캡쳐를 활성화/비활성화한다.
	void SetSceneCaptureEnabled( bool bEnabled );

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

};
