// Copyright AI Assistant. All Rights Reserved.

#include "UE5AIAssistantModule.h"
#include "HttpCommandServer.h"

#define LOCTEXT_NAMESPACE "FUE5AIAssistantModule"

void FUE5AIAssistantModule::StartupModule()
{
	CommandServer = MakeUnique<FHttpCommandServer>();
	CommandServer->Start(58080);

	UE_LOG(LogTemp, Log, TEXT("[UE5AIAssistant] Plugin loaded. HTTP Server starting on port 58080..."));
}

void FUE5AIAssistantModule::ShutdownModule()
{
	if (CommandServer)
	{
		CommandServer->Stop();
		CommandServer.Reset();
	}

	UE_LOG(LogTemp, Log, TEXT("[UE5AIAssistant] Plugin unloaded. HTTP Server stopped."));
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FUE5AIAssistantModule, UE5AIAssistant)
