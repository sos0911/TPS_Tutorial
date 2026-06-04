// Copyright AI Assistant. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

/**
 * Execution result returned by all command handlers.
 * Matches the frontend ExecutionResult interface.
 */
struct FCommandResult
{
	bool bSuccess = false;
	TSharedPtr<FJsonObject> Data;
	FString Error;

	/** Create a success result with data */
	static FCommandResult Success(TSharedPtr<FJsonObject> InData)
	{
		FCommandResult R;
		R.bSuccess = true;
		R.Data = InData;
		return R;
	}

	/** Create a success result with a simple message */
	static FCommandResult SuccessMessage(const FString& Message)
	{
		TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
		Obj->SetStringField(TEXT("message"), Message);
		return Success(Obj);
	}

	/** Create a failure result */
	static FCommandResult Fail(const FString& InError)
	{
		FCommandResult R;
		R.bSuccess = false;
		R.Error = InError;
		return R;
	}

	/** Serialize to JSON string */
	FString ToJsonString() const
	{
		TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
		Root->SetBoolField(TEXT("success"), bSuccess);

		if (Data.IsValid())
		{
			Root->SetObjectField(TEXT("data"), Data);
		}
		else
		{
			Root->SetField(TEXT("data"), MakeShared<FJsonValueNull>());
		}

		if (!Error.IsEmpty())
		{
			Root->SetStringField(TEXT("error"), Error);
		}
		else
		{
			Root->SetField(TEXT("error"), MakeShared<FJsonValueNull>());
		}

		FString OutputString;
		TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);
		FJsonSerializer::Serialize(Root.ToSharedRef(), Writer);
		return OutputString;
	}
};

/**
 * Interface for command handlers.
 * Each handler manages a group of related commands (e.g. Actor, Blueprint, Material).
 */
class ICommandHandler
{
public:
	virtual ~ICommandHandler() = default;

	/** Return the list of command names this handler supports */
	virtual TArray<FString> GetSupportedCommands() const = 0;

	/** Execute a command with given arguments. Called on GameThread. */
	virtual FCommandResult Execute(const FString& Command, const TSharedPtr<FJsonObject>& Args) = 0;
};
