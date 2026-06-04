// Copyright AI Assistant. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ICommandHandler.h"

/**
 * Handles Editor-level commands.
 *
 * Commands:
 *   focus_viewport         - Focus the viewport on a specific actor
 *   get_current_level_info - Get info about the current level
 *   save_all                  - Save all dirty assets
 *   get_project_settings      - Read project settings (default map, game mode, etc.)
 *   set_project_setting       - Write a project setting
 *   get_world_settings        - Read world settings for current level
 *   set_world_setting         - Write a world setting
 */
class FEditorCommandHandler : public ICommandHandler
{
public:
	virtual TArray<FString> GetSupportedCommands() const override;
	virtual FCommandResult Execute(const FString& Command, const TSharedPtr<FJsonObject>& Args) override;

private:
	FCommandResult HandleFocusViewport(const TSharedPtr<FJsonObject>& Args);
	FCommandResult HandleGetCurrentLevelInfo(const TSharedPtr<FJsonObject>& Args);
	FCommandResult HandleSaveAll(const TSharedPtr<FJsonObject>& Args);
	FCommandResult HandleGetProjectSettings(const TSharedPtr<FJsonObject>& Args);
	FCommandResult HandleSetProjectSetting(const TSharedPtr<FJsonObject>& Args);
	FCommandResult HandleGetWorldSettings(const TSharedPtr<FJsonObject>& Args);
	FCommandResult HandleSetWorldSetting(const TSharedPtr<FJsonObject>& Args);
};
