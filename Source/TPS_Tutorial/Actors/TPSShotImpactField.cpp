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

	// Geometry Collection 찾아서 데미지 확인
	TArray<AActor*> FoundActors;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), AActor::StaticClass(), FoundActors);
	
	for (AActor* Actor : FoundActors)
	{
		if (Actor->GetName().Contains(TEXT("GeometryCollection")))
		{
			UGeometryCollectionComponent* GeomComp = Actor->FindComponentByClass<UGeometryCollectionComponent>();
			if (GeomComp)
			{
				float Distance = FVector::Distance(GetActorLocation(), Actor->GetActorLocation());
				UE_LOG(LogTemp, Warning, TEXT("Found Vase! Distance: %.2f, Simulating: %s"), 
					Distance, GeomComp->IsSimulatingPhysics() ? TEXT("YES") : TEXT("NO"));
				
				// 물리 활성화 시도
				GeomComp->SetSimulatePhysics(true);
				
				// 직접 힘 가하기 테스트
				FVector ImpulseDir = (Actor->GetActorLocation() - GetActorLocation()).GetSafeNormal();
				GeomComp->AddImpulse(ImpulseDir * 100000.0f);
				
				UE_LOG(LogTemp, Warning, TEXT("Applied impulse to Vase!"));
			}
			break;
		}
	}
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

