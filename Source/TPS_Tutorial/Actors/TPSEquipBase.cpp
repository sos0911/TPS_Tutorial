// TPS 장착용 무기 액터 클래스 소스 파일


#include "TPSEquipBase.h"
#include "Components/TPSDataComponent.h"
#include "GameFramework/SpringArmComponent.h"


// Sets default values
ATPSEquipBase::ATPSEquipBase()
{
	// 현재 시점에서 틱은 필요하지 않으므로 비활성화 처리.
	PrimaryActorTick.bCanEverTick = false;
	
	WeaponComp = CreateDefaultSubobject< USkeletalMeshComponent >( TEXT( "Weapon" ) );
	RootComponent = WeaponComp;

	SpringArmComp = CreateDefaultSubobject< USpringArmComponent >( TEXT( "SpringArm" ) );
	SpringArmComp->SetupAttachment( WeaponComp );
	
	SpringArmChildComp = CreateDefaultSubobject< UChildActorComponent >( TEXT( "ChildActor" ) );
	SpringArmChildComp->SetupAttachment( SpringArmComp );
	
	DataComp = CreateDefaultSubobject< UTPSDataComponent >( TEXT( "TPSDataComponent" ) );
}

// 무기를 발사하는 상호작용을 실행한다.
bool ATPSEquipBase::HandleFireWeaponInteract()
{
	Fire();

	return true;
}

// Called when the game starts or when spawned
void ATPSEquipBase::BeginPlay()
{
	Super::BeginPlay();
}

