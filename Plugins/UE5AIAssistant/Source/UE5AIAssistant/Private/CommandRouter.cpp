// Copyright AI Assistant. All Rights Reserved.

#include "CommandRouter.h"

FCommandRouter::FCommandRouter()
{
}

FCommandRouter::~FCommandRouter()
{
}

void FCommandRouter::RegisterHandler(TSharedPtr<ICommandHandler> Handler)
{
	if (!Handler.IsValid())
	{
		return;
	}

	Handlers.Add(Handler);

	for (const FString& Cmd : Handler->GetSupportedCommands())
	{
		if (CommandMap.Contains(Cmd))
		{
			UE_LOG(LogTemp, Warning, TEXT("[CommandRouter] Command '%s' is already registered, overwriting."), *Cmd);
		}
		CommandMap.Add(Cmd, Handler);
		UE_LOG(LogTemp, Log, TEXT("[CommandRouter] Registered command: %s"), *Cmd);
	}
}

FCommandResult FCommandRouter::Execute(const FString& Command, const TSharedPtr<FJsonObject>& Args)
{
	TSharedPtr<ICommandHandler>* HandlerPtr = CommandMap.Find(Command);
	if (!HandlerPtr || !HandlerPtr->IsValid())
	{
		return FCommandResult::Fail(FString::Printf(TEXT("Unknown command: '%s'. Use GET /api/commands to list available commands."), *Command));
	}

	return (*HandlerPtr)->Execute(Command, Args);
}

TArray<FString> FCommandRouter::GetAllCommands() const
{
	TArray<FString> Commands;
	CommandMap.GetKeys(Commands);
	Commands.Sort();
	return Commands;
}

TSharedPtr<FJsonObject> FCommandRouter::GetCommandsInfo() const
{
	TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();

	TArray<TSharedPtr<FJsonValue>> CommandArray;
	for (const FString& Cmd : GetAllCommands())
	{
		TSharedPtr<FJsonValue> CmdValue = MakeShared<FJsonValueString>(Cmd);
		CommandArray.Add(CmdValue);
	}

	Root->SetArrayField(TEXT("commands"), CommandArray);
	Root->SetNumberField(TEXT("count"), CommandArray.Num());
	return Root;
}
