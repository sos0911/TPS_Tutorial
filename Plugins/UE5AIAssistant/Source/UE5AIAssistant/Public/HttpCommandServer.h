// Copyright AI Assistant. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HttpServerModule.h"
#include "IHttpRouter.h"
#include "HttpRouteHandle.h"
#include "CommandRouter.h"

/**
 * Embedded HTTP server that receives commands from external AI tools.
 * 
 * Endpoints:
 *   GET  /api/ping      - Health check, returns engine info
 *   GET  /api/commands   - List all available commands
 *   POST /api/execute    - Execute a command: { "command": "xxx", "args": {...} }
 *   OPTIONS *            - CORS preflight
 */
class FHttpCommandServer
{
public:
	FHttpCommandServer();
	~FHttpCommandServer();

	/** Start the HTTP server on the given port */
	void Start(uint32 Port = 58080);

	/** Stop the HTTP server */
	void Stop();

	/** Access the command router to register handlers */
	FCommandRouter& GetRouter() { return Router; }

private:
	/** Handle POST /api/execute */
	bool HandleExecute(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete);

	/** Handle GET /api/ping */
	bool HandlePing(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete);

	/** Handle GET /api/commands */
	bool HandleCommands(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete);

	/** Add CORS headers to response */
	TUniquePtr<FHttpServerResponse> CreateJsonResponse(const FString& JsonBody, int32 StatusCode = 200);

	/** Create CORS preflight response */
	TUniquePtr<FHttpServerResponse> CreateCorsPreflightResponse();

	FCommandRouter Router;
	TArray<FHttpRouteHandle> RouteHandles;
	uint32 ServerPort = 0;
	bool bIsRunning = false;
};
