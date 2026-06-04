// Copyright AI Assistant. All Rights Reserved.

#include "HttpCommandServer.h"
#include "HttpServerModule.h"
#include "IHttpRouter.h"
#include "HttpServerRequest.h"
#include "HttpServerResponse.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Misc/App.h"
#include "Editor.h"

// Include all command handlers
#include "Commands/ActorCommandHandler.h"
#include "Commands/BlueprintCommandHandler.h"
#include "Commands/BlueprintNodeHandler.h"
#include "Commands/MaterialCommandHandler.h"
#include "Commands/AssetCommandHandler.h"
#include "Commands/EditorCommandHandler.h"
#include "Commands/AnimationCommandHandler.h"
#include "Commands/GenericPropertyHandler.h"

FHttpCommandServer::FHttpCommandServer()
{
}

FHttpCommandServer::~FHttpCommandServer()
{
	Stop();
}

void FHttpCommandServer::Start(uint32 Port)
{
	if (bIsRunning)
	{
		UE_LOG(LogTemp, Warning, TEXT("[HttpCommandServer] Server already running on port %d"), ServerPort);
		return;
	}

	ServerPort = Port;

	// Register all command handlers
	Router.RegisterHandler(MakeShared<FActorCommandHandler>());
	Router.RegisterHandler(MakeShared<FBlueprintCommandHandler>());
	Router.RegisterHandler(MakeShared<FBlueprintNodeHandler>());
	Router.RegisterHandler(MakeShared<FMaterialCommandHandler>());
	Router.RegisterHandler(MakeShared<FAssetCommandHandler>());
	Router.RegisterHandler(MakeShared<FEditorCommandHandler>());
	Router.RegisterHandler(MakeShared<FAnimationCommandHandler>());
	Router.RegisterHandler(MakeShared<FGenericPropertyHandler>());

	// Start HTTP server
	FHttpServerModule& HttpServerModule = FHttpServerModule::Get();
	TSharedPtr<IHttpRouter> HttpRouter = HttpServerModule.GetHttpRouter(Port);

	if (!HttpRouter.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("[HttpCommandServer] Failed to get HTTP router on port %d"), Port);
		return;
	}

	// POST /api/execute - Main command execution endpoint
	RouteHandles.Add(HttpRouter->BindRoute(
		FHttpPath(TEXT("/api/execute")),
		EHttpServerRequestVerbs::VERB_POST | EHttpServerRequestVerbs::VERB_OPTIONS,
		FHttpRequestHandler( [this]( const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete ) { return HandleExecute( Request, OnComplete ); } )
	));

	// GET /api/ping - Health check
	RouteHandles.Add(HttpRouter->BindRoute(
		FHttpPath(TEXT("/api/ping")),
		EHttpServerRequestVerbs::VERB_GET | EHttpServerRequestVerbs::VERB_OPTIONS,
		FHttpRequestHandler( [this]( const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete ) { return HandlePing( Request, OnComplete ); } )
	));

	// GET /api/commands - List available commands
	RouteHandles.Add(HttpRouter->BindRoute(
		FHttpPath(TEXT("/api/commands")),
		EHttpServerRequestVerbs::VERB_GET | EHttpServerRequestVerbs::VERB_OPTIONS,
		FHttpRequestHandler( [this]( const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete ) { return HandleCommands( Request, OnComplete ); } )
	));

	HttpServerModule.StartAllListeners();
	bIsRunning = true;

	UE_LOG(LogTemp, Log, TEXT("[HttpCommandServer] Started on port %d with %d commands registered."),
		Port, Router.GetAllCommands().Num());
}

void FHttpCommandServer::Stop()
{
	if (!bIsRunning)
	{
		return;
	}

	// Unbind routes
	FHttpServerModule& HttpServerModule = FHttpServerModule::Get();
	for (const FHttpRouteHandle& Handle : RouteHandles)
	{
		HttpServerModule.GetHttpRouter(ServerPort)->UnbindRoute(Handle);
	}
	RouteHandles.Empty();

	bIsRunning = false;
	UE_LOG(LogTemp, Log, TEXT("[HttpCommandServer] Stopped."));
}

