// 장착용 스나이퍼 라이플 액터 소스 파일


#include "Actors/TPSEquipSniperRifle.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Kismet/KismetRenderingLibrary.h"


// 생성자 
ATPSEquipSniperRifle::ATPSEquipSniperRifle()
{
	// SceneCaptureComp = FindComponentByClass< USceneCaptureComponent2D >();
	// if ( SceneCaptureComp ) SceneCaptureComp->SetupAttachment( WeaponComp, TEXT( "Scope" ) );
	
	SceneCaptureComp = CreateDefaultSubobject< USceneCaptureComponent2D >( TEXT( "SceneCaptureComp" ) );
	if ( SceneCaptureComp )
	{
		SceneCaptureComp->SetupAttachment( WeaponComp, TEXT( "Scope" ) );
		SceneCaptureComp->bCaptureEveryFrame = false;
		SceneCaptureComp->bCaptureOnMovement = false;
	}
}

// 씬 캡쳐를 활성화/비활성화한다.
void ATPSEquipSniperRifle::SetSceneCaptureEnabled( bool bEnabled )
{
	if ( !SceneCaptureComp ) return;

	if ( bEnabled )
	{
		SceneCaptureComp->bCaptureEveryFrame = true;
		SceneCaptureComp->CaptureScene();
	}
	else
	{
		SceneCaptureComp->bCaptureEveryFrame = false;
	}
}

// Called when the game starts or when spawned
void ATPSEquipSniperRifle::BeginPlay()
{
	Super::BeginPlay();
	
	LensMeshComp  = FindComponentByTag< UStaticMeshComponent >( "Lens"  );
	ScopeMeshComp = FindComponentByTag< UStaticMeshComponent >( "Scope" );
	
	// TODO : 메터리얼에 스코프 소켓에서 보이는 뷰포트 화면 렌더링
	UTextureRenderTarget2D* renderTarget2D = UKismetRenderingLibrary::CreateRenderTarget2D( this, 512, 512, RTF_RGBA8 );
	
	// 씬캡쳐 컴포넌트가 매 틱 찍는 스크린샷을 renderTarget2D 로 받아온다.
	SceneCaptureComp->TextureTarget = renderTarget2D;
	if ( LensMeshComp )
	{
		UMaterialInterface* material = LensMeshComp->GetMaterial( 0 );
		if ( material )
		{
			UMaterialInstanceDynamic* mI = LensMeshComp->CreateDynamicMaterialInstance( 0, material );
			// Lens 컴포넌트에서 사용하는 메터리얼 인스턴스에 받아오는 스크린샷을 적용 후 컴포넌트에 메터리얼로 삽입
			if ( mI )
			{
				mI->SetTextureParameterValue( "RenderTarget", renderTarget2D );
				LensMeshComp->SetMaterial( 0, mI );
			}
		}
	}
}
