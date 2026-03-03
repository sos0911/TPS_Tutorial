////////////////////////////////////////////////////////
// @brief 데이터 매니저 소스 파일
// @Date  2026-03-03
////////////////////////////////////////////////////////


#include "Manager/TPSDataManager.h"
#include "Log/TPSLog.h"


// 인스턴스를 생성하여 반환한다.
UTPSDataManager* UTPSDataManager::Create( UObject* Owner )
{
	return NewObject< UTPSDataManager >( Owner, UTPSDataManager::StaticClass() );
}

// 초기화한다. 모든 데이터 테이블을 로드한다.
void UTPSDataManager::Init()
{
	_LoadTable( TEXT( "DT_Weapon" ), TEXT( "/Game/CustomContents/Data/DT_Weapon" ) );
	_LoadTable( TEXT( "DT_String" ), TEXT( "/Game/CustomContents/Data/DT_String" ) );
}

// 데이터를 정리한다.
void UTPSDataManager::Clear()
{
	DataTableMap.Empty();
}

// 데이터 테이블을 반환한다.
UDataTable* UTPSDataManager::GetTable( const FName& TableName ) const
{
	UDataTable* const* tablePtr = DataTableMap.Find( TableName );
	return tablePtr ? *tablePtr : nullptr;
}

// 데이터 테이블을 로드하여 맵에 등록한다.
void UTPSDataManager::_LoadTable( const FName& TableName, const TCHAR* Path )
{
	UDataTable* table = LoadObject< UDataTable >( nullptr, Path );
	if ( !table )
	{
		UE_LOG( LogGameplay, Error, TEXT( "[TPS] DataManager: Failed to load DataTable [%s] from [%s]" ), *TableName.ToString(), Path );
		return;
	}

	DataTableMap.Add( TableName, table );
}
