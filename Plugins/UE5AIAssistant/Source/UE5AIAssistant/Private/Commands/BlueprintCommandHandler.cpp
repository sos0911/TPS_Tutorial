// Copyright AI Assistant. All Rights Reserved.

#include "Commands/BlueprintCommandHandler.h"
#include "Editor.h"
#include "Engine/Blueprint.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/SCS_Node.h"
#include "Engine/LevelScriptBlueprint.h"
#include "Engine/World.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_Event.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_FunctionResult.h"
#include "K2Node_CallFunction.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/AudioComponent.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/BoxComponent.h"
#include "Components/SphereComponent.h"
#include "Components/ArrowComponent.h"
#include "Particles/ParticleSystemComponent.h"
#include "UObject/UObjectIterator.h"
#include "Blueprint/WidgetTree.h"
#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "Blueprint/UserWidget.h"
#include "Components/CanvasPanel.h"
#include "Components/VerticalBox.h"
#include "Components/HorizontalBox.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "WidgetBlueprint.h"

TArray<FString> FBlueprintCommandHandler::GetSupportedCommands() const
{
	return {
		TEXT("create_blueprint"),
		TEXT("compile_blueprint"),
		TEXT("read_blueprint_content"),
		TEXT("create_variable"),
		TEXT("add_component_to_blueprint"),
		TEXT("reparent_component"),
		TEXT("create_function"),
		TEXT("add_function_parameter"),
		TEXT("create_event_dispatcher"),
		TEXT("create_blueprint_interface"),
		TEXT("implement_interface"),
		TEXT("add_widget_child"),
	};
}

FCommandResult FBlueprintCommandHandler::Execute(const FString& Command, const TSharedPtr<FJsonObject>& Args)
{
	if (Command == TEXT("create_blueprint"))           return HandleCreateBlueprint(Args);
	if (Command == TEXT("compile_blueprint"))           return HandleCompileBlueprint(Args);
	if (Command == TEXT("read_blueprint_content"))      return HandleReadBlueprintContent(Args);
	if (Command == TEXT("create_variable"))             return HandleCreateVariable(Args);
	if (Command == TEXT("add_component_to_blueprint"))  return HandleAddComponent(Args);
	if (Command == TEXT("reparent_component"))          return HandleReparentComponent(Args);
	if (Command == TEXT("create_function"))             return HandleCreateFunction(Args);
	if (Command == TEXT("add_function_parameter"))      return HandleAddFunctionParameter(Args);
	if (Command == TEXT("create_event_dispatcher"))    return HandleCreateEventDispatcher(Args);
	if (Command == TEXT("create_blueprint_interface")) return HandleCreateBlueprintInterface(Args);
	if (Command == TEXT("implement_interface"))         return HandleImplementInterface(Args);
	if (Command == TEXT("add_widget_child"))            return HandleAddWidgetChild(Args);

	return FCommandResult::Fail(FString::Printf(TEXT("BlueprintHandler: Unknown command '%s'"), *Command));
}

// ============================================
// create_blueprint
// Args: { "name": "BP_MyActor", "parent_class": "Character" }
// ============================================
FCommandResult FBlueprintCommandHandler::HandleCreateBlueprint(const TSharedPtr<FJsonObject>& Args)
{
	FString Name;
	if (!Args->TryGetStringField(TEXT("name"), Name))
	{
		return FCommandResult::Fail(TEXT("Missing required 'name' argument"));
	}

	FString ParentClassName;
	if (!Args->TryGetStringField(TEXT("parent_class"), ParentClassName))
	{
		ParentClassName = TEXT("Actor");
	}

	UClass* ParentClass = ResolveParentClass(ParentClassName);
	if (!ParentClass)
	{
		return FCommandResult::Fail(FString::Printf(TEXT("Parent class '%s' not found. Pass any UClass name (e.g. Actor, Character, Pawn, PlayerController, ActorComponent, UserWidget, etc.)"), *ParentClassName));
	}

	// Create the Blueprint asset
	FString PackagePath = TEXT("/Game/Blueprints");
	FString AssetName = Name;

	// Ensure the directory exists
	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();

	UPackage* Package = CreatePackage(*FString::Printf(TEXT("%s/%s"), *PackagePath, *AssetName));
	if (!Package)
	{
		return FCommandResult::Fail(TEXT("Failed to create package for Blueprint"));
	}

	UBlueprint* NewBlueprint = FKismetEditorUtilities::CreateBlueprint(
		ParentClass,
		Package,
		FName(*AssetName),
		BPTYPE_Normal,
		UBlueprint::StaticClass(),
		UBlueprintGeneratedClass::StaticClass()
	);

	if (!NewBlueprint)
	{
		return FCommandResult::Fail(TEXT("Failed to create Blueprint"));
	}

	// Mark dirty and save
	NewBlueprint->MarkPackageDirty();

	// Notify asset registry
	FAssetRegistryModule::AssetCreated(NewBlueprint);

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("name"), AssetName);
	Data->SetStringField(TEXT("parentClass"), ParentClassName);
	Data->SetStringField(TEXT("path"), NewBlueprint->GetPathName());
	Data->SetStringField(TEXT("message"), FString::Printf(TEXT("Successfully created Blueprint '%s' (parent: %s)"), *AssetName, *ParentClassName));

	return FCommandResult::Success(Data);
}

