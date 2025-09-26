// TPS 픽업용 무기 액터 클래스 소스 파일


#include "TPSPickUpBase.h"


// Sets default values
ATPSPickUpBase::ATPSPickUpBase()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
}

// Called when the game starts or when spawned
void ATPSPickUpBase::BeginPlay()
{
	Super::BeginPlay();

	// 충돌한 캐릭터 클래스에 의해서만 충돌 핸들링 처리를 한다.
	// 추후 이벤트리스너 방식도 고려해 보자.
	// if ( USphereComponent* sphereComponent = FindComponentByClass< USphereComponent >() ) sphereComponent->OnComponentBeginOverlap.AddDynamic( this, &ATPSPickUpBase::OnBeginOverlap );
}

// 무기를 줍는 상호작용을 실행한다.
bool ATPSPickUpBase::HandlePickUpWeaponInteract( AActor* OtherActor )
{
	// TODO : 액터 디스폰 처리 및 정리
	return Destroy();
}

// 오버랩이 시작되었음을 알리는 이벤트를 처리한다.
void ATPSPickUpBase::OnBeginOverlap( UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult )
{
	HandlePickUpWeaponInteract( this );
}

// Called every frame
void ATPSPickUpBase::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

