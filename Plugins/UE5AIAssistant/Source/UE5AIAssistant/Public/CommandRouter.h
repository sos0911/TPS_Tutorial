// Copyright AI Assistant. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Commands/ICommandHandler.h"

/**
 * Routes incoming commands to the appropriate handler.
 * Maintains a registry of command name -> handler mapping.
 */
class FCommandRouter
{
public:
	FCommandRouter();
	~FCommandRouter();

	/** Register a command handler. Automatically maps all its supported commands. */
	void RegisterHandler(TSharedPtr<ICommandHandler> Handler);

	/** Execute a command by name. Returns fail result if command is unknown. */
	FCommandResult Execute(const FString& Command, const TSharedPtr<FJsonObject>& Args);

	/** Get list of all registered command names */
	TArray<FString> GetAllCommands() const;

	/** Get command descriptions for /api/commands endpoint */
	TSharedPtr<FJsonObject> GetCommandsInfo() const;

private:
	/** Command name -> Handler mapping */
	TMap<FString, TSharedPtr<ICommandHandler>> CommandMap;

	/** All registered handlers (for lifecycle management) */
	TArray<TSharedPtr<ICommandHandler>> Handlers;
};