// ============================================
// compile_blueprint
// Args: { "blueprint_name": "BP_MyActor" }
// Returns: compilation result with detailed errors/warnings per node
// ============================================
FCommandResult FBlueprintCommandHandler::HandleCompileBlueprint(const TSharedPtr<FJsonObject>& Args)
{
	FString BPName;
	if (!Args->TryGetStringField(TEXT("blueprint_name"), BPName))
	{
		return FCommandResult::Fail(TEXT("Missing required 'blueprint_name' argument"));
	}

	UBlueprint* Blueprint = FindBlueprintByName(BPName);
	if (!Blueprint)
	{
		return FCommandResult::Fail(FString::Printf(TEXT("Blueprint '%s' not found"), *BPName));
	}

	// Pre-compile validation: remove orphaned nodes with 0 pins to prevent crashes
	TArray<FString> RemovedOrphans;

	auto CleanGraphForCompile = [&RemovedOrphans](UEdGraph* Graph, bool bIsFunctionGraph)
	{
		if (!Graph) return;
		TArray<UEdGraphNode*> NodesToRemove;
		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (!Node || Node->Pins.Num() > 0) continue;
			if (Node->IsA(UK2Node_Event::StaticClass())) continue;
			if (bIsFunctionGraph && (Node->IsA(UK2Node_FunctionEntry::StaticClass()) || Node->IsA(UK2Node_FunctionResult::StaticClass()))) continue;
			NodesToRemove.Add(Node);
		}
		for (UEdGraphNode* BadNode : NodesToRemove)
		{
			FString Info = FString::Printf(TEXT("%s (id=%s, graph=%s)"),
				*BadNode->GetNodeTitle(ENodeTitleType::ListView).ToString(),
				*BadNode->NodeGuid.ToString(),
				*Graph->GetName());
			RemovedOrphans.Add(Info);
			UE_LOG(LogTemp, Warning, TEXT("AIAssistant: Removed 0-pin orphaned node '%s' before compile"), *Info);
			Graph->RemoveNode(BadNode);
		}
	};

	for (UEdGraph* Graph : Blueprint->UbergraphPages) CleanGraphForCompile(Graph, false);
	for (UEdGraph* Graph : Blueprint->FunctionGraphs) CleanGraphForCompile(Graph, true);

	FKismetEditorUtilities::CompileBlueprint(Blueprint);

	bool bHasErrors = Blueprint->Status == BS_Error;
	bool bHasWarnings = Blueprint->Status == BS_UpToDateWithWarnings;

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("name"), BPName);
	Data->SetBoolField(TEXT("compiled"), !bHasErrors);
	Data->SetBoolField(TEXT("hasErrors"), bHasErrors);
	Data->SetBoolField(TEXT("hasWarnings"), bHasWarnings);

	// Report removed orphan nodes so the caller knows what was cleaned up
	if (RemovedOrphans.Num() > 0)
	{
		TArray<TSharedPtr<FJsonValue>> OrphanArr;
		for (const FString& S : RemovedOrphans)
			OrphanArr.Add(MakeShared<FJsonValueString>(S));
		Data->SetArrayField(TEXT("removedOrphanNodes"), OrphanArr);
		Data->SetNumberField(TEXT("removedOrphanCount"), RemovedOrphans.Num());
	}

	// Collect detailed errors/warnings from all graph nodes
	TArray<TSharedPtr<FJsonValue>> DiagArray;
	int32 ErrorCount = 0;
	int32 WarningCount = 0;

	// Helper lambda to scan a graph for nodes with compiler messages
	auto ScanGraphForMessages = [&](UEdGraph* Graph, const FString& GraphName)
	{
		if (!Graph) return;

		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (!Node || !Node->bHasCompilerMessage) continue;

			TSharedPtr<FJsonObject> DiagObj = MakeShared<FJsonObject>();
			DiagObj->SetStringField(TEXT("graph"), GraphName);
			DiagObj->SetStringField(TEXT("nodeId"), Node->NodeGuid.ToString());
			DiagObj->SetStringField(TEXT("nodeTitle"), Node->GetNodeTitle(ENodeTitleType::ListView).ToString());
			DiagObj->SetStringField(TEXT("nodeType"), Node->GetClass()->GetName());
			DiagObj->SetNumberField(TEXT("posX"), Node->NodePosX);
			DiagObj->SetNumberField(TEXT("posY"), Node->NodePosY);

			bool bIsError = (Node->ErrorType <= EMessageSeverity::Error);
			DiagObj->SetStringField(TEXT("severity"), bIsError ? TEXT("Error") : TEXT("Warning"));
			DiagObj->SetStringField(TEXT("message"), Node->ErrorMsg);

			if (bIsError) ErrorCount++;
			else WarningCount++;

			DiagArray.Add(MakeShared<FJsonValueObject>(DiagObj));
		}
	};

	// Scan all graph types
	for (UEdGraph* Graph : Blueprint->UbergraphPages)
	{
		ScanGraphForMessages(Graph, Graph ? Graph->GetName() : TEXT("Unknown"));
	}
	for (UEdGraph* Graph : Blueprint->FunctionGraphs)
	{
		ScanGraphForMessages(Graph, Graph ? Graph->GetName() : TEXT("Unknown"));
	}

	Data->SetArrayField(TEXT("diagnostics"), DiagArray);
	Data->SetNumberField(TEXT("errorCount"), ErrorCount);
	Data->SetNumberField(TEXT("warningCount"), WarningCount);

	// Summary message
	if (bHasErrors)
	{
		Data->SetStringField(TEXT("message"),
			FString::Printf(TEXT("Blueprint '%s' compiled with %d error(s), %d warning(s)"), *BPName, ErrorCount, WarningCount));
	}
	else if (bHasWarnings)
	{
		Data->SetStringField(TEXT("message"),
			FString::Printf(TEXT("Blueprint '%s' compiled with %d warning(s)"), *BPName, WarningCount));
	}
	else
	{
		Data->SetStringField(TEXT("message"),
			FString::Printf(TEXT("Blueprint '%s' compiled successfully"), *BPName));
	}

	return FCommandResult::Success(Data);
}

