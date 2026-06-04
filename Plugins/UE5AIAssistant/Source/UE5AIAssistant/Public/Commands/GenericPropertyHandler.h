// Copyright AI Assistant. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ICommandHandler.h"

class UBlueprint;
class UEdGraph;
class UEdGraphNode;

/**
 * Generic property access via UE5 reflection system.
 * Replaces all domain-specific get/set handlers with universal commands.
 *
 * Commands:
 *   get_component_property  - Read any property from any component on a Blueprint CDO
 *   set_component_property  - Write any property on any component on a Blueprint CDO
 *   list_components         - List all components and their class on a Blueprint CDO
 *   list_properties         - List all editable properties on a UObject/component
 *   create_asset            - Create any asset by class name
 *   get_asset_property      - Read any property from an asset via reflection
 *   set_asset_property      - Write any property on an asset via reflection
 *   call_function           - Invoke any BlueprintCallable UFunction on any UObject via reflection
 *   get_object              - Get any UObject by path, with optional sub-object traversal
 *   modify_array_property   - Add/remove/get items in TArray properties
 *   execute_python          - Execute editor Python script (ultimate escape hatch)
 *
 * Universal addressing: All commands that resolve a target UObject support three paths:
 *   1. "object_path"  — direct UObject path (e.g. /Game/BP.BP)
 *   2. "asset_name"   — asset registry lookup by name
 *   3. "node_id" + "blueprint_name" [+ "graph_name"] — Blueprint graph node by GUID
 */
class FGenericPropertyHandler : public ICommandHandler
{
public:
	virtual TArray<FString> GetSupportedCommands() const override;
	virtual FCommandResult Execute(const FString& Command, const TSharedPtr<FJsonObject>& Args) override;

private:
	FCommandResult HandleGetComponentProperty(const TSharedPtr<FJsonObject>& Args);
	FCommandResult HandleSetComponentProperty(const TSharedPtr<FJsonObject>& Args);
	FCommandResult HandleListComponents(const TSharedPtr<FJsonObject>& Args);
	FCommandResult HandleListProperties(const TSharedPtr<FJsonObject>& Args);
	FCommandResult HandleCreateAsset(const TSharedPtr<FJsonObject>& Args);
	FCommandResult HandleGetAssetProperty(const TSharedPtr<FJsonObject>& Args);
	FCommandResult HandleSetAssetProperty(const TSharedPtr<FJsonObject>& Args);
	FCommandResult HandleCallFunction(const TSharedPtr<FJsonObject>& Args);
	FCommandResult HandleGetObject(const TSharedPtr<FJsonObject>& Args);
	FCommandResult HandleModifyArrayProperty(const TSharedPtr<FJsonObject>& Args);
	FCommandResult HandleExecutePython(const TSharedPtr<FJsonObject>& Args);

	// ---- Universal object resolution ----

	/**
	 * Resolve a target UObject from command arguments.
	 * Supports three addressing modes:
	 *   1. object_path  — StaticFindObject / LoadObject
	 *   2. asset_name   — FindAssetByName
	 *   3. node_id + blueprint_name [+ graph_name] — Blueprint graph node by GUID
	 * @return The resolved UObject, or nullptr (with OutError set)
	 */
	UObject* ResolveTargetObject(const TSharedPtr<FJsonObject>& Args, FString& OutError) const;

	// ---- Blueprint graph node helpers ----

	/** Find a graph in a Blueprint by name (searches UbergraphPages + FunctionGraphs) */
	UEdGraph* FindGraphInBlueprint(UBlueprint* Blueprint, const FString& GraphName) const;

	/** Find a node in a graph by its GUID string */
	UEdGraphNode* FindNodeByGuid(UEdGraph* Graph, const FString& GuidString) const;

	// ---- Existing helpers ----

	/** Find a Blueprint by name in asset registry */
	UBlueprint* FindBlueprintByName(const FString& Name) const;

	/** Find any UObject asset by name (searches all classes) */
	UObject* FindAssetByName(const FString& Name) const;

	/** Find a component on a Blueprint CDO by name */
	UActorComponent* FindComponentOnCDO(UBlueprint* Blueprint, const FString& ComponentName) const;

	/** Read a single property value as string via reflection */
	bool ReadPropertyValue(UObject* Object, const FString& PropertyPath, FString& OutValue, FString& OutType) const;

	/** Write a single property value from string via reflection */
	bool WritePropertyValue(UObject* Object, const FString& PropertyPath, const FString& Value, FString& OutError) const;

	/** Serialize all editable properties of a UObject to JSON */
	TSharedPtr<FJsonObject> PropertiesToJson(UObject* Object, bool bEditableOnly = true) const;

	/** Helper: escape a Python script string for safe embedding in a compile("""...""") call */
	FString QuoteForPython(const FString& Script) const;
};
