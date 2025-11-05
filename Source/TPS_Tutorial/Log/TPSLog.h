// TPS 프로젝트 로그 헤더 파일


#pragma once


#include "CoreMinimal.h"


// TPS 인게임 로그 카테고리 선언
DECLARE_LOG_CATEGORY_EXTERN(LogGameplay, Log, All);


// 디버깅 유틸리티
class TPSLog
{
public:
	// 현재 스레드의 콜스택을 로그로 덤프한다.
	static void DumpCurrentCallstack();
};