// ============================================
// read_blueprint_content
// Args: { "blueprint_name": "BP_MyActor", "include_pins": true (default), "graph_name": "" (optional, filter specific graph) }
// Returns full graph structure with nodes, pins, connections, and default values
// ============================================
FCommandResult FBlueprintCommandHandler::HandleReadBlueprintContent(const TSharedPtr<FJsonObject>& Args)
{
	FString BPName;
	if (!Args->TryGetStringField(TEXT("blueprint_name"), BPName))
	{
		return FCommandResult::Fail(TEXT("Missing required 'blueprint_name' argument"));
	}

	UBlueprint* Blueprint = FindBlueprintByName(BPName);
	if (!Blueprint)
	{
		return FCommandResult::Fail(FString::Printf(TEXT("Blueprint '%s' not found"), *BPName));
	}

	// Options
	bool bIncludePins = true;
	Args->TryGetBoolField(TEXT("include_pins"), bIncludePins);

	FString FilterGraph;
	Args->TryGetStringField(TEXT("graph_name"), FilterGraph);

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("name"), BPName);
	Data->SetStringField(TEXT("parentClass"), Blueprint->ParentClass ? Blueprint->ParentClass->GetName() : TEXT("None"));
	Data->SetStringField(TEXT("path"), Blueprint->GetPathName());

	// Blueprint status
	FString StatusStr;
	switch (Blueprint->Status)
	{
		case BS_Error:             StatusStr = TEXT("Error"); break;
		case BS_UpToDate:          StatusStr = TEXT("UpToDate"); break;
		case BS_Dirty:             StatusStr = TEXT("Dirty"); break;
		case BS_UpToDateWithWarnings: StatusStr = TEXT("UpToDateWithWarnings"); break;
		default:                   StatusStr = TEXT("Unknown"); break;
	}
	Data->SetStringField(TEXT("status"), StatusStr);

	// Variables
	TArray<TSharedPtr<FJsonValue>> VarArray;
	for (const FBPVariableDescription& Var : Blueprint->NewVariables)
	{
		TSharedPtr<FJsonObject> VarObj = MakeShared<FJsonObject>();
		VarObj->SetStringField(TEXT("name"), Var.VarName.ToString());
		VarObj->SetStringField(TEXT("type"), Var.VarType.PinCategory.ToString());

		if (Var.VarType.PinSubCategoryObject.Get() != nullptr)
		{
			VarObj->SetStringField(TEXT("subtype"), Var.VarType.PinSubCategoryObject->GetName());
		}

		VarObj->SetBoolField(TEXT("isArray"), Var.VarType.ContainerType == EPinContainerType::Array);

		if (!Var.DefaultValue.IsEmpty())
		{
			VarObj->SetStringField(TEXT("defaultValue"), Var.DefaultValue);
		}

		VarArray.Add(MakeShared<FJsonValueObject>(VarObj));
	}
	Data->SetArrayField(TEXT("variables"), VarArray);

	// Helper lambda: serialize a single node with full pin + connection info
	auto NodeToFullJson = [bIncludePins](UEdGraphNode* Node) -> TSharedPtr<FJsonObject>
	{
		TSharedPtr<FJsonObject> NodeObj = MakeShared<FJsonObject>();
		NodeObj->SetStringField(TEXT("id"), Node->NodeGuid.ToString());
		NodeObj->SetStringField(TEXT("type"), Node->GetClass()->GetName());
		NodeObj->SetStringField(TEXT("title"), Node->GetNodeTitle(ENodeTitleType::ListView).ToString());
		NodeObj->SetNumberField(TEXT("posX"), Node->NodePosX);
		NodeObj->SetNumberField(TEXT("posY"), Node->NodePosY);

		// Node comment (useful for understanding intent)
		if (!Node->NodeComment.IsEmpty())
		{
			NodeObj->SetStringField(TEXT("comment"), Node->NodeComment);
		}

		// Error/warning status
		if (Node->bHasCompilerMessage)
		{
			NodeObj->SetBoolField(TEXT("hasError"), Node->ErrorType <= EMessageSeverity::Error);
			NodeObj->SetStringField(TEXT("errorMsg"), Node->ErrorMsg);
		}

		if (bIncludePins)
		{
			TArray<TSharedPtr<FJsonValue>> PinArray;
			for (UEdGraphPin* Pin : Node->Pins)
			{
				if (!Pin) continue;

				TSharedPtr<FJsonObject> PinObj = MakeShared<FJsonObject>();
				PinObj->SetStringField(TEXT("name"), Pin->PinName.ToString());
				PinObj->SetStringField(TEXT("direction"), Pin->Direction == EGPD_Input ? TEXT("in") : TEXT("out"));
				PinObj->SetStringField(TEXT("type"), Pin->PinType.PinCategory.ToString());

				// Sub-type info (e.g. object class, struct name)
				if (Pin->PinType.PinSubCategoryObject.Get() != nullptr)
				{
					PinObj->SetStringField(TEXT("subtype"), Pin->PinType.PinSubCategoryObject->GetName());
				}

				// Default value
				if (!Pin->DefaultValue.IsEmpty())
				{
					PinObj->SetStringField(TEXT("default"), Pin->DefaultValue);
				}
				if (!Pin->DefaultTextValue.IsEmpty())
				{
					PinObj->SetStringField(TEXT("defaultText"), Pin->DefaultTextValue.ToString());
				}
				if (Pin->DefaultObject != nullptr)
				{
					PinObj->SetStringField(TEXT("defaultObject"), Pin->DefaultObject->GetName());
				}

				// Connections — the critical data for understanding graph flow
				if (Pin->LinkedTo.Num() > 0)
				{
					TArray<TSharedPtr<FJsonValue>> ConnArray;
					for (UEdGraphPin* Linked : Pin->LinkedTo)
					{
						if (!Linked || !Linked->GetOwningNode()) continue;

						TSharedPtr<FJsonObject> ConnObj = MakeShared<FJsonObject>();
						ConnObj->SetStringField(TEXT("nodeId"), Linked->GetOwningNode()->NodeGuid.ToString());
						ConnObj->SetStringField(TEXT("pin"), Linked->PinName.ToString());
						ConnArray.Add(MakeShared<FJsonValueObject>(ConnObj));
					}
					PinObj->SetArrayField(TEXT("connections"), ConnArray);
				}

				PinArray.Add(MakeShared<FJsonValueObject>(PinObj));
			}
			NodeObj->SetArrayField(TEXT("pins"), PinArray);
		}

		return NodeObj;
	};

	// Helper lambda: serialize a graph (nodes + full pin data)
	auto GraphToJson = [&NodeToFullJson](UEdGraph* Graph) -> TSharedPtr<FJsonObject>
	{
		TSharedPtr<FJsonObject> GraphObj = MakeShared<FJsonObject>();
		GraphObj->SetStringField(TEXT("name"), Graph->GetName());
		GraphObj->SetNumberField(TEXT("nodeCount"), Graph->Nodes.Num());

		TArray<TSharedPtr<FJsonValue>> NodeArray;
		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (!Node) continue;
			NodeArray.Add(MakeShared<FJsonValueObject>(NodeToFullJson(Node)));
		}
		GraphObj->SetArrayField(TEXT("nodes"), NodeArray);

		return GraphObj;
	};

	// Functions — now with full node details
	TArray<TSharedPtr<FJsonValue>> FuncArray;
	for (UEdGraph* Graph : Blueprint->FunctionGraphs)
	{
		if (!Graph) continue;
		if (!FilterGraph.IsEmpty() && !Graph->GetName().Equals(FilterGraph, ESearchCase::IgnoreCase)) continue;

		FuncArray.Add(MakeShared<FJsonValueObject>(GraphToJson(Graph)));
	}
	Data->SetArrayField(TEXT("functions"), FuncArray);

	// Components (from SCS)
	TArray<TSharedPtr<FJsonValue>> CompArray;
	if (Blueprint->SimpleConstructionScript)
	{
		for (USCS_Node* SCSNode : Blueprint->SimpleConstructionScript->GetAllNodes())
		{
			if (!SCSNode || !SCSNode->ComponentTemplate) continue;

			TSharedPtr<FJsonObject> CompObj = MakeShared<FJsonObject>();
			CompObj->SetStringField(TEXT("name"), SCSNode->GetVariableName().ToString());
			CompObj->SetStringField(TEXT("type"), SCSNode->ComponentTemplate->GetClass()->GetName());

			// Parent component
			if (SCSNode->ParentComponentOrVariableName != NAME_None)
			{
				CompObj->SetStringField(TEXT("parent"), SCSNode->ParentComponentOrVariableName.ToString());
			}

			CompArray.Add(MakeShared<FJsonValueObject>(CompObj));
		}
	}
	Data->SetArrayField(TEXT("components"), CompArray);

	// Event graphs — with full node + pin + connection details
	TArray<TSharedPtr<FJsonValue>> GraphArray;
	for (UEdGraph* Graph : Blueprint->UbergraphPages)
	{
		if (!Graph) continue;
		if (!FilterGraph.IsEmpty() && !Graph->GetName().Equals(FilterGraph, ESearchCase::IgnoreCase)) continue;

		GraphArray.Add(MakeShared<FJsonValueObject>(GraphToJson(Graph)));
	}
	Data->SetArrayField(TEXT("eventGraphs"), GraphArray);

	return FCommandResult::Success(Data);
}

// ============================================
// create_variable
// Args: { "blueprint_name": "BP_MyActor", "variable_name": "Health", "variable_type": "Float", "default_value": "100.0" }
// ============================================
FCommandResult FBlueprintCommandHandler::HandleCreateVariable(const TSharedPtr<FJsonObject>& Args)
{
	FString BPName;
	if (!Args->TryGetStringField(TEXT("blueprint_name"), BPName))
	{
		return FCommandResult::Fail(TEXT("Missing required 'blueprint_name' argument"));
	}

	FString VarName;
	if (!Args->TryGetStringField(TEXT("variable_name"), VarName))
	{
		return FCommandResult::Fail(TEXT("Missing required 'variable_name' argument"));
	}

	FString VarType;
	if (!Args->TryGetStringField(TEXT("variable_type"), VarType))
	{
		return FCommandResult::Fail(TEXT("Missing required 'variable_type' argument"));
	}

	UBlueprint* Blueprint = FindBlueprintByName(BPName);
	if (!Blueprint)
	{
		return FCommandResult::Fail(FString::Printf(TEXT("Blueprint '%s' not found"), *BPName));
	}

	// Resolve type
	FEdGraphPinType PinType;
	if (!ResolveVariableType(VarType, PinType))
	{
		return FCommandResult::Fail(FString::Printf(
			TEXT("Unknown variable type '%s'. Pass: Boolean, Int, Float, Double, String, Text, Name, Byte, Object, or any Struct/Class name (Vector, Rotator, Transform, GameplayTag, etc.)"),
			*VarType));
	}

	// Add the variable
	FName VarFName(*VarName);
	bool bAdded = FBlueprintEditorUtils::AddMemberVariable(Blueprint, VarFName, PinType);
	if (!bAdded)
	{
		return FCommandResult::Fail(FString::Printf(TEXT("Failed to add variable '%s' (may already exist)"), *VarName));
	}

	// Set default value if provided
	FString DefaultValue;
	if (Args->TryGetStringField(TEXT("default_value"), DefaultValue) && !DefaultValue.IsEmpty())
	{
		for (FBPVariableDescription& Var : Blueprint->NewVariables)
		{
			if (Var.VarName == VarFName)
			{
				Var.DefaultValue = DefaultValue;
				break;
			}
		}
	}

	Blueprint->MarkPackageDirty();
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("blueprintName"), BPName);
	Data->SetStringField(TEXT("variableName"), VarName);
	Data->SetStringField(TEXT("variableType"), VarType);
	if (!DefaultValue.IsEmpty())
	{
		Data->SetStringField(TEXT("defaultValue"), DefaultValue);
	}
	Data->SetStringField(TEXT("message"), FString::Printf(TEXT("Added variable '%s' (%s) to '%s'"), *VarName, *VarType, *BPName));

	return FCommandResult::Success(Data);
}

