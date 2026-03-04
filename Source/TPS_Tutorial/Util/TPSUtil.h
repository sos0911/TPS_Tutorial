// 유틸 함수 모음 헤더 파일


#pragma once


#include "Consts/TPSConsts.h"


namespace TPSUtil
{
	// 변수명이 Name인 T 타입의 오브젝트 프로퍼티의 Value를 반환한다.
	template < class T >
	T* GetValueForObjProp( const UObject* Object );

	// 값을 FString으로 변환한다.
	template < typename T >
	FString ToString( T Value );
}

//////////////////
/// Implements ///
//////////////////
namespace TPSUtil
{
	// 변수명이 Name인 T 타입의 오브젝트 프로퍼티의 Value를 반환한다.
	template <class T>
	T* GetValueForObjProp( const UObject* Object )
	{
		if ( !Object ) return nullptr;

		const UClass* uclass = Object->GetClass();
		if ( !uclass ) return nullptr;

		for( TFieldIterator< FObjectProperty > it( uclass ); it; ++it )
		{
			FObjectProperty* objProperty = *it;
			if ( objProperty->PropertyClass != T::StaticClass() ) continue;

			UObject* objValue = objProperty->GetPropertyValue_InContainer( Object );
			if ( !objValue ) continue;

			return Cast< T >( objValue );
		}

		return nullptr;
	}
	
	// 값을 FString으로 변환한다.
	template <>
	inline FString ToString< int32 >( int32 Value )
	{
		return FString::FromInt( Value );
	}

	// 값을 FString으로 변환한다.
	template <>
	inline FString ToString< EWeaponType >( EWeaponType Value )
	{
		return StaticEnum< EWeaponType >()->GetNameStringByValue( static_cast< int64 >( Value ) );
	}
}
