// Copyright AI Assistant. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ICommandHandler.h"

/**
 * Handles Asset browsing/management commands.
 *
 * Commands:
 *   search_assets       - Search assets by name or path pattern
 *   get_assets_by_class - Get all assets of a specific class
 *   get_asset_details   - Get detailed info about a specific asset
 */
class FAssetCommandHandler : public ICommandHandler
{
public:
	virtual TArray<FString> GetSupportedCommands() const override;
	virtual FCommandResult Execute(const FString& Command, const TSharedPtr<FJsonObject>& Args) override;

private:
	FCommandResult HandleSearchAssets(const TSharedPtr<FJsonObject>& Args);
	FCommandResult HandleGetAssetsByClass(const TSharedPtr<FJsonObject>& Args);
	FCommandResult HandleGetAssetDetails(const TSharedPtr<FJsonObject>& Args);
};
