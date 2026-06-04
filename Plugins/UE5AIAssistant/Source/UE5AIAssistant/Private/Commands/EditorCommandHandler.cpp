// Copyright AI Assistant. All Rights Reserved.

#include "Commands/EditorCommandHandler.h"
#include "Editor.h"
#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"
#include "EngineUtils.h"
#include "LevelEditor.h"
#include "LevelEditorViewport.h"
#include "FileHelpers.h"
#include "Misc/App.h"
#include "GameMapsSettings.h"
#include "GameFramework/GameModeBase.h"
#include "UObject/UObjectIterator.h"
#include "AssetRegistry/AssetRegistryModule.h"

TArray<FString> FEditorCommandHandler::GetSupportedCommands() const
{
	return {
		TEXT("focus_viewport"),
		TEXT("get_current_level_info"),
		TEXT("save_all"),
		TEXT("get_project_settings"),
		TEXT("set_project_setting"),
		TEXT("get_world_settings"),
		TEXT("set_world_setting"),
	};
}

FCommandResult FEditorCommandHandler::Execute(const FString& Command, const TSharedPtr<FJsonObject>& Args)
{
	if (Command == TEXT("focus_viewport"))         return HandleFocusViewport(Args);
	if (Command == TEXT("get_current_level_info")) return HandleGetCurrentLevelInfo(Args);
	if (Command == TEXT("save_all"))               return HandleSaveAll(Args);
	if (Command == TEXT("get_project_settings"))   return HandleGetProjectSettings(Args);
	if (Command == TEXT("set_project_setting"))    return HandleSetProjectSetting(Args);
	if (Command == TEXT("get_world_settings"))     return HandleGetWorldSettings(Args);
	if (Command == TEXT("set_world_setting"))      return HandleSetWorldSetting(Args);

	return FCommandResult::Fail(FString::Printf(TEXT("EditorHandler: Unknown command '%s'"), *Command));
}

// ============================================
// focus_viewport
// Args: { "actor_name": "MyCube" } or { "location": [x,y,z] }
// ============================================
FCommandResult FEditorCommandHandler::HandleFocusViewport(const TSharedPtr<FJsonObject>& Args)
{
	if (!GEditor)
	{
		return FCommandResult::Fail(TEXT("GEditor not available"));
	}

	FString ActorName;
	if (Args->TryGetStringField(TEXT("actor_name"), ActorName))
	{
		// Find and focus on actor
		UWorld* World = GEditor->GetEditorWorldContext().World();
		if (!World)
		{
			return FCommandResult::Fail(TEXT("No editor world available"));
		}

		AActor* TargetActor = nullptr;
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			if ((*It)->GetActorLabel().Equals(ActorName, ESearchCase::IgnoreCase) ||
				(*It)->GetName().Equals(ActorName, ESearchCase::IgnoreCase))
			{
				TargetActor = *It;
				break;
			}
		}

		if (!TargetActor)
		{
			return FCommandResult::Fail(FString::Printf(TEXT("Actor '%s' not found"), *ActorName));
		}

		// Select and focus
		GEditor->SelectNone(true, true, false);
		GEditor->SelectActor(TargetActor, true, true, true);
		GEditor->MoveViewportCamerasToActor(*TargetActor, false);

		return FCommandResult::SuccessMessage(FString::Printf(TEXT("Focused viewport on '%s'"), *ActorName));
	}

	const TArray<TSharedPtr<FJsonValue>>* LocArray;
	if (Args->TryGetArrayField(TEXT("location"), LocArray) && LocArray->Num() >= 3)
	{
		FVector Location(
			(*LocArray)[0]->AsNumber(),
			(*LocArray)[1]->AsNumber(),
			(*LocArray)[2]->AsNumber()
		);

		// Move active viewport to location
		for (FLevelEditorViewportClient* ViewportClient : GEditor->GetLevelViewportClients())
		{
			if (ViewportClient)
			{
				ViewportClient->SetViewLocation(Location);
				break;
			}
		}

		return FCommandResult::SuccessMessage(FString::Printf(TEXT("Focused viewport on location [%.1f, %.1f, %.1f]"),
			Location.X, Location.Y, Location.Z));
	}

	return FCommandResult::Fail(TEXT("Provide 'actor_name' or 'location' argument"));
}