// ============================================
// POST /api/execute
// Body: { "command": "spawn_actor", "args": { "name": "MyCube", ... } }
// NOTE: FHttpServerModule callbacks are already on GameThread (via Editor Tick),
//       so we execute commands directly without AsyncTask dispatch.
// ============================================
bool FHttpCommandServer::HandleExecute(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete)
{
	// Handle CORS preflight
	if (Request.Verb == EHttpServerRequestVerbs::VERB_OPTIONS)
	{
		OnComplete(CreateCorsPreflightResponse());
		return true;
	}

	// Parse JSON body — construct FString with explicit length (Body is NOT null-terminated)
	FString BodyString;
	if (Request.Body.Num() > 0)
	{
		FUTF8ToTCHAR Converter(reinterpret_cast<const char*>(Request.Body.GetData()), Request.Body.Num());
		BodyString = FString(Converter.Length(), Converter.Get());
	}

	TSharedPtr<FJsonObject> RequestJson;
	TSharedRef<TJsonReader<>> JsonReader = TJsonReaderFactory<>::Create(BodyString);
	if (!FJsonSerializer::Deserialize(JsonReader, RequestJson) || !RequestJson.IsValid())
	{
		FCommandResult ErrorResult = FCommandResult::Fail(TEXT("Invalid JSON in request body"));
		OnComplete(CreateJsonResponse(ErrorResult.ToJsonString(), 400));
		return true;
	}

	// Extract command and args
	FString Command;
	if (!RequestJson->TryGetStringField(TEXT("command"), Command) || Command.IsEmpty())
	{
		FCommandResult ErrorResult = FCommandResult::Fail(TEXT("Missing 'command' field in request body"));
		OnComplete(CreateJsonResponse(ErrorResult.ToJsonString(), 400));
		return true;
	}

	// Use TryGetObjectField to avoid assertion failure when "args" key is missing
	TSharedPtr<FJsonObject> Args;
	const TSharedPtr<FJsonObject>* ArgsPtr = nullptr;
	if (RequestJson->TryGetObjectField(TEXT("args"), ArgsPtr) && ArgsPtr)
	{
		Args = *ArgsPtr;
	}
	else
	{
		Args = MakeShared<FJsonObject>(); // Empty args if not provided
	}

	// Execute directly - already on GameThread
	FCommandResult Result = Router.Execute(Command, Args);

	int32 StatusCode = Result.bSuccess ? 200 : 500;
	OnComplete(CreateJsonResponse(Result.ToJsonString(), StatusCode));
	return true;
}

// ============================================
// GET /api/ping
// NOTE: Already on GameThread, access GEditor directly.
// ============================================
bool FHttpCommandServer::HandlePing(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete)
{
	if (Request.Verb == EHttpServerRequestVerbs::VERB_OPTIONS)
	{
		OnComplete(CreateCorsPreflightResponse());
		return true;
	}

	TSharedPtr<FJsonObject> PingData = MakeShared<FJsonObject>();
	PingData->SetStringField(TEXT("status"), TEXT("ok"));
	PingData->SetStringField(TEXT("plugin"), TEXT("UE5AIAssistant"));
	PingData->SetStringField(TEXT("version"), TEXT("1.0"));
	PingData->SetStringField(TEXT("engineVersion"), FApp::GetBuildVersion());
	PingData->SetNumberField(TEXT("commandCount"), Router.GetAllCommands().Num());

	// Get current level name - already on GameThread, access directly
	FString LevelName = TEXT("(no level loaded)");
	if (GEditor && GEditor->GetEditorWorldContext().World())
	{
		LevelName = GEditor->GetEditorWorldContext().World()->GetMapName();
	}

	PingData->SetStringField(TEXT("currentLevel"), LevelName);

	FString JsonString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonString);
	FJsonSerializer::Serialize(PingData.ToSharedRef(), Writer);

	OnComplete(CreateJsonResponse(JsonString));
	return true;
}

// ============================================
// GET /api/commands
// ============================================
bool FHttpCommandServer::HandleCommands(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete)
{
	if (Request.Verb == EHttpServerRequestVerbs::VERB_OPTIONS)
	{
		OnComplete(CreateCorsPreflightResponse());
		return true;
	}

	TSharedPtr<FJsonObject> CommandsInfo = Router.GetCommandsInfo();

	FString JsonString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonString);
	FJsonSerializer::Serialize(CommandsInfo.ToSharedRef(), Writer);

	OnComplete(CreateJsonResponse(JsonString));
	return true;
}

// ============================================
// Response helpers
// ============================================
TUniquePtr<FHttpServerResponse> FHttpCommandServer::CreateJsonResponse(const FString& JsonBody, int32 StatusCode)
{
	auto Response = FHttpServerResponse::Create(JsonBody, TEXT("application/json"));

	// CORS headers
	Response->Headers.Add(TEXT("Access-Control-Allow-Origin"), { TEXT("*") });
	Response->Headers.Add(TEXT("Access-Control-Allow-Methods"), { TEXT("GET, POST, OPTIONS") });
	Response->Headers.Add(TEXT("Access-Control-Allow-Headers"), { TEXT("Content-Type, Accept") });

	return Response;
}

TUniquePtr<FHttpServerResponse> FHttpCommandServer::CreateCorsPreflightResponse()
{
	auto Response = FHttpServerResponse::Create(FString(TEXT("")), FString(TEXT("text/plain")));

	Response->Headers.Add(TEXT("Access-Control-Allow-Origin"), { TEXT("*") });
	Response->Headers.Add(TEXT("Access-Control-Allow-Methods"), { TEXT("GET, POST, OPTIONS") });
	Response->Headers.Add(TEXT("Access-Control-Allow-Headers"), { TEXT("Content-Type, Accept") });
	Response->Headers.Add(TEXT("Access-Control-Max-Age"), { TEXT("86400") });

	return Response;
}