// ============================================
// add_component_to_blueprint
// Args: { "blueprint_name": "BP_MyActor", "component_type": "StaticMesh", "component_name": "MyMesh" }
// ============================================
FCommandResult FBlueprintCommandHandler::HandleAddComponent(const TSharedPtr<FJsonObject>& Args)
{
	FString BPName;
	if (!Args->TryGetStringField(TEXT("blueprint_name"), BPName))
	{
		return FCommandResult::Fail(TEXT("Missing required 'blueprint_name' argument"));
	}

	FString CompType;
	if (!Args->TryGetStringField(TEXT("component_type"), CompType))
	{
		return FCommandResult::Fail(TEXT("Missing required 'component_type' argument"));
	}

	FString CompName;
	if (!Args->TryGetStringField(TEXT("component_name"), CompName))
	{
		CompName = FString::Printf(TEXT("New%s"), *CompType);
	}

	UBlueprint* Blueprint = FindBlueprintByName(BPName);
	if (!Blueprint)
	{
		return FCommandResult::Fail(FString::Printf(TEXT("Blueprint '%s' not found"), *BPName));
	}

	if (!Blueprint->SimpleConstructionScript)
	{
		return FCommandResult::Fail(FString::Printf(TEXT("Blueprint '%s' has no SimpleConstructionScript (may not be an Actor BP)"), *BPName));
	}

	// Dynamic component class lookup — search all UActorComponent subclasses by name
	UClass* ComponentClass = nullptr;

	for (TObjectIterator<UClass> It; It; ++It)
	{
		if (!It->IsChildOf(UActorComponent::StaticClass())) continue;
		if (It->HasAnyClassFlags(CLASS_Abstract)) continue;

		FString ClassName = It->GetName();
		// Strip "Component" suffix for matching: UStaticMeshComponent -> "StaticMesh"
		FString ShortName = ClassName;
		ShortName.RemoveFromEnd(TEXT("Component"));
		// Also strip 'U' prefix
		FString NoPrefix = ClassName;
		if (NoPrefix.Len() > 1 && NoPrefix[0] == 'U') NoPrefix.RemoveAt(0);
		FString NoPrefix2 = ShortName;

		if (ClassName.Equals(CompType, ESearchCase::IgnoreCase) ||
			ShortName.Equals(CompType, ESearchCase::IgnoreCase) ||
			NoPrefix.Equals(CompType, ESearchCase::IgnoreCase) ||
			NoPrefix2.Equals(CompType, ESearchCase::IgnoreCase))
		{
			ComponentClass = *It;
			break;
		}
	}

	if (!ComponentClass)
	{
		return FCommandResult::Fail(FString::Printf(
			TEXT("Component type '%s' not found. Pass any UActorComponent subclass name (e.g. StaticMeshComponent, CameraComponent, CapsuleComponent, SpringArmComponent, etc.)"),
			*CompType));
	}

	// Create the SCS node
	USCS_Node* NewNode = Blueprint->SimpleConstructionScript->CreateNode(ComponentClass, FName(*CompName));
	if (!NewNode)
	{
		return FCommandResult::Fail(TEXT("Failed to create component node"));
	}

	// Add to root or default scene root
	Blueprint->SimpleConstructionScript->AddNode(NewNode);

	Blueprint->MarkPackageDirty();
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("blueprintName"), BPName);
	Data->SetStringField(TEXT("componentName"), CompName);
	Data->SetStringField(TEXT("componentType"), ComponentClass->GetName());
	Data->SetStringField(TEXT("message"), FString::Printf(TEXT("Added %s component '%s' to '%s'"), *ComponentClass->GetName(), *CompName, *BPName));

	return FCommandResult::Success(Data);
}

// ============================================
// reparent_component
// Change the parent of a component in a Blueprint's component tree (SCS).
// Args: {
//   "blueprint_name": "BP_MyActor",
//   "component_name": "FPSCamera",
//   "new_parent": "CameraBoom"          // Name of the new parent component, or empty/"" for root
// }
// ============================================
FCommandResult FBlueprintCommandHandler::HandleReparentComponent(const TSharedPtr<FJsonObject>& Args)
{
	FString BPName;
	if (!Args->TryGetStringField(TEXT("blueprint_name"), BPName))
		return FCommandResult::Fail(TEXT("Missing required 'blueprint_name' argument"));

	FString CompName;
	if (!Args->TryGetStringField(TEXT("component_name"), CompName))
		return FCommandResult::Fail(TEXT("Missing required 'component_name' argument"));

	FString NewParentName;
	Args->TryGetStringField(TEXT("new_parent"), NewParentName);

	UBlueprint* Blueprint = FindBlueprintByName(BPName);
	if (!Blueprint)
		return FCommandResult::Fail(FString::Printf(TEXT("Blueprint '%s' not found"), *BPName));

	if (!Blueprint->SimpleConstructionScript)
		return FCommandResult::Fail(TEXT("Blueprint has no SimpleConstructionScript (may not be an Actor BP)"));

	USCS_Node* ChildNode = nullptr;
	USCS_Node* NewParentNode = nullptr;
	FString OldParentName = TEXT("(root)");

	// Find the child node and optionally the new parent node
	for (USCS_Node* Node : Blueprint->SimpleConstructionScript->GetAllNodes())
	{
		if (!Node || !Node->ComponentTemplate) continue;
		FString Name = Node->ComponentTemplate->GetName();
		if (Name.Equals(CompName, ESearchCase::IgnoreCase))
			ChildNode = Node;
		if (!NewParentName.IsEmpty() && Name.Equals(NewParentName, ESearchCase::IgnoreCase))
			NewParentNode = Node;
	}

	if (!ChildNode)
		return FCommandResult::Fail(FString::Printf(TEXT("Component '%s' not found in SCS"), *CompName));

	if (!NewParentName.IsEmpty() && !NewParentNode)
		return FCommandResult::Fail(FString::Printf(TEXT("Parent component '%s' not found in SCS"), *NewParentName));

	if (ChildNode == NewParentNode)
		return FCommandResult::Fail(TEXT("Cannot reparent a component to itself"));

	// Record old parent for response
	USCS_Node* OldParent = nullptr;
	for (USCS_Node* Node : Blueprint->SimpleConstructionScript->GetAllNodes())
	{
		if (!Node) continue;
		if (Node->GetChildNodes().Contains(ChildNode))
		{
			OldParent = Node;
			OldParentName = Node->ComponentTemplate ? Node->ComponentTemplate->GetName() : TEXT("(unknown)");
			break;
		}
	}

	// Remove from current parent
	if (OldParent)
	{
		OldParent->RemoveChildNode(ChildNode, /*bRecompile=*/false);
	}
	else
	{
		// It's a root node
		Blueprint->SimpleConstructionScript->RemoveNode(ChildNode, /*bRecompile=*/false);
	}

	// Add to new parent (or root)
	if (NewParentNode)
	{
		NewParentNode->AddChildNode(ChildNode, /*bRecompile=*/false);
	}
	else
	{
		Blueprint->SimpleConstructionScript->AddNode(ChildNode);
	}

	Blueprint->MarkPackageDirty();
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("blueprintName"), BPName);
	Data->SetStringField(TEXT("componentName"), CompName);
	Data->SetStringField(TEXT("oldParent"), OldParentName);
	Data->SetStringField(TEXT("newParent"), NewParentName.IsEmpty() ? TEXT("(root)") : NewParentName);
	Data->SetStringField(TEXT("message"), FString::Printf(TEXT("Reparented '%s' from '%s' to '%s'"),
		*CompName, *OldParentName, NewParentName.IsEmpty() ? TEXT("(root)") : *NewParentName));

	return FCommandResult::Success(Data);
}

