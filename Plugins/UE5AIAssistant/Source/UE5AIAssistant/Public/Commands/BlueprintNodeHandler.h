// Copyright AI Assistant. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ICommandHandler.h"

class UBlueprint;
class UEdGraph;
class UEdGraphNode;
class UEdGraphPin;
class UK2Node_CallFunction;

/**
 * Handles Blueprint node/graph editing and discovery commands.
 *
 * Operation commands:
 *   add_node             - Add any node to a Blueprint graph (K2Node class name or alias)
 *   connect_nodes        - Connect two node pins
 *   remove_node          - Remove a node from graph
 *   get_node_pins        - List all pins on a node
 *   set_pin_default      - Set the default value of a pin
 *   batch_execute        - Execute multiple operations in one call
 *   auto_layout_graph    - Arrange nodes for readability
 *   add_pin              - Dynamically add output pins (Sequence, SwitchEnum, MakeArray, etc.)
 *
 * Discovery commands (query → use, zero hardcoding):
 *   list_node_types      - List all available K2Node types (for add_node)
 *   list_blueprint_classes - List all blueprint function library classes (for CallFunction)
 *   list_functions        - List all callable functions in a class (with real names + params)
 */
class FBlueprintNodeHandler : public ICommandHandler
{
public:
	virtual TArray<FString> GetSupportedCommands() const override;
	virtual FCommandResult Execute(const FString& Command, const TSharedPtr<FJsonObject>& Args) override;

private:
	// --- Operation command handlers ---
	FCommandResult HandleAddNode(const TSharedPtr<FJsonObject>& Args);
	FCommandResult HandleConnectNodes(const TSharedPtr<FJsonObject>& Args);
	FCommandResult HandleDisconnectNodes(const TSharedPtr<FJsonObject>& Args);
	FCommandResult HandleRemoveNode(const TSharedPtr<FJsonObject>& Args);
	FCommandResult HandleGetNodePins(const TSharedPtr<FJsonObject>& Args);
	FCommandResult HandleSetPinDefault(const TSharedPtr<FJsonObject>& Args);
	FCommandResult HandleBatchExecute(const TSharedPtr<FJsonObject>& Args);
	FCommandResult HandleAutoLayoutGraph(const TSharedPtr<FJsonObject>& Args);
	FCommandResult HandleAddPin(const TSharedPtr<FJsonObject>& Args);

	// --- Discovery command handlers ---
	FCommandResult HandleListNodeTypes(const TSharedPtr<FJsonObject>& Args);
	FCommandResult HandleListBlueprintClasses(const TSharedPtr<FJsonObject>& Args);
	FCommandResult HandleListFunctions(const TSharedPtr<FJsonObject>& Args);

	// --- Function resolution (clean, exact match only) ---
	/** Exact-match search across known library classes and Blueprint parent chain */
	UFunction* ResolveFunction(UBlueprint* Blueprint, const FString& FuncName) const;

	/** Build the list of searchable UClass pointers (libraries + blueprint parent chain) */
	TArray<UClass*> GetSearchClasses(UBlueprint* Blueprint) const;

	/** Create a CallFunction node with proper function reference resolution */
	UK2Node_CallFunction* CreateCallFunctionNode(UEdGraph* Graph, UBlueprint* Blueprint, const FString& FuncName, int32 PosX, int32 PosY) const;

	// --- Pin helpers ---
	UEdGraphPin* FindPinByName(UEdGraphNode* Node, const FString& PinName) const;
	FString BuildPinListError(UEdGraphNode* Node, const FString& RequestedPin, const FString& Context) const;

	// --- Graph layout ---
	int32 AutoLayoutGraph(UEdGraph* Graph) const;

	// --- Lookup helpers ---
	UBlueprint* FindBlueprintByName(const FString& Name) const;
	UEdGraph* FindGraphInBlueprint(UBlueprint* Blueprint, const FString& GraphName) const;
	UEdGraphNode* FindNodeByGuid(UEdGraph* Graph, const FString& GuidString) const;

	// --- Serialization ---
	TSharedPtr<FJsonObject> NodeToJson(UEdGraphNode* Node) const;
	TSharedPtr<FJsonObject> PinToJson(UEdGraphPin* Pin) const;
};
