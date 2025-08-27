// 데이터테이블 활용하기 위한 데이터 유틸 컴포넌트 헤더 파일


#pragma once


#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "TPSDataComponent.generated.h"


UCLASS( ClassGroup=( Custom ), meta=( BlueprintSpawnableComponent ) )
class TPS_TUTORIAL_API UTPSDataComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	// Sets default values for this component's properties
	UTPSDataComponent();

	// 데이터 행 참조 핸들
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Data" )
	FDataTableRowHandle DataRowHandle;

	// 데이터를 반환한다.
	// FWeaponData* GetData() const { return DataRowHandle.GetRow< FWeaponData >(TEXT( "DTRow" ) ); }
	template< typename RowT >
	const RowT* GetData( const TCHAR* Context = TEXT( "DTRow" ) ) const
	{
		static_assert( std::is_base_of_v< FTableRowBase, RowT >, "RowT must be a FTableRowBase" );

		if ( DataRowHandle.IsNull() ) return nullptr;
		if ( DataRowHandle.RowName.IsNone() ) return nullptr;

		return DataRowHandle.GetRow< RowT >( Context );
	}

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

public:
// 	// Called every frame
// 	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
// 	                           FActorComponentTickFunction* ThisTickFunction) override;
};