// ============================================
// create_function
// Args: { "blueprint_name": "BP_MyActor", "function_name": "TakeDamage" }
// ============================================
FCommandResult FBlueprintCommandHandler::HandleCreateFunction(const TSharedPtr<FJsonObject>& Args)
{
	FString BPName;
	if (!Args->TryGetStringField(TEXT("blueprint_name"), BPName))
	{
		return FCommandResult::Fail(TEXT("Missing required 'blueprint_name' argument"));
	}

	FString FuncName;
	if (!Args->TryGetStringField(TEXT("function_name"), FuncName))
	{
		return FCommandResult::Fail(TEXT("Missing required 'function_name' argument"));
	}

	UBlueprint* Blueprint = FindBlueprintByName(BPName);
	if (!Blueprint)
	{
		return FCommandResult::Fail(FString::Printf(TEXT("Blueprint '%s' not found"), *BPName));
	}

	// Check if function already exists
	for (UEdGraph* Graph : Blueprint->FunctionGraphs)
	{
		if (Graph && Graph->GetName() == FuncName)
		{
			return FCommandResult::Fail(FString::Printf(TEXT("Function '%s' already exists in '%s'"), *FuncName, *BPName));
		}
	}

	// Create function graph
	UEdGraph* FunctionGraph = FBlueprintEditorUtils::CreateNewGraph(
		Blueprint,
		FName(*FuncName),
		UEdGraph::StaticClass(),
		UEdGraphSchema_K2::StaticClass()
	);

	if (!FunctionGraph)
	{
		return FCommandResult::Fail(TEXT("Failed to create function graph"));
	}

	FBlueprintEditorUtils::AddFunctionGraph(Blueprint, FunctionGraph, /*bIsUserCreated=*/ true, /*SignatureFromObject=*/ (UFunction*)nullptr);

	Blueprint->MarkPackageDirty();
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("blueprintName"), BPName);
	Data->SetStringField(TEXT("functionName"), FuncName);
	Data->SetNumberField(TEXT("nodeCount"), FunctionGraph->Nodes.Num());
	Data->SetStringField(TEXT("message"), FString::Printf(TEXT("Created function '%s' in '%s'"), *FuncName, *BPName));

	return FCommandResult::Success(Data);
}

// ============================================
// add_function_parameter
// Args: { "blueprint_name": "BP_MyActor", "function_name": "TakeDamage", "param_name": "Amount", "param_type": "Float", "is_output": false }
// ============================================
FCommandResult FBlueprintCommandHandler::HandleAddFunctionParameter(const TSharedPtr<FJsonObject>& Args)
{
	FString BPName;
	if (!Args->TryGetStringField(TEXT("blueprint_name"), BPName))
	{
		return FCommandResult::Fail(TEXT("Missing required 'blueprint_name' argument"));
	}

	FString FuncName;
	if (!Args->TryGetStringField(TEXT("function_name"), FuncName))
	{
		return FCommandResult::Fail(TEXT("Missing required 'function_name' argument"));
	}

	FString ParamName;
	if (!Args->TryGetStringField(TEXT("param_name"), ParamName))
	{
		return FCommandResult::Fail(TEXT("Missing required 'param_name' argument"));
	}

	FString ParamType;
	if (!Args->TryGetStringField(TEXT("param_type"), ParamType))
	{
		return FCommandResult::Fail(TEXT("Missing required 'param_type' argument"));
	}

	bool bIsOutput = false;
	Args->TryGetBoolField(TEXT("is_output"), bIsOutput);

	UBlueprint* Blueprint = FindBlueprintByName(BPName);
	if (!Blueprint)
	{
		return FCommandResult::Fail(FString::Printf(TEXT("Blueprint '%s' not found"), *BPName));
	}

	// Find the function graph
	UEdGraph* FunctionGraph = nullptr;
	for (UEdGraph* Graph : Blueprint->FunctionGraphs)
	{
		if (Graph && Graph->GetName() == FuncName)
		{
			FunctionGraph = Graph;
			break;
		}
	}

	if (!FunctionGraph)
	{
		return FCommandResult::Fail(FString::Printf(TEXT("Function '%s' not found in '%s'"), *FuncName, *BPName));
	}

	// Resolve the parameter type
	FEdGraphPinType PinType;
	if (!ResolveVariableType(ParamType, PinType))
	{
		return FCommandResult::Fail(FString::Printf(TEXT("Unknown parameter type '%s'"), *ParamType));
	}

	// Find the function entry or result node
	UEdGraphNode* TargetNode = nullptr;
	for (UEdGraphNode* Node : FunctionGraph->Nodes)
	{
		if (!bIsOutput && Node->IsA<UK2Node_FunctionEntry>())
		{
			TargetNode = Node;
			break;
		}
		if (bIsOutput && Node->IsA<UK2Node_FunctionResult>())
		{
			TargetNode = Node;
			break;
		}
	}

	if (!TargetNode)
	{
		if (bIsOutput)
		{
			return FCommandResult::Fail(TEXT("No FunctionResult node found. The function may need a return node first."));
		}
		return FCommandResult::Fail(TEXT("No FunctionEntry node found in function graph"));
	}

	// Add the pin via the node
	UEdGraphPin* NewPin = TargetNode->CreatePin(
		bIsOutput ? EGPD_Input : EGPD_Output,  // Entry outputs = function inputs; Result inputs = function outputs
		PinType.PinCategory,
		FName(*ParamName)
	);

	if (!NewPin)
	{
		return FCommandResult::Fail(TEXT("Failed to create parameter pin"));
	}

	NewPin->PinType = PinType;

	TargetNode->ReconstructNode();

	// --- Auto-refresh all CallFunction nodes that reference this function ---
	// After adding a parameter, any existing call sites need ReconstructNode()
	// to pick up the new pin. Search all graphs in this Blueprint.
	int32 RefreshedCallNodes = 0;

	// Resolve the function's UFunction to match against call nodes
	UFunction* TargetFunc = nullptr;
	if (Blueprint->SkeletonGeneratedClass)
	{
		TargetFunc = Blueprint->SkeletonGeneratedClass->FindFunctionByName(FName(*FuncName));
	}

	if (TargetFunc)
	{
		// Collect all graphs to search
		TArray<UEdGraph*> AllGraphs;
		AllGraphs.Append(Blueprint->UbergraphPages);
		AllGraphs.Append(Blueprint->FunctionGraphs);

		for (UEdGraph* SearchGraph : AllGraphs)
		{
			if (!SearchGraph) continue;
			for (UEdGraphNode* Node : SearchGraph->Nodes)
			{
				UK2Node_CallFunction* CallNode = Cast<UK2Node_CallFunction>(Node);
				if (CallNode)
				{
					// Check if this call node targets the same function
					UFunction* CalledFunc = CallNode->GetTargetFunction();
					if (CalledFunc == TargetFunc ||
						(CalledFunc && CalledFunc->GetName() == FuncName))
					{
						CallNode->ReconstructNode();
						RefreshedCallNodes++;
					}
				}
			}
		}
	}

	Blueprint->MarkPackageDirty();
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("blueprintName"), BPName);
	Data->SetStringField(TEXT("functionName"), FuncName);
	Data->SetStringField(TEXT("paramName"), ParamName);
	Data->SetStringField(TEXT("paramType"), ParamType);
	Data->SetBoolField(TEXT("isOutput"), bIsOutput);
	Data->SetNumberField(TEXT("refreshedCallNodes"), RefreshedCallNodes);
	Data->SetStringField(TEXT("message"), FString::Printf(TEXT("Added %s parameter '%s' (%s) to function '%s'. Refreshed %d call node(s)."),
		bIsOutput ? TEXT("output") : TEXT("input"), *ParamName, *ParamType, *FuncName, RefreshedCallNodes));

	return FCommandResult::Success(Data);
}

// ============================================
// Helpers
// ============================================

