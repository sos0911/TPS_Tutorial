// TPS 프로젝트 로그 소스 파일


#include "TPSLog.h"
#include "HAL/IConsoleManager.h"
#include "Misc/OutputDevice.h"


// TPS 인게임 로그 카테고리 정의
DEFINE_LOG_CATEGORY(LogGameplay);


// 콘솔 명령 구현: tps.DumpStack
static void TPS_DumpStackCmd()
{
	UE_LOG(LogGameplay, Log, TEXT("[TPS] 현재 콜스택을 로그로 덤프합니다..."));

	// StackWalk으로 버퍼에 담은 뒤 LogGameplay 카테고리로 출력하여 필터에 의해 프레임이 숨지지 않도록 한다.
	const SIZE_T StackTraceSize = 65535;
	ANSICHAR* StackTrace = (ANSICHAR*)FMemory::SystemMalloc( StackTraceSize );
	if ( StackTrace )
	{
		StackTrace[ 0 ] = 0;
		// 이 함수(TPS_DumpStackCmd)와 호출 래퍼 프레임을 건너뛰어 사용자 코드 프레임부터 보이게 한다.
		const int32 NumFramesToIgnore = 2;
		FPlatformStackWalk::StackWalkAndDumpEx(
			StackTrace,
			StackTraceSize,
			NumFramesToIgnore,
			FGenericPlatformStackWalk::EStackWalkFlags::AccurateStackWalk,
			nullptr );

		// 전체 스택을 한 번에 우리 카테고리로 출력하여 엔진/게임 프레임이 모두 보이도록 한다.
		UE_LOG( LogGameplay, Log, TEXT( "%s" ), ANSI_TO_TCHAR( StackTrace ) );
		FMemory::SystemFree( StackTrace );
	}
	else
	{
		UE_LOG( LogGameplay, Warning, TEXT("[TPS] 스택 트레이스 버퍼 할당에 실패했습니다." ) );
	}
}

static FAutoConsoleCommand GDumpStackCmd(
	TEXT( "tps.DumpStack" ),
	TEXT( "현재 콜스택을 로그로 덤프합니다(엔진 프레임 포함, 전체 스택)." ),
	FConsoleCommandDelegate::CreateStatic( &TPS_DumpStackCmd )
);

void TPSLog::DumpCurrentCallstack()
{
	TPS_DumpStackCmd();
}
