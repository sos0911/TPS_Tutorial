////////////////////////////////////////////////////////
// @brief 데이터 매니저 헤더 파일
// @Date  2026-03-03
////////////////////////////////////////////////////////


#pragma once


#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "GameInstance/TPSGameInstance.h"
#include "TPSDataManager.generated.h"


UCLASS()
class TPS_TUTORIAL_API UTPSDataManager : public UObject
{
	GENERATED_BODY()

private:
	UPROPERTY()
	TMap< FName, UDataTable* > DataTableMap; // 데이터 테이블 맵

public:
	// 인스턴스를 생성하여 반환한다.
	static UTPSDataManager* Create( UObject* Owner );

	// 초기화한다. 모든 데이터 테이블을 로드한다.
	void Init();

	// 데이터를 정리한다.
	void Clear();

	// 데이터 테이블을 반환한다.
	UDataTable* GetTable( const FName& TableName ) const;

	// 데이터 테이블에서 Row를 찾아 반환한다.
	template< typename RowT >
	const RowT* FindRow( const FName& TableName, const FName& RowName, const TCHAR* Context = TEXT( "DataMgr" ) ) const
	{
		static_assert( std::is_base_of_v< FTableRowBase, RowT >, "RowT must derive from FTableRowBase" );

		UDataTable* table = GetTable( TableName );
		if ( !table ) return nullptr;

		return table->FindRow< RowT >( RowName, Context );
	}

private:
	// 데이터 테이블을 로드하여 맵에 등록한다.
	void _LoadTable( const FName& TableName, const TCHAR* Path );
};

// 데이터 매니저를 반환한다.
inline UTPSDataManager* GetTPSDataManager()
{
	UTPSGameInstance* gameInstance = UTPSGameInstance::GetGameInstance();
	if ( !gameInstance ) return nullptr;

	return gameInstance->GetDataManager();
}