UBlueprint* FBlueprintCommandHandler::FindBlueprintByName(const FString& Name) const
{
	// Special case: Level Blueprint access
	if (Name.Equals(TEXT("LevelBlueprint"), ESearchCase::IgnoreCase) ||
		Name.Equals(TEXT("__LevelBlueprint__"), ESearchCase::IgnoreCase) ||
		Name.Contains(TEXT("LevelScriptBlueprint")))
	{
		if (GEditor)
		{
			UWorld* World = GEditor->GetEditorWorldContext().World();
			if (World && World->PersistentLevel)
			{
				ULevelScriptBlueprint* LevelBP = World->PersistentLevel->GetLevelScriptBlueprint(true);
				return LevelBP;
			}
		}
		return nullptr;
	}

	// Search in /Game/ tree
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

	TArray<FAssetData> AssetList;
	AssetRegistry.GetAssetsByClass(UBlueprint::StaticClass()->GetClassPathName(), AssetList, true);

	for (const FAssetData& Asset : AssetList)
	{
		if (Asset.AssetName.ToString() == Name ||
			Asset.AssetName.ToString().Equals(Name, ESearchCase::IgnoreCase))
		{
			return Cast<UBlueprint>(Asset.GetAsset());
		}
	}

	// Also try loading directly
	FString Path = FString::Printf(TEXT("/Game/Blueprints/%s.%s"), *Name, *Name);
	return LoadObject<UBlueprint>(nullptr, *Path);
}

UClass* FBlueprintCommandHandler::ResolveParentClass(const FString& ClassName) const
{
	// Dynamic class lookup — no hardcoded mapping.
	// Try multiple module paths and prefix combinations.
	TArray<FString> Candidates = {
		ClassName,
		FString::Printf(TEXT("A%s"), *ClassName),   // AActor, ACharacter...
		FString::Printf(TEXT("U%s"), *ClassName),   // UActorComponent, UUserWidget...
	};

	TArray<FString> Modules = {
		TEXT("/Script/Engine"),
		TEXT("/Script/UMG"),
		TEXT("/Script/GameplayAbilities"),
		TEXT("/Script/AIModule"),
		TEXT("/Script/EnhancedInput"),
	};

	for (const FString& Module : Modules)
	{
		for (const FString& Candidate : Candidates)
		{
			UClass* Found = FindObject<UClass>(nullptr, *FString::Printf(TEXT("%s.%s"), *Module, *Candidate));
			if (Found) return Found;
		}
	}

	// Last resort: iterate all UClasses for a name match
	for (TObjectIterator<UClass> It; It; ++It)
	{
		UClass* C = *It;
		FString CName = C->GetName();
		// Match: "Character" -> "ACharacter", or "ACharacter" -> "ACharacter"
		FString ShortName = CName;
		if (ShortName.Len() > 1 && (ShortName[0] == 'A' || ShortName[0] == 'U'))
		{
			ShortName.RemoveAt(0);
		}
		if (CName.Equals(ClassName, ESearchCase::IgnoreCase) ||
			ShortName.Equals(ClassName, ESearchCase::IgnoreCase))
		{
			// Verify it's usable as a blueprint parent
			if (C->IsChildOf(UObject::StaticClass()) && !C->HasAnyClassFlags(CLASS_Deprecated))
			{
				return C;
			}
		}
	}

	return nullptr;
}

bool FBlueprintCommandHandler::ResolveVariableType(const FString& TypeName, FEdGraphPinType& OutPinType) const
{
	FString TypeUpper = TypeName.ToUpper();

	OutPinType = FEdGraphPinType();
	OutPinType.ContainerType = EPinContainerType::None;

	if (TypeUpper == TEXT("BOOLEAN") || TypeUpper == TEXT("BOOL"))
	{
		OutPinType.PinCategory = UEdGraphSchema_K2::PC_Boolean;
	}
	else if (TypeUpper == TEXT("INTEGER") || TypeUpper == TEXT("INT") || TypeUpper == TEXT("INT32"))
	{
		OutPinType.PinCategory = UEdGraphSchema_K2::PC_Int;
	}
	else if (TypeUpper == TEXT("INT64"))
	{
		OutPinType.PinCategory = UEdGraphSchema_K2::PC_Int64;
	}
	else if (TypeUpper == TEXT("FLOAT"))
	{
		OutPinType.PinCategory = UEdGraphSchema_K2::PC_Real;
		OutPinType.PinSubCategory = TEXT("float");
	}
	else if (TypeUpper == TEXT("DOUBLE"))
	{
		OutPinType.PinCategory = UEdGraphSchema_K2::PC_Real;
		OutPinType.PinSubCategory = TEXT("double");
	}
	else if (TypeUpper == TEXT("STRING"))
	{
		OutPinType.PinCategory = UEdGraphSchema_K2::PC_String;
	}
	else if (TypeUpper == TEXT("TEXT"))
	{
		OutPinType.PinCategory = UEdGraphSchema_K2::PC_Text;
	}
	else if (TypeUpper == TEXT("NAME"))
	{
		OutPinType.PinCategory = UEdGraphSchema_K2::PC_Name;
	}
	else if (TypeUpper == TEXT("VECTOR"))
	{
		OutPinType.PinCategory = UEdGraphSchema_K2::PC_Struct;
		OutPinType.PinSubCategoryObject = TBaseStructure<FVector>::Get();
	}
	else if (TypeUpper == TEXT("ROTATOR"))
	{
		OutPinType.PinCategory = UEdGraphSchema_K2::PC_Struct;
		OutPinType.PinSubCategoryObject = TBaseStructure<FRotator>::Get();
	}
	else if (TypeUpper == TEXT("TRANSFORM"))
	{
		OutPinType.PinCategory = UEdGraphSchema_K2::PC_Struct;
		OutPinType.PinSubCategoryObject = TBaseStructure<FTransform>::Get();
	}
	else if (TypeUpper == TEXT("COLOR"))
	{
		OutPinType.PinCategory = UEdGraphSchema_K2::PC_Struct;
		OutPinType.PinSubCategoryObject = TBaseStructure<FColor>::Get();
	}
	else if (TypeUpper == TEXT("LINEARCOLOR"))
	{
		OutPinType.PinCategory = UEdGraphSchema_K2::PC_Struct;
		OutPinType.PinSubCategoryObject = TBaseStructure<FLinearColor>::Get();
	}
	else if (TypeUpper == TEXT("OBJECT"))
	{
		OutPinType.PinCategory = UEdGraphSchema_K2::PC_Object;
		OutPinType.PinSubCategoryObject = UObject::StaticClass();
	}
	else if (TypeUpper == TEXT("BYTE"))
	{
		OutPinType.PinCategory = UEdGraphSchema_K2::PC_Byte;
	}
	else if (TypeUpper == TEXT("ENUM"))
	{
		OutPinType.PinCategory = UEdGraphSchema_K2::PC_Byte;
		// If an enum_name is specified, it will be resolved by the caller
	}
	else if (TypeUpper == TEXT("CLASS"))
	{
		OutPinType.PinCategory = UEdGraphSchema_K2::PC_Class;
		OutPinType.PinSubCategoryObject = UObject::StaticClass();
	}
	else if (TypeUpper == TEXT("SOFTOBJECT"))
	{
		OutPinType.PinCategory = UEdGraphSchema_K2::PC_SoftObject;
		OutPinType.PinSubCategoryObject = UObject::StaticClass();
	}
	else
	{
		// Dynamic lookup: try to find a UScriptStruct by name (e.g. "GameplayTag", "Vector2D", "Guid")
		UScriptStruct* FoundStruct = nullptr;
		for (TObjectIterator<UScriptStruct> It; It; ++It)
		{
			FString StructName = It->GetName();
			// Match with or without 'F' prefix: "FVector2D" or "Vector2D"
			FString ShortName = StructName;
			if (ShortName.Len() > 1 && ShortName[0] == 'F') ShortName.RemoveAt(0);

			if (StructName.Equals(TypeName, ESearchCase::IgnoreCase) ||
				ShortName.Equals(TypeName, ESearchCase::IgnoreCase))
			{
				FoundStruct = *It;
				break;
			}
		}

		if (FoundStruct)
		{
			OutPinType.PinCategory = UEdGraphSchema_K2::PC_Struct;
			OutPinType.PinSubCategoryObject = FoundStruct;
		}
		else
		{
			// Try as UObject subclass (e.g. "StaticMesh", "Texture2D")
			UClass* FoundClass = nullptr;
			for (TObjectIterator<UClass> ClassIt; ClassIt; ++ClassIt)
			{
				FString CName = ClassIt->GetName();
				FString ShortCName = CName;
				if (ShortCName.Len() > 1 && ShortCName[0] == 'U') ShortCName.RemoveAt(0);

				if (CName.Equals(TypeName, ESearchCase::IgnoreCase) ||
					ShortCName.Equals(TypeName, ESearchCase::IgnoreCase))
				{
					FoundClass = *ClassIt;
					break;
				}
			}

			if (FoundClass)
			{
				OutPinType.PinCategory = UEdGraphSchema_K2::PC_Object;
				OutPinType.PinSubCategoryObject = FoundClass;
			}
			else
			{
				return false;
			}
		}
	}

	return true;
}

