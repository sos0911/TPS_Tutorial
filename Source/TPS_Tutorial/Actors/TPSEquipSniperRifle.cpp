// 장착용 스나이퍼 라이플 액터 소스 파일


#include "Actors/TPSEquipSniperRifle.h"
#include "Components/SceneCaptureComponent2D.h"


// 생성자 
ATPSEquipSniperRifle::ATPSEquipSniperRifle()
{
	// SceneCaptureComp = FindComponentByClass< USceneCaptureComponent2D >();
	// if ( SceneCaptureComp ) SceneCaptureComp->SetupAttachment( WeaponComp, TEXT( "Scope" ) );
	
	SceneCaptureComp = CreateDefaultSubobject< USceneCaptureComponent2D >( TEXT( "SceneCaptureComp" ) );
	if ( SceneCaptureComp ) SceneCaptureComp->SetupAttachment( WeaponComp, TEXT( "Scope" ) );
}

// Called when the game starts or when spawned
void ATPSEquipSniperRifle::BeginPlay()
{
	Super::BeginPlay();
	
	// TODO : 메터리얼에 스코프 소켓에서 보이는 뷰포트 화면 렌더링
	//CreateRenderTarget()
}