// ============================================
// get_current_level_info
// Args: (none)
// ============================================
FCommandResult FEditorCommandHandler::HandleGetCurrentLevelInfo(const TSharedPtr<FJsonObject>& Args)
{
	if (!GEditor)
	{
		return FCommandResult::Fail(TEXT("GEditor not available"));
	}

	UWorld* World = GEditor->GetEditorWorldContext().World();
	if (!World)
	{
		return FCommandResult::Fail(TEXT("No editor world available"));
	}

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("levelName"), World->GetMapName());
	Data->SetStringField(TEXT("worldName"), World->GetName());
	Data->SetStringField(TEXT("engineVersion"), FApp::GetBuildVersion());

	// Count actors
	int32 ActorCount = 0;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		ActorCount++;
	}
	Data->SetNumberField(TEXT("actorCount"), ActorCount);

	// List sub-levels
	TArray<TSharedPtr<FJsonValue>> LevelArray;
	for (ULevel* Level : World->GetLevels())
	{
		if (Level)
		{
			TSharedPtr<FJsonObject> LevelObj = MakeShared<FJsonObject>();
			LevelObj->SetStringField(TEXT("name"), Level->GetOuter()->GetName());
			LevelObj->SetNumberField(TEXT("actorCount"), Level->Actors.Num());
			LevelObj->SetBoolField(TEXT("isPersistent"), Level == World->PersistentLevel);
			LevelArray.Add(MakeShared<FJsonValueObject>(LevelObj));
		}
	}
	Data->SetArrayField(TEXT("levels"), LevelArray);

	return FCommandResult::Success(Data);
}

// ============================================
// save_all
// Args: (none)
// ============================================
FCommandResult FEditorCommandHandler::HandleSaveAll(const TSharedPtr<FJsonObject>& Args)
{
	bool bSaved = FEditorFileUtils::SaveDirtyPackages(
		/*bPromptUserToSave=*/ false,
		/*bSaveMapPackages=*/ true,
		/*bSaveContentPackages=*/ true
	);

	if (bSaved)
	{
		return FCommandResult::SuccessMessage(TEXT("All dirty packages saved successfully"));
	}
	else
	{
		return FCommandResult::SuccessMessage(TEXT("Save completed (some packages may have been skipped)"));
	}
}

// ============================================
// get_project_settings
// Args: { "category": "" (optional, e.g. "Maps", "GameMode") }
// ============================================
FCommandResult FEditorCommandHandler::HandleGetProjectSettings(const TSharedPtr<FJsonObject>& Args)
{
	UGameMapsSettings* MapSettings = UGameMapsSettings::GetGameMapsSettings();
	if (!MapSettings)
	{
		return FCommandResult::Fail(TEXT("Could not access GameMapsSettings"));
	}

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("settingsClass"), MapSettings->GetClass()->GetName());

	// Use reflection to list all editable properties (safe — avoids private member access)
	FString Filter;
	Args->TryGetStringField(TEXT("filter"), Filter);

	TArray<TSharedPtr<FJsonValue>> PropsArray;
	for (TFieldIterator<FProperty> PropIt(MapSettings->GetClass()); PropIt; ++PropIt)
	{
		FProperty* Prop = *PropIt;
		if (!Prop->HasAnyPropertyFlags(CPF_Edit)) continue;

		FString PropName = Prop->GetName();
		if (!Filter.IsEmpty() && !PropName.Contains(Filter, ESearchCase::IgnoreCase)) continue;

		FString ValueStr;
		Prop->ExportText_Direct(ValueStr, Prop->ContainerPtrToValuePtr<void>(MapSettings), nullptr, MapSettings, PPF_None);

		TSharedPtr<FJsonObject> PropObj = MakeShared<FJsonObject>();
		PropObj->SetStringField(TEXT("name"), PropName);
		PropObj->SetStringField(TEXT("type"), Prop->GetCPPType());
		PropObj->SetStringField(TEXT("value"), ValueStr);
		PropsArray.Add(MakeShared<FJsonValueObject>(PropObj));
	}
	Data->SetArrayField(TEXT("allProperties"), PropsArray);

	return FCommandResult::Success(Data);
}

// ============================================
// set_project_setting
// Args: { "property_name": "GlobalDefaultGameMode", "value": "/Game/Blueprints/BP_MyGameMode.BP_MyGameMode_C" }
// ============================================
FCommandResult FEditorCommandHandler::HandleSetProjectSetting(const TSharedPtr<FJsonObject>& Args)
{
	FString PropName, Value;
	if (!Args->TryGetStringField(TEXT("property_name"), PropName))
	{
		return FCommandResult::Fail(TEXT("Missing required 'property_name' argument"));
	}
	if (!Args->TryGetStringField(TEXT("value"), Value))
	{
		return FCommandResult::Fail(TEXT("Missing required 'value' argument"));
	}

	UGameMapsSettings* MapSettings = UGameMapsSettings::GetGameMapsSettings();
	if (!MapSettings)
	{
		return FCommandResult::Fail(TEXT("Could not access GameMapsSettings"));
	}

	// Find the property by name using reflection
	FProperty* FoundProp = nullptr;
	for (TFieldIterator<FProperty> PropIt(MapSettings->GetClass()); PropIt; ++PropIt)
	{
		if (PropIt->GetName().Equals(PropName, ESearchCase::IgnoreCase))
		{
			FoundProp = *PropIt;
			break;
		}
	}

	if (!FoundProp)
	{
		return FCommandResult::Fail(FString::Printf(TEXT("Property '%s' not found in GameMapsSettings. Use get_project_settings to list available properties."), *PropName));
	}

	void* ValuePtr = FoundProp->ContainerPtrToValuePtr<void>(MapSettings);
	bool bImported = FoundProp->ImportText_Direct(*Value, ValuePtr, MapSettings, PPF_None) != nullptr;
	if (!bImported)
	{
		return FCommandResult::Fail(FString::Printf(TEXT("Failed to set '%s' = '%s' (type mismatch?)"), *PropName, *Value));
	}

	MapSettings->SaveConfig();

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("property"), PropName);
	Data->SetStringField(TEXT("value"), Value);
	Data->SetStringField(TEXT("message"), FString::Printf(TEXT("Set project setting '%s' = '%s'"), *PropName, *Value));
	return FCommandResult::Success(Data);
}