// ============================================
// create_event_dispatcher
// Args: { "blueprint_name": "BP_Hero", "dispatcher_name": "OnHealthChanged",
//         "params": [{"name": "NewHealth", "type": "Float"}] (optional) }
// ============================================
FCommandResult FBlueprintCommandHandler::HandleCreateEventDispatcher(const TSharedPtr<FJsonObject>& Args)
{
	FString BPName, DispatcherName;
	if (!Args->TryGetStringField(TEXT("blueprint_name"), BPName))
	{
		return FCommandResult::Fail(TEXT("Missing required 'blueprint_name' argument"));
	}
	if (!Args->TryGetStringField(TEXT("dispatcher_name"), DispatcherName))
	{
		return FCommandResult::Fail(TEXT("Missing required 'dispatcher_name' argument"));
	}

	UBlueprint* Blueprint = FindBlueprintByName(BPName);
	if (!Blueprint)
	{
		return FCommandResult::Fail(FString::Printf(TEXT("Blueprint '%s' not found"), *BPName));
	}

	FName DispFName(*DispatcherName);

	// Check if it already exists
	for (const FBPVariableDescription& Var : Blueprint->NewVariables)
	{
		if (Var.VarName == DispFName && Var.VarType.PinCategory == UEdGraphSchema_K2::PC_MCDelegate)
		{
			return FCommandResult::Fail(FString::Printf(TEXT("Event dispatcher '%s' already exists in '%s'"), *DispatcherName, *BPName));
		}
	}

	// Create the multicast delegate variable
	FEdGraphPinType PinType;
	PinType.PinCategory = UEdGraphSchema_K2::PC_MCDelegate;
	PinType.PinSubCategory = NAME_None;

	bool bAdded = FBlueprintEditorUtils::AddMemberVariable(Blueprint, DispFName, PinType);
	if (!bAdded)
	{
		return FCommandResult::Fail(FString::Printf(TEXT("Failed to create event dispatcher '%s'"), *DispatcherName));
	}

	// Find the delegate signature graph to add parameters
	const TArray<TSharedPtr<FJsonValue>>* ParamsArray = nullptr;
	if (Args->TryGetArrayField(TEXT("params"), ParamsArray) && ParamsArray)
	{
		// The delegate gets a signature graph - find it
		UEdGraph* DelegateGraph = nullptr;
		for (UEdGraph* Graph : Blueprint->DelegateSignatureGraphs)
		{
			if (Graph && Graph->GetName().Contains(DispatcherName))
			{
				DelegateGraph = Graph;
				break;
			}
		}

		if (DelegateGraph)
		{
			for (const TSharedPtr<FJsonValue>& ParamVal : *ParamsArray)
			{
				const TSharedPtr<FJsonObject>* ParamObj;
				if (ParamVal->TryGetObject(ParamObj))
				{
					FString PName, PType;
					(*ParamObj)->TryGetStringField(TEXT("name"), PName);
					(*ParamObj)->TryGetStringField(TEXT("type"), PType);

					if (!PName.IsEmpty() && !PType.IsEmpty())
					{
						FEdGraphPinType ParamPinType;
						if (ResolveVariableType(PType, ParamPinType))
						{
							// Find the FunctionEntry node and add the pin
							for (UEdGraphNode* Node : DelegateGraph->Nodes)
							{
								if (UK2Node_FunctionEntry* EntryNode = Cast<UK2Node_FunctionEntry>(Node))
								{
									TSharedPtr<FUserPinInfo> NewPin = MakeShared<FUserPinInfo>();
									NewPin->PinName = FName(*PName);
									NewPin->PinType = ParamPinType;
									EntryNode->UserDefinedPins.Add(NewPin);
									EntryNode->ReconstructNode();
									break;
								}
							}
						}
					}
				}
			}
		}
	}

	Blueprint->MarkPackageDirty();
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("blueprintName"), BPName);
	Data->SetStringField(TEXT("dispatcherName"), DispatcherName);
	Data->SetStringField(TEXT("message"), FString::Printf(TEXT("Created event dispatcher '%s' in '%s'"), *DispatcherName, *BPName));
	return FCommandResult::Success(Data);
}

// ============================================
// create_blueprint_interface
// Args: { "name": "BPI_Interactable" }
// ============================================
FCommandResult FBlueprintCommandHandler::HandleCreateBlueprintInterface(const TSharedPtr<FJsonObject>& Args)
{
	FString Name;
	if (!Args->TryGetStringField(TEXT("name"), Name))
	{
		return FCommandResult::Fail(TEXT("Missing required 'name' argument"));
	}

	FString PackagePath = TEXT("/Game/Blueprints/Interfaces");
	Args->TryGetStringField(TEXT("package_path"), PackagePath);

	// Use AssetTools to create
	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();

	// Find the BlueprintInterface factory
	UFactory* Factory = nullptr;
	for (TObjectIterator<UClass> It; It; ++It)
	{
		if (It->IsChildOf(UFactory::StaticClass()) && !It->HasAnyClassFlags(CLASS_Abstract))
		{
			UFactory* TestFactory = It->GetDefaultObject<UFactory>();
			if (TestFactory && TestFactory->SupportedClass && TestFactory->SupportedClass->GetName().Contains(TEXT("BlueprintInterface")))
			{
				Factory = NewObject<UFactory>(GetTransientPackage(), *It);
				break;
			}
		}
	}

	// Fallback: create manually
	FString FullPath = FString::Printf(TEXT("%s/%s"), *PackagePath, *Name);
	UPackage* Package = CreatePackage(*FullPath);
	if (!Package)
	{
		return FCommandResult::Fail(TEXT("Failed to create package for Blueprint Interface"));
	}

	UBlueprint* InterfaceBP = FKismetEditorUtilities::CreateBlueprint(
		UInterface::StaticClass(),
		Package,
		FName(*Name),
		BPTYPE_Interface,
		UBlueprint::StaticClass(),
		UBlueprintGeneratedClass::StaticClass()
	);

	if (!InterfaceBP)
	{
		return FCommandResult::Fail(TEXT("Failed to create Blueprint Interface"));
	}

	// Mark dirty and notify
	Package->MarkPackageDirty();
	FAssetRegistryModule::AssetCreated(InterfaceBP);

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("name"), Name);
	Data->SetStringField(TEXT("path"), InterfaceBP->GetPathName());
	Data->SetStringField(TEXT("type"), TEXT("BlueprintInterface"));
	Data->SetStringField(TEXT("message"), FString::Printf(TEXT("Created Blueprint Interface '%s'"), *Name));
	return FCommandResult::Success(Data);
}

