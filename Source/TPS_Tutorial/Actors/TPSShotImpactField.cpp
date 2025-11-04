// 사격 시 사용하는 임팩트 필드 액터 소스 파일


#include "TPSShotImpactField.h"
#include "GeometryCollection/GeometryCollectionComponent.h"
#include "Kismet/GameplayStatics.h"


// Sets default values
ATPSShotImpactField::ATPSShotImpactField()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	// PrimaryActorTick.bCanEverTick = true;
}

// Called when the game starts or when spawned
void ATPSShotImpactField::BeginPlay()
{
	Super::BeginPlay();
	OnBeginPlayed();
}

// Called every frame
void ATPSShotImpactField::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

// 위젯 경로를 반환한다. 
FString ATPSShotImpactField::GetPath()
{
	return TEXT( "/Game/CustomContents/Player/Weapons/BP_ShotImpactField.BP_ShotImpactField_C" );
}

