// TPS 프로젝트 로그 소스 파일

#include "TPSLog.h"
#include "HAL/IConsoleManager.h"
#include "Misc/OutputDevice.h"

// TPS 인게임 로그 카테고리 정의
DEFINE_LOG_CATEGORY(LogGameplay);

// 콘솔 명령 구현: tps.DumpStack
static void TPS_DumpStackCmd()
{
	UE_LOG(LogGameplay, Log, TEXT("[TPS] Dumping current call stack to log..."));

	// StackWalk into a buffer and emit through LogGameplay so category filters don't hide frames.
	const SIZE_T StackTraceSize = 65535;
	ANSICHAR* StackTrace = (ANSICHAR*)FMemory::SystemMalloc( StackTraceSize );
	if ( StackTrace )
	{
		StackTrace[ 0 ] = 0;
		// Ignore this function (TPS_DumpStackCmd) and the caller wrapper to show user frames first.
		const int32 NumFramesToIgnore = 2;
		FPlatformStackWalk::StackWalkAndDumpEx(
			StackTrace,
			StackTraceSize,
			NumFramesToIgnore,
			FGenericPlatformStackWalk::EStackWalkFlags::AccurateStackWalk,
			nullptr );

		// Log the entire stack as one block under our category so both engine and game frames are visible.
		UE_LOG( LogGameplay, Log, TEXT( "%s" ), ANSI_TO_TCHAR( StackTrace ) );
		FMemory::SystemFree( StackTrace );
	}
	else
	{
		UE_LOG( LogGameplay, Warning, TEXT("[TPS] Failed to allocate buffer for stack trace." ) );
	}
}

static FAutoConsoleCommand GDumpStackCmd(
	TEXT( "tps.DumpStack" ),
	TEXT( "Dump current call stack to the log (full stack, includes engine frames)." ),
	FConsoleCommandDelegate::CreateStatic( &TPS_DumpStackCmd )
);

void TPSLog::DumpCurrentCallstack()
{
	TPS_DumpStackCmd();
}