// ============================================
// implement_interface
// Args: { "blueprint_name": "BP_Hero", "interface_name": "BPI_Interactable" }
// ============================================
FCommandResult FBlueprintCommandHandler::HandleImplementInterface(const TSharedPtr<FJsonObject>& Args)
{
	FString BPName, InterfaceName;
	if (!Args->TryGetStringField(TEXT("blueprint_name"), BPName))
	{
		return FCommandResult::Fail(TEXT("Missing required 'blueprint_name' argument"));
	}
	if (!Args->TryGetStringField(TEXT("interface_name"), InterfaceName))
	{
		return FCommandResult::Fail(TEXT("Missing required 'interface_name' argument"));
	}

	UBlueprint* Blueprint = FindBlueprintByName(BPName);
	if (!Blueprint)
	{
		return FCommandResult::Fail(FString::Printf(TEXT("Blueprint '%s' not found"), *BPName));
	}

	// Find the interface Blueprint
	UBlueprint* InterfaceBP = FindBlueprintByName(InterfaceName);
	if (!InterfaceBP)
	{
		return FCommandResult::Fail(FString::Printf(TEXT("Interface '%s' not found"), *InterfaceName));
	}

	// Get the generated interface class
	UClass* InterfaceClass = InterfaceBP->GeneratedClass;
	if (!InterfaceClass)
	{
		return FCommandResult::Fail(FString::Printf(TEXT("Interface '%s' has no generated class — compile it first"), *InterfaceName));
	}

	// Check if already implemented
	for (const FBPInterfaceDescription& Impl : Blueprint->ImplementedInterfaces)
	{
		if (Impl.Interface == InterfaceClass)
		{
			return FCommandResult::Fail(FString::Printf(TEXT("Blueprint '%s' already implements '%s'"), *BPName, *InterfaceName));
		}
	}

	// Add the interface
	FBPInterfaceDescription NewInterface;
	NewInterface.Interface = TSubclassOf<UInterface>(InterfaceClass);

	Blueprint->ImplementedInterfaces.Add(NewInterface);

	// Regenerate function graphs for the interface functions
	FBlueprintEditorUtils::ImplementNewInterface(Blueprint, InterfaceClass->GetClassPathName());

	Blueprint->MarkPackageDirty();
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("blueprintName"), BPName);
	Data->SetStringField(TEXT("interfaceName"), InterfaceName);

	// List the interface functions that were added
	TArray<TSharedPtr<FJsonValue>> FuncArray;
	for (const FBPInterfaceDescription& Impl : Blueprint->ImplementedInterfaces)
	{
		if (Impl.Interface == InterfaceClass)
		{
			for (UEdGraph* Graph : Impl.Graphs)
			{
				if (Graph)
				{
					TSharedPtr<FJsonObject> FuncObj = MakeShared<FJsonObject>();
					FuncObj->SetStringField(TEXT("name"), Graph->GetName());
					FuncObj->SetNumberField(TEXT("nodeCount"), Graph->Nodes.Num());
					FuncArray.Add(MakeShared<FJsonValueObject>(FuncObj));
				}
			}
			break;
		}
	}
	Data->SetArrayField(TEXT("interfaceFunctions"), FuncArray);
	Data->SetStringField(TEXT("message"), FString::Printf(TEXT("Blueprint '%s' now implements '%s'"), *BPName, *InterfaceName));
	return FCommandResult::Success(Data);
}

// ============================================
// add_widget_child
// Args: { "blueprint_name": "WBP_HUD", "widget_type": "TextBlock", "widget_name": "TxtHealth",
//         "parent_name": "" (optional, root if omitted) }
// ============================================
FCommandResult FBlueprintCommandHandler::HandleAddWidgetChild(const TSharedPtr<FJsonObject>& Args)
{
	FString BPName, WidgetType, WidgetName;
	if (!Args->TryGetStringField(TEXT("blueprint_name"), BPName))
	{
		return FCommandResult::Fail(TEXT("Missing required 'blueprint_name' argument"));
	}
	if (!Args->TryGetStringField(TEXT("widget_type"), WidgetType))
	{
		return FCommandResult::Fail(TEXT("Missing required 'widget_type' argument"));
	}
	if (!Args->TryGetStringField(TEXT("widget_name"), WidgetName))
	{
		return FCommandResult::Fail(TEXT("Missing required 'widget_name' argument"));
	}

	// Find the Widget Blueprint
	UBlueprint* BP = FindBlueprintByName(BPName);
	UWidgetBlueprint* WidgetBP = Cast<UWidgetBlueprint>(BP);
	if (!WidgetBP)
	{
		return FCommandResult::Fail(FString::Printf(TEXT("'%s' is not a Widget Blueprint (UMG). Create it with parent_class 'UserWidget' first."), *BPName));
	}

	UWidgetTree* WidgetTree = WidgetBP->WidgetTree;
	if (!WidgetTree)
	{
		return FCommandResult::Fail(TEXT("Widget Blueprint has no WidgetTree"));
	}

	// Dynamically resolve the widget class
	UClass* WidgetClass = nullptr;
	for (TObjectIterator<UClass> It; It; ++It)
	{
		if (!It->IsChildOf(UWidget::StaticClass())) continue;
		if (It->HasAnyClassFlags(CLASS_Abstract)) continue;

		FString ClassName = It->GetName();
		FString ShortName = ClassName;
		// Strip common prefix 'U'
		if (ShortName.Len() > 1 && ShortName[0] == 'U') ShortName.RemoveAt(0);

		if (ClassName.Equals(WidgetType, ESearchCase::IgnoreCase) ||
			ShortName.Equals(WidgetType, ESearchCase::IgnoreCase))
		{
			WidgetClass = *It;
			break;
		}
	}

	if (!WidgetClass)
	{
		// List available widget types for helpful error
		TArray<FString> AvailableTypes;
		for (TObjectIterator<UClass> It; It; ++It)
		{
			if (It->IsChildOf(UWidget::StaticClass()) && !It->HasAnyClassFlags(CLASS_Abstract))
			{
				FString CName = It->GetName();
				if (CName.Len() > 1 && CName[0] == 'U') CName.RemoveAt(0);
				AvailableTypes.Add(CName);
			}
		}
		AvailableTypes.Sort();
		FString TypeList = FString::Join(AvailableTypes, TEXT(", "));
		return FCommandResult::Fail(FString::Printf(TEXT("Widget type '%s' not found. Available types: %s"), *WidgetType, *TypeList));
	}

	// Create the widget
	UWidget* NewWidget = WidgetTree->ConstructWidget<UWidget>(WidgetClass, FName(*WidgetName));
	if (!NewWidget)
	{
		return FCommandResult::Fail(FString::Printf(TEXT("Failed to construct widget of type '%s'"), *WidgetType));
	}

	// Find parent panel (or use root)
	FString ParentName;
	Args->TryGetStringField(TEXT("parent_name"), ParentName);

	if (!ParentName.IsEmpty())
	{
		UWidget* ParentWidget = WidgetTree->FindWidget(FName(*ParentName));
		if (!ParentWidget)
		{
			return FCommandResult::Fail(FString::Printf(TEXT("Parent widget '%s' not found in widget tree"), *ParentName));
		}

		UPanelWidget* ParentPanel = Cast<UPanelWidget>(ParentWidget);
		if (!ParentPanel)
		{
			return FCommandResult::Fail(FString::Printf(TEXT("Parent '%s' is not a panel widget (cannot have children)"), *ParentName));
		}

		UPanelSlot* Slot = ParentPanel->AddChild(NewWidget);
		if (!Slot)
		{
			return FCommandResult::Fail(TEXT("Failed to add widget to parent panel"));
		}
	}
	else
	{
		// Add to root
		if (!WidgetTree->RootWidget)
		{
			WidgetTree->RootWidget = NewWidget;
		}
		else
		{
			UPanelWidget* RootPanel = Cast<UPanelWidget>(WidgetTree->RootWidget);
			if (RootPanel)
			{
				RootPanel->AddChild(NewWidget);
			}
			else
			{
				return FCommandResult::Fail(TEXT("Root widget is not a panel — cannot add child. Specify a parent_name or set root to a panel widget like CanvasPanel first."));
			}
		}
	}

	WidgetBP->MarkPackageDirty();

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("blueprintName"), BPName);
	Data->SetStringField(TEXT("widgetName"), WidgetName);
	Data->SetStringField(TEXT("widgetType"), WidgetClass->GetName());
	Data->SetStringField(TEXT("parent"), ParentName.IsEmpty() ? TEXT("root") : ParentName);
	Data->SetStringField(TEXT("message"), FString::Printf(TEXT("Added '%s' (%s) to '%s'"), *WidgetName, *WidgetType, *BPName));
	return FCommandResult::Success(Data);
}
