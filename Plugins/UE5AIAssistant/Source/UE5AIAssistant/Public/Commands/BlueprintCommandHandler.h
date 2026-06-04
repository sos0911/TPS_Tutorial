// Copyright AI Assistant. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ICommandHandler.h"

class UBlueprint;

/**
 * Handles Blueprint structure commands.
 *
 * Commands:
 *   create_blueprint           - Create a new Blueprint class
 *   compile_blueprint          - Compile a Blueprint
 *   read_blueprint_content     - Read full Blueprint structure (variables, functions, components, graphs)
 *   create_variable            - Add a member variable to a Blueprint
 *   add_component_to_blueprint - Add a component to a Blueprint's component tree
 *   reparent_component         - Change the parent of a component in the SCS hierarchy
 *   create_function            - Create a new function graph in a Blueprint
 *   add_function_parameter     - Add an input/output parameter to a Blueprint function
 *   create_event_dispatcher    - Add a multicast delegate (event dispatcher) to a Blueprint
 *   create_blueprint_interface - Create a new Blueprint Interface asset
 *   implement_interface        - Make a Blueprint implement a Blueprint Interface
 *   add_widget_child           - Add a child widget to a Widget Blueprint (UserWidget)
 */
class FBlueprintCommandHandler : public ICommandHandler
{
public:
	virtual TArray<FString> GetSupportedCommands() const override;
	virtual FCommandResult Execute(const FString& Command, const TSharedPtr<FJsonObject>& Args) override;

private:
	FCommandResult HandleCreateBlueprint(const TSharedPtr<FJsonObject>& Args);
	FCommandResult HandleCompileBlueprint(const TSharedPtr<FJsonObject>& Args);
	FCommandResult HandleReadBlueprintContent(const TSharedPtr<FJsonObject>& Args);
	FCommandResult HandleCreateVariable(const TSharedPtr<FJsonObject>& Args);
	FCommandResult HandleAddComponent(const TSharedPtr<FJsonObject>& Args);
	FCommandResult HandleReparentComponent(const TSharedPtr<FJsonObject>& Args);
	FCommandResult HandleCreateFunction(const TSharedPtr<FJsonObject>& Args);
	FCommandResult HandleAddFunctionParameter(const TSharedPtr<FJsonObject>& Args);
	FCommandResult HandleCreateEventDispatcher(const TSharedPtr<FJsonObject>& Args);
	FCommandResult HandleCreateBlueprintInterface(const TSharedPtr<FJsonObject>& Args);
	FCommandResult HandleImplementInterface(const TSharedPtr<FJsonObject>& Args);
	FCommandResult HandleAddWidgetChild(const TSharedPtr<FJsonObject>& Args);

	/** Find a Blueprint asset by name. Supports regular BPs and Level Blueprint ("LevelBlueprint" or "__LevelBlueprint__"). */
	UBlueprint* FindBlueprintByName(const FString& Name) const;

	/** Resolve a parent class name string to UClass* */
	UClass* ResolveParentClass(const FString& ClassName) const;

	/** Resolve a variable type string to FEdGraphPinType */
	bool ResolveVariableType(const FString& TypeName, struct FEdGraphPinType& OutPinType) const;
};