// ============================================
// get_world_settings
// Args: { "filter": "" (optional) }
// ============================================
FCommandResult FEditorCommandHandler::HandleGetWorldSettings(const TSharedPtr<FJsonObject>& Args)
{
	if (!GEditor)
	{
		return FCommandResult::Fail(TEXT("GEditor not available"));
	}

	UWorld* World = GEditor->GetEditorWorldContext().World();
	if (!World)
	{
		return FCommandResult::Fail(TEXT("No editor world available"));
	}

	AWorldSettings* WorldSettings = World->GetWorldSettings();
	if (!WorldSettings)
	{
		return FCommandResult::Fail(TEXT("No world settings found"));
	}

	FString Filter;
	Args->TryGetStringField(TEXT("filter"), Filter);

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("levelName"), World->GetMapName());
	Data->SetStringField(TEXT("settingsClass"), WorldSettings->GetClass()->GetName());

	// All editable properties via reflection (safe — avoids direct member access)
	TArray<TSharedPtr<FJsonValue>> PropsArray;
	for (TFieldIterator<FProperty> PropIt(WorldSettings->GetClass()); PropIt; ++PropIt)
	{
		FProperty* Prop = *PropIt;
		if (!Prop->HasAnyPropertyFlags(CPF_Edit)) continue;

		FString PropName = Prop->GetName();
		if (!Filter.IsEmpty() && !PropName.Contains(Filter, ESearchCase::IgnoreCase)) continue;

		FString ValueStr;
		Prop->ExportText_Direct(ValueStr, Prop->ContainerPtrToValuePtr<void>(WorldSettings), nullptr, WorldSettings, PPF_None);

		TSharedPtr<FJsonObject> PropObj = MakeShared<FJsonObject>();
		PropObj->SetStringField(TEXT("name"), PropName);
		PropObj->SetStringField(TEXT("type"), Prop->GetCPPType());
		PropObj->SetStringField(TEXT("value"), ValueStr);
		PropsArray.Add(MakeShared<FJsonValueObject>(PropObj));
	}
	Data->SetArrayField(TEXT("properties"), PropsArray);

	return FCommandResult::Success(Data);
}

// ============================================
// set_world_setting
// Args: { "property_name": "DefaultGameMode", "value": "/Game/BP_MyGameMode.BP_MyGameMode_C" }
// ============================================
FCommandResult FEditorCommandHandler::HandleSetWorldSetting(const TSharedPtr<FJsonObject>& Args)
{
	FString PropName, Value;
	if (!Args->TryGetStringField(TEXT("property_name"), PropName))
	{
		return FCommandResult::Fail(TEXT("Missing required 'property_name' argument"));
	}
	if (!Args->TryGetStringField(TEXT("value"), Value))
	{
		return FCommandResult::Fail(TEXT("Missing required 'value' argument"));
	}

	if (!GEditor)
	{
		return FCommandResult::Fail(TEXT("GEditor not available"));
	}

	UWorld* World = GEditor->GetEditorWorldContext().World();
	if (!World)
	{
		return FCommandResult::Fail(TEXT("No editor world available"));
	}

	AWorldSettings* WorldSettings = World->GetWorldSettings();
	if (!WorldSettings)
	{
		return FCommandResult::Fail(TEXT("No world settings found"));
	}

	// Find the property by name using reflection
	FProperty* FoundProp = nullptr;
	for (TFieldIterator<FProperty> PropIt(WorldSettings->GetClass()); PropIt; ++PropIt)
	{
		if (PropIt->GetName().Equals(PropName, ESearchCase::IgnoreCase))
		{
			FoundProp = *PropIt;
			break;
		}
	}

	if (!FoundProp)
	{
		return FCommandResult::Fail(FString::Printf(TEXT("Property '%s' not found in WorldSettings. Use get_world_settings to list available properties."), *PropName));
	}

	void* ValuePtr = FoundProp->ContainerPtrToValuePtr<void>(WorldSettings);
	bool bImported = FoundProp->ImportText_Direct(*Value, ValuePtr, WorldSettings, PPF_None) != nullptr;
	if (!bImported)
	{
		return FCommandResult::Fail(FString::Printf(TEXT("Failed to set '%s' = '%s' (type mismatch?)"), *PropName, *Value));
	}

	World->MarkPackageDirty();

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("property"), PropName);
	Data->SetStringField(TEXT("value"), Value);
	Data->SetStringField(TEXT("message"), FString::Printf(TEXT("Set world setting '%s' = '%s'"), *PropName, *Value));
	return FCommandResult::Success(Data);
}
