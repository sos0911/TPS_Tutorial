// Copyright AI Assistant. All Rights Reserved.

#include "Commands/BlueprintNodeHandler.h"
#include "Editor.h"
#include "Engine/Blueprint.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphSchema_K2.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Kismet/KismetMathLibrary.h"
#include "Kismet/KismetStringLibrary.h"
#include "Kismet/KismetArrayLibrary.h"
#include "Kismet/KismetTextLibrary.h"
#include "Kismet/GameplayStatics.h"

// K2Node types for creating different node kinds
#include "K2Node_Event.h"
#include "K2Node_CallFunction.h"
#include "K2Node_IfThenElse.h"
#include "K2Node_ExecutionSequence.h"
#include "K2Node_VariableGet.h"
#include "K2Node_VariableSet.h"
#include "K2Node_SpawnActorFromClass.h"
#include "K2Node_Timeline.h"
#include "K2Node_InputAction.h"
#include "K2Node_CustomEvent.h"
#include "K2Node_MakeArray.h"
#include "K2Node_ForEachElementInEnum.h"
#include "K2Node_MacroInstance.h"
#include "K2Node_TemporaryVariable.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_FunctionResult.h"

// For generic K2Node discovery (TObjectIterator)
#include "UObject/UObjectIterator.h"
#include "K2Node.h"

// For component class function resolution
#include "Engine/SimpleConstructionScript.h"
#include "Engine/SCS_Node.h"
#include "GameFramework/Actor.h"
#include "Components/ActorComponent.h"

TArray<FString> FBlueprintNodeHandler::GetSupportedCommands() const
{
	return {
		TEXT("add_node"),
		TEXT("connect_nodes"),
		TEXT("disconnect_nodes"),
		TEXT("remove_node"),
		TEXT("get_node_pins"),
		TEXT("set_pin_default"),
		TEXT("batch_execute"),
		TEXT("auto_layout_graph"),
		TEXT("add_pin"),
		TEXT("list_node_types"),
		TEXT("list_blueprint_classes"),
		TEXT("list_functions"),
	};
}

FCommandResult FBlueprintNodeHandler::Execute(const FString& Command, const TSharedPtr<FJsonObject>& Args)
{
	if (Command == TEXT("add_node"))              return HandleAddNode(Args);
	if (Command == TEXT("connect_nodes"))         return HandleConnectNodes(Args);
	if (Command == TEXT("disconnect_nodes"))      return HandleDisconnectNodes(Args);
	if (Command == TEXT("remove_node"))           return HandleRemoveNode(Args);
	if (Command == TEXT("get_node_pins"))         return HandleGetNodePins(Args);
	if (Command == TEXT("set_pin_default"))       return HandleSetPinDefault(Args);
	if (Command == TEXT("batch_execute"))         return HandleBatchExecute(Args);
	if (Command == TEXT("auto_layout_graph"))     return HandleAutoLayoutGraph(Args);
	if (Command == TEXT("add_pin"))              return HandleAddPin(Args);
	if (Command == TEXT("list_node_types"))       return HandleListNodeTypes(Args);
	if (Command == TEXT("list_blueprint_classes")) return HandleListBlueprintClasses(Args);
	if (Command == TEXT("list_functions"))        return HandleListFunctions(Args);

	return FCommandResult::Fail(FString::Printf(TEXT("BlueprintNodeHandler: Unknown command '%s'"), *Command));
}

// ============================================
// Unified PinToJson — used everywhere for consistent format
// ============================================
TSharedPtr<FJsonObject> FBlueprintNodeHandler::PinToJson(UEdGraphPin* Pin) const
{
	TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
	if (!Pin) return Obj;

	Obj->SetStringField(TEXT("name"), Pin->PinName.ToString());
	Obj->SetStringField(TEXT("direction"), Pin->Direction == EGPD_Input ? TEXT("in") : TEXT("out"));
	Obj->SetStringField(TEXT("type"), Pin->PinType.PinCategory.ToString());

	// Sub-type info (e.g. object class, struct name)
	if (Pin->PinType.PinSubCategoryObject.Get() != nullptr)
	{
		Obj->SetStringField(TEXT("subtype"), Pin->PinType.PinSubCategoryObject->GetName());
	}

	// Default values
	if (!Pin->DefaultValue.IsEmpty())
	{
		Obj->SetStringField(TEXT("default"), Pin->DefaultValue);
	}
	if (!Pin->DefaultTextValue.IsEmpty())
	{
		Obj->SetStringField(TEXT("defaultText"), Pin->DefaultTextValue.ToString());
	}
	if (Pin->DefaultObject != nullptr)
	{
		Obj->SetStringField(TEXT("defaultObject"), Pin->DefaultObject->GetName());
	}

	// Connections
	if (Pin->LinkedTo.Num() > 0)
	{
		TArray<TSharedPtr<FJsonValue>> ConnArray;
		for (UEdGraphPin* Linked : Pin->LinkedTo)
		{
			if (Linked && Linked->GetOwningNode())
			{
				TSharedPtr<FJsonObject> ConnObj = MakeShared<FJsonObject>();
				ConnObj->SetStringField(TEXT("nodeId"), Linked->GetOwningNode()->NodeGuid.ToString());
				ConnObj->SetStringField(TEXT("pin"), Linked->PinName.ToString());
				ConnArray.Add(MakeShared<FJsonValueObject>(ConnObj));
			}
		}
		Obj->SetArrayField(TEXT("connections"), ConnArray);
	}

	return Obj;
}

// ============================================
// NodeToJson — returns node info with full pin details
// ============================================
TSharedPtr<FJsonObject> FBlueprintNodeHandler::NodeToJson(UEdGraphNode* Node) const
{
	TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
	if (!Node) return Obj;

	Obj->SetStringField(TEXT("id"), Node->NodeGuid.ToString());
	Obj->SetStringField(TEXT("type"), Node->GetClass()->GetName());
	Obj->SetStringField(TEXT("title"), Node->GetNodeTitle(ENodeTitleType::ListView).ToString());
	Obj->SetNumberField(TEXT("posX"), Node->NodePosX);
	Obj->SetNumberField(TEXT("posY"), Node->NodePosY);

	// Full pin details — always include for AI to know what pins are available
	TArray<TSharedPtr<FJsonValue>> PinArray;
	for (UEdGraphPin* Pin : Node->Pins)
	{
		if (Pin)
		{
			PinArray.Add(MakeShared<FJsonValueObject>(PinToJson(Pin)));
		}
	}
	Obj->SetArrayField(TEXT("pins"), PinArray);
	Obj->SetNumberField(TEXT("pinCount"), PinArray.Num());

	return Obj;
}

// ============================================
// Smart auto-position: find the rightmost node and place new node to its right
// ============================================
static void ComputeSmartPosition(UEdGraph* Graph, int32& OutPosX, int32& OutPosY)
{
	int32 MaxX = 0;
	int32 AvgY = 0;
	int32 NodeCount = 0;

	for (UEdGraphNode* Existing : Graph->Nodes)
	{
		if (Existing)
		{
			if (Existing->NodePosX > MaxX)
			{
				MaxX = Existing->NodePosX;
			}
			AvgY += Existing->NodePosY;
			NodeCount++;
		}
	}

	// Place 350px to the right of the rightmost node
	OutPosX = MaxX + 350;
	OutPosY = NodeCount > 0 ? (AvgY / NodeCount) : 0;
}

// ============================================
// GetSearchClasses — build list of UClasses to search for functions
// Includes: Kismet libraries, Blueprint parent chain, AND component classes
// ============================================
TArray<UClass*> FBlueprintNodeHandler::GetSearchClasses(UBlueprint* Blueprint) const
{
	TArray<UClass*> SearchClasses = {
		UKismetSystemLibrary::StaticClass(),
		UKismetMathLibrary::StaticClass(),
		UKismetStringLibrary::StaticClass(),
		UKismetArrayLibrary::StaticClass(),
		UKismetTextLibrary::StaticClass(),
		UGameplayStatics::StaticClass(),
	};

	if (Blueprint && Blueprint->ParentClass)
	{
		// Add Blueprint parent chain (Actor -> Pawn -> Character -> ...)
		for (UClass* C = Blueprint->ParentClass; C; C = C->GetSuperClass())
		{
			SearchClasses.AddUnique(C);
		}
	}

	// Add component classes from CDO (inherited components like CharacterMovementComponent)
	if (Blueprint && Blueprint->GeneratedClass)
	{
		UObject* CDO = Blueprint->GeneratedClass->GetDefaultObject();
		AActor* ActorCDO = Cast<AActor>(CDO);
		if (ActorCDO)
		{
			TArray<UActorComponent*> Components;
			ActorCDO->GetComponents(Components);
			for (UActorComponent* Comp : Components)
			{
				if (Comp)
				{
					for (UClass* C = Comp->GetClass(); C && C != UActorComponent::StaticClass()->GetSuperClass(); C = C->GetSuperClass())
					{
						SearchClasses.AddUnique(C);
					}
				}
			}
		}
	}

	// Add SCS component classes (Blueprint-added components like SpringArm, Camera)
	if (Blueprint && Blueprint->SimpleConstructionScript)
	{
		for (USCS_Node* Node : Blueprint->SimpleConstructionScript->GetAllNodes())
		{
			if (Node && Node->ComponentTemplate)
			{
				for (UClass* C = Node->ComponentTemplate->GetClass(); C && C != UActorComponent::StaticClass()->GetSuperClass(); C = C->GetSuperClass())
				{
					SearchClasses.AddUnique(C);
				}
			}
		}
	}

	return SearchClasses;
}

// ============================================
// ResolveFunction — clean exact match only
// If function not found, AI should use list_functions to discover the real name.
// ============================================
UFunction* FBlueprintNodeHandler::ResolveFunction(UBlueprint* Blueprint, const FString& FuncName) const
{
	TArray<UClass*> SearchClasses = GetSearchClasses(Blueprint);

	for (UClass* SearchClass : SearchClasses)
	{
		UFunction* Func = SearchClass->FindFunctionByName(FName(*FuncName));
		if (Func)
		{
			UE_LOG(LogTemp, Log, TEXT("[UE5AI] ResolveFunction: Found '%s' in '%s'"), *FuncName, *SearchClass->GetName());
			return Func;
		}
	}

	UE_LOG(LogTemp, Log, TEXT("[UE5AI] ResolveFunction: '%s' not found in libraries, will use SetSelfMember"), *FuncName);
	return nullptr;
}

// ============================================
// Create a CallFunction node with proper function resolution
// ============================================
UK2Node_CallFunction* FBlueprintNodeHandler::CreateCallFunctionNode(
	UEdGraph* Graph, UBlueprint* Blueprint, const FString& FuncName, int32 PosX, int32 PosY) const
{
	UK2Node_CallFunction* CallNode = NewObject<UK2Node_CallFunction>(Graph);

	UFunction* FoundFunc = ResolveFunction(Blueprint, FuncName);

	if (FoundFunc)
	{
		UClass* OwnerClass = FoundFunc->GetOwnerClass();
		if (!OwnerClass)
		{
			// Safety: OwnerClass should never be null, but guard against it
			UE_LOG(LogTemp, Error, TEXT("[UE5AI] CallFunction '%s': FoundFunc has null OwnerClass, falling back to SelfMember"), *FuncName);
			CallNode->FunctionReference.SetSelfMember(FName(*FuncName));
		}
		else
		{
			CallNode->FunctionReference.SetExternalMember(FoundFunc->GetFName(), OwnerClass);
			UE_LOG(LogTemp, Log, TEXT("[UE5AI] CallFunction: Found '%s' in '%s'"), 
				*FoundFunc->GetName(), *OwnerClass->GetName());
		}
	}
	else
	{
		// Blueprint's own function — use SetSelfMember
		CallNode->FunctionReference.SetSelfMember(FName(*FuncName));
		UE_LOG(LogTemp, Log, TEXT("[UE5AI] CallFunction: '%s' -> SetSelfMember (blueprint function)"), *FuncName);
	}

	CallNode->NodePosX = PosX;
	CallNode->NodePosY = PosY;
	Graph->AddNode(CallNode, /*bUserAction=*/ true, /*bSelectNode=*/ false);
	CallNode->AllocateDefaultPins();

	// Safety net: ReconstructNode if AllocateDefaultPins produced nothing
	if (CallNode->Pins.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[UE5AI] CallFunction '%s': 0 pins after AllocateDefaultPins, trying ReconstructNode"), *FuncName);
		CallNode->ReconstructNode();
	}

	// If STILL 0 pins after reconstruct, remove the invalid node from graph to prevent crashes
	if (CallNode->Pins.Num() == 0)
	{
		FString MemberName = CallNode->FunctionReference.GetMemberName().ToString();
		UClass* MemberParent = CallNode->FunctionReference.GetMemberParentClass();
		FString ParentName = MemberParent ? MemberParent->GetName() : TEXT("null");
		UE_LOG(LogTemp, Error, TEXT("[UE5AI] CallFunction '%s': STILL 0 pins! Removing invalid node. FuncRef: member='%s', parent='%s', isSelf=%d"),
			*FuncName, *MemberName, *ParentName,
			CallNode->FunctionReference.IsSelfContext() ? 1 : 0);

		// Remove the invalid node to prevent subsequent crashes during layout/compile
		Graph->RemoveNode(CallNode);
		return nullptr;
	}

	UE_LOG(LogTemp, Log, TEXT("[UE5AI] CallFunction '%s': %d pins created"), *FuncName, CallNode->Pins.Num());
	return CallNode;
}

// ============================================
// Find a pin on a node by name (case-insensitive, checks both PinName and DisplayName)
// ============================================
UEdGraphPin* FBlueprintNodeHandler::FindPinByName(UEdGraphNode* Node, const FString& PinName) const
{
	if (!Node) return nullptr;

	// Exact match first (case-insensitive)
	for (UEdGraphPin* Pin : Node->Pins)
	{
		if (Pin && (Pin->PinName.ToString().Equals(PinName, ESearchCase::IgnoreCase) ||
					Pin->GetDisplayName().ToString().Equals(PinName, ESearchCase::IgnoreCase)))
		{
			return Pin;
		}
	}

	// Fuzzy match: contains search (for partial names like "A" matching "A" or "self" matching "self")
	for (UEdGraphPin* Pin : Node->Pins)
	{
		if (Pin && Pin->PinName.ToString().Contains(PinName, ESearchCase::IgnoreCase))
		{
			return Pin;
		}
	}

	return nullptr;
}

// ============================================
// Build a detailed error message listing all available pins on a node
// ============================================
FString FBlueprintNodeHandler::BuildPinListError(UEdGraphNode* Node, const FString& RequestedPin, const FString& Context) const
{
	TArray<FString> PinDescriptions;
	for (UEdGraphPin* Pin : Node->Pins)
	{
		if (Pin)
		{
			PinDescriptions.Add(FString::Printf(TEXT("'%s' (%s, %s)"),
				*Pin->PinName.ToString(),
				Pin->Direction == EGPD_Input ? TEXT("in") : TEXT("out"),
				*Pin->PinType.PinCategory.ToString()));
		}
	}

	FString PinList = PinDescriptions.Num() > 0
		? FString::Join(PinDescriptions, TEXT(", "))
		: TEXT("(node has 0 pins - node may be invalid)");

	return FString::Printf(TEXT("Pin '%s' not found on %s (title: '%s', %d pins). Available: [%s]"),
		*RequestedPin,
		*Context,
		*Node->GetNodeTitle(ENodeTitleType::ListView).ToString(),
		Node->Pins.Num(),
		*PinList);
}

// ============================================
// add_node
// ============================================
FCommandResult FBlueprintNodeHandler::HandleAddNode(const TSharedPtr<FJsonObject>& Args)
{
	FString BPName;
	if (!Args->TryGetStringField(TEXT("blueprint_name"), BPName))
	{
		return FCommandResult::Fail(TEXT("Missing required 'blueprint_name' argument"));
	}

	FString NodeType;
	if (!Args->TryGetStringField(TEXT("node_type"), NodeType))
	{
		return FCommandResult::Fail(TEXT("Missing required 'node_type' argument"));
	}

	UBlueprint* Blueprint = FindBlueprintByName(BPName);
	if (!Blueprint)
	{
		return FCommandResult::Fail(FString::Printf(TEXT("Blueprint '%s' not found"), *BPName));
	}

	FString GraphName;
	if (!Args->TryGetStringField(TEXT("graph_name"), GraphName))
	{
		GraphName = TEXT("EventGraph");
	}

	UEdGraph* Graph = FindGraphInBlueprint(Blueprint, GraphName);
	if (!Graph)
	{
		return FCommandResult::Fail(FString::Printf(TEXT("Graph '%s' not found in Blueprint '%s'"), *GraphName, *BPName));
	}

	// Smart position: if not provided, auto-place to the right of existing nodes
	int32 PosX = 0, PosY = 0;
	bool bHasPosX = Args->TryGetNumberField(TEXT("position_x"), PosX);
	bool bHasPosY = Args->TryGetNumberField(TEXT("position_y"), PosY);
	if (!bHasPosX && !bHasPosY)
	{
		ComputeSmartPosition(Graph, PosX, PosY);
	}

	UEdGraphNode* NewNode = nullptr;

	// ---- Alias map: short name → canonical K2Node class name ----
	static TMap<FString, FString> AliasMap;
	if (AliasMap.Num() == 0)
	{
		AliasMap.Add(TEXT("CALLFUNCTION"), TEXT("K2Node_CallFunction"));
		AliasMap.Add(TEXT("FUNCTION"),     TEXT("K2Node_CallFunction"));
		AliasMap.Add(TEXT("CALL"),         TEXT("K2Node_CallFunction"));
		AliasMap.Add(TEXT("EVENT"),        TEXT("K2Node_Event"));
		AliasMap.Add(TEXT("CUSTOMEVENT"),  TEXT("K2Node_CustomEvent"));
		AliasMap.Add(TEXT("CUSTOM_EVENT"), TEXT("K2Node_CustomEvent"));
		AliasMap.Add(TEXT("BRANCH"),       TEXT("K2Node_IfThenElse"));
		AliasMap.Add(TEXT("IF"),           TEXT("K2Node_IfThenElse"));
		AliasMap.Add(TEXT("SEQUENCE"),     TEXT("K2Node_ExecutionSequence"));
		AliasMap.Add(TEXT("VARIABLEGET"),  TEXT("K2Node_VariableGet"));
		AliasMap.Add(TEXT("GETVAR"),       TEXT("K2Node_VariableGet"));
		AliasMap.Add(TEXT("GET"),          TEXT("K2Node_VariableGet"));
		AliasMap.Add(TEXT("VARIABLESET"),  TEXT("K2Node_VariableSet"));
		AliasMap.Add(TEXT("SETVAR"),       TEXT("K2Node_VariableSet"));
		AliasMap.Add(TEXT("SET"),          TEXT("K2Node_VariableSet"));
		AliasMap.Add(TEXT("DELAY"),        TEXT("K2Node_CallFunction")); // special: auto-resolved
		AliasMap.Add(TEXT("PRINT"),        TEXT("K2Node_CallFunction")); // special: auto-resolved
		AliasMap.Add(TEXT("PRINTSTRING"),  TEXT("K2Node_CallFunction")); // special: auto-resolved
		AliasMap.Add(TEXT("SPAWNACTOR"),   TEXT("K2Node_SpawnActorFromClass"));
		AliasMap.Add(TEXT("SPAWN"),        TEXT("K2Node_SpawnActorFromClass"));
	}

	// Resolve the node type: alias → K2Node class name
	FString TypeUpper = NodeType.ToUpper();
	FString ResolvedClassName;
	if (const FString* AliasClass = AliasMap.Find(TypeUpper))
	{
		ResolvedClassName = *AliasClass;
	}
	else
	{
		// Direct K2Node class name (e.g. "K2Node_SwitchEnum", "K2Node_Timeline")
		ResolvedClassName = NodeType;
		// Add prefix if user just said the short name
		if (!ResolvedClassName.StartsWith(TEXT("K2Node_")))
		{
			ResolvedClassName = TEXT("K2Node_") + ResolvedClassName;
		}
	}

	// ---- Special-case nodes that need extra initialization ----

	// CallFunction: needs function_name resolution
	if (ResolvedClassName == TEXT("K2Node_CallFunction"))
	{
		// Handle shortcut aliases (Delay, Print)
		FString FuncName;
		if (TypeUpper == TEXT("DELAY"))
		{
			FuncName = TEXT("Delay");
		}
		else if (TypeUpper == TEXT("PRINT") || TypeUpper == TEXT("PRINTSTRING"))
		{
			FuncName = TEXT("PrintString");
		}
		else if (!Args->TryGetStringField(TEXT("function_name"), FuncName))
		{
			return FCommandResult::Fail(TEXT("CallFunction node requires 'function_name' argument. Use list_functions to discover available functions."));
		}
		NewNode = CreateCallFunctionNode(Graph, Blueprint, FuncName, PosX, PosY);
		if (!NewNode)
		{
			return FCommandResult::Fail(FString::Printf(
				TEXT("Function '%s' could not be resolved and produced an invalid node (0 pins). "
				     "Use list_functions to find the correct C++ function name."), *FuncName));
		}

		// If 'target' is specified, auto-connect the Target/self pin to a component variable getter
		// This allows calling component functions like CharMoveComp->SetMaxWalkSpeed()
		FString TargetComp;
		if (Args->TryGetStringField(TEXT("target"), TargetComp))
		{
			UK2Node_CallFunction* CallNode = Cast<UK2Node_CallFunction>(NewNode);
			if (CallNode)
			{
				// Find the "self" or "Target" pin on the function node
				UEdGraphPin* TargetPin = FindPinByName(CallNode, TEXT("self"));
				if (!TargetPin) TargetPin = FindPinByName(CallNode, TEXT("Target"));

				if (TargetPin && TargetPin->Direction == EGPD_Input)
				{
					// Create a VariableGet node for the component
					UK2Node_VariableGet* GetVarNode = NewObject<UK2Node_VariableGet>(Graph);
					GetVarNode->VariableReference.SetSelfMember(FName(*TargetComp));
					GetVarNode->NodePosX = PosX - 250;
					GetVarNode->NodePosY = PosY;
					Graph->AddNode(GetVarNode, true, false);
					GetVarNode->AllocateDefaultPins();

					if (GetVarNode->Pins.Num() > 0)
					{
						// Find the output pin on the getter (usually the variable name pin)
						UEdGraphPin* CompOutPin = nullptr;
						for (UEdGraphPin* Pin : GetVarNode->Pins)
						{
							if (Pin && Pin->Direction == EGPD_Output)
							{
								CompOutPin = Pin;
								break;
							}
						}
						if (CompOutPin)
						{
							TargetPin->MakeLinkTo(CompOutPin);
							UE_LOG(LogTemp, Log, TEXT("[UE5AI] CallFunction '%s': auto-connected target '%s' to self pin"), *FuncName, *TargetComp);
						}
					}
					else
					{
						UE_LOG(LogTemp, Warning, TEXT("[UE5AI] CallFunction '%s': target variable '%s' produced 0 pins"), *FuncName, *TargetComp);
					}
				}
			}
		}
	}
	// Event: needs event_name
	else if (ResolvedClassName == TEXT("K2Node_Event"))
	{
		FString EventName;
		if (!Args->TryGetStringField(TEXT("event_name"), EventName))
		{
			return FCommandResult::Fail(TEXT("Event node requires 'event_name' argument (e.g. BeginPlay, Tick, ActorBeginOverlap)"));
		}

		// Resolve the actual UFunction for this event in the Blueprint's class hierarchy.
		// UE5 blueprint events use "Receive" prefix (e.g. Tick -> ReceiveTick, BeginPlay -> ReceiveBeginPlay).
		// We search dynamically instead of hardcoding so any event (including Character-specific ones) works.
		UClass* OwnerClass = Blueprint->ParentClass ? (UClass*)Blueprint->ParentClass : AActor::StaticClass();
		UFunction* EventFunc = nullptr;

		// Try variations: exact name, "Receive" + name
		TArray<FString> Candidates = { EventName, TEXT("Receive") + EventName };
		for (const FString& Candidate : Candidates)
		{
			for (UClass* C = OwnerClass; C; C = C->GetSuperClass())
			{
				EventFunc = C->FindFunctionByName(FName(*Candidate));
				if (EventFunc && EventFunc->HasAnyFunctionFlags(FUNC_BlueprintEvent))
				{
					OwnerClass = C;
					break;
				}
				EventFunc = nullptr;
			}
			if (EventFunc) break;
		}

		UK2Node_Event* EventNode = NewObject<UK2Node_Event>(Graph);

		if (EventFunc)
		{
			// Use the resolved function — this ensures all parameters (e.g. DeltaSeconds for Tick) are exposed as pins
			EventNode->EventReference.SetExternalMember(EventFunc->GetFName(), OwnerClass);
			UE_LOG(LogTemp, Log, TEXT("[UE5AI] Event: Resolved event with function reference"));
		}
		else
		{
			// Fallback for custom/unknown events
			EventNode->EventReference.SetExternalMember(FName(*EventName), OwnerClass);
			UE_LOG(LogTemp, Warning, TEXT("[UE5AI] Event: Could not resolve as UFunction, using raw name"));
		}

		EventNode->bOverrideFunction = true;
		EventNode->NodePosX = PosX;
		EventNode->NodePosY = PosY;
		Graph->AddNode(EventNode, true, false);
		EventNode->AllocateDefaultPins();
		NewNode = EventNode;
	}
	// CustomEvent: optional event_name
	else if (ResolvedClassName == TEXT("K2Node_CustomEvent"))
	{
		FString EventName;
		if (!Args->TryGetStringField(TEXT("event_name"), EventName))
		{
			EventName = TEXT("MyCustomEvent");
		}

		UK2Node_CustomEvent* CustomEventNode = NewObject<UK2Node_CustomEvent>(Graph);
		CustomEventNode->CustomFunctionName = FName(*EventName);
		CustomEventNode->NodePosX = PosX;
		CustomEventNode->NodePosY = PosY;
		Graph->AddNode(CustomEventNode, true, false);
		CustomEventNode->AllocateDefaultPins();
		NewNode = CustomEventNode;
	}
	// VariableGet / VariableSet: need variable_name
	else if (ResolvedClassName == TEXT("K2Node_VariableGet"))
	{
		FString VarName;
		if (!Args->TryGetStringField(TEXT("variable_name"), VarName))
		{
			return FCommandResult::Fail(TEXT("VariableGet node requires 'variable_name' argument"));
		}

		UK2Node_VariableGet* GetNode = NewObject<UK2Node_VariableGet>(Graph);
		GetNode->VariableReference.SetSelfMember(FName(*VarName));
		GetNode->NodePosX = PosX;
		GetNode->NodePosY = PosY;
		Graph->AddNode(GetNode, true, false);
		GetNode->AllocateDefaultPins();
		NewNode = GetNode;
	}
	else if (ResolvedClassName == TEXT("K2Node_VariableSet"))
	{
		FString VarName;
		if (!Args->TryGetStringField(TEXT("variable_name"), VarName))
		{
			return FCommandResult::Fail(TEXT("VariableSet node requires 'variable_name' argument"));
		}

		UK2Node_VariableSet* SetNode = NewObject<UK2Node_VariableSet>(Graph);
		SetNode->VariableReference.SetSelfMember(FName(*VarName));
		SetNode->NodePosX = PosX;
		SetNode->NodePosY = PosY;
		Graph->AddNode(SetNode, true, false);
		SetNode->AllocateDefaultPins();
		NewNode = SetNode;
	}
	// ---- Generic K2Node creation path ----
	else
	{
		// Find the UClass by name via UObject reflection
		FString FullClassName = TEXT("Class'/Script/BlueprintGraph.") + ResolvedClassName + TEXT("'");
		UClass* NodeClass = FindObject<UClass>(nullptr, *FullClassName);

		// Fallback: search in other modules (Engine, UnrealEd, etc.)
		if (!NodeClass)
		{
			// Try common module paths
			TArray<FString> ModulePaths = {
				TEXT("Class'/Script/BlueprintGraph.") + ResolvedClassName + TEXT("'"),
				TEXT("Class'/Script/UnrealEd.") + ResolvedClassName + TEXT("'"),
				TEXT("Class'/Script/Engine.") + ResolvedClassName + TEXT("'"),
			};

			for (const FString& Path : ModulePaths)
			{
				NodeClass = FindObject<UClass>(nullptr, *Path);
				if (NodeClass) break;
			}
		}

		// Last resort: iterate all classes
		if (!NodeClass)
		{
			for (TObjectIterator<UClass> It; It; ++It)
			{
				if (It->GetName() == ResolvedClassName && It->IsChildOf(UK2Node::StaticClass()))
				{
					NodeClass = *It;
					break;
				}
			}
		}

		if (!NodeClass)
		{
			return FCommandResult::Fail(FString::Printf(
				TEXT("K2Node class '%s' not found. Use list_node_types to discover available types."),
				*ResolvedClassName));
		}

		if (!NodeClass->IsChildOf(UK2Node::StaticClass()))
		{
			return FCommandResult::Fail(FString::Printf(
				TEXT("'%s' is not a K2Node subclass."),
				*ResolvedClassName));
		}

		UEdGraphNode* GenericNode = NewObject<UEdGraphNode>(Graph, NodeClass);
		GenericNode->NodePosX = PosX;
		GenericNode->NodePosY = PosY;
		Graph->AddNode(GenericNode, true, false);
		GenericNode->AllocateDefaultPins();
		NewNode = GenericNode;
	}

	if (!NewNode)
	{
		return FCommandResult::Fail(TEXT("Failed to create node"));
	}

	// Ensure the node has a valid GUID
	if (!NewNode->NodeGuid.IsValid())
	{
		NewNode->CreateNewGuid();
	}

	// Post-creation: handle Sequence output_count (adds extra Then pins)
	if (UK2Node_ExecutionSequence* SeqNode = Cast<UK2Node_ExecutionSequence>(NewNode))
	{
		int32 OutputCount = 2; // default
		if (Args->TryGetNumberField(TEXT("output_count"), OutputCount) && OutputCount > 2)
		{
			// Sequence starts with 2 outputs by default, add more as needed
			for (int32 i = 2; i < OutputCount; ++i)
			{
				SeqNode->AddInputPin();
			}
			UE_LOG(LogTemp, Log, TEXT("[UE5AI] Sequence node: expanded to %d outputs"), OutputCount);
		}
	}

	Blueprint->MarkPackageDirty();
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);

	// Return full node info including all pins
	TSharedPtr<FJsonObject> Data = NodeToJson(NewNode);
	Data->SetStringField(TEXT("blueprintName"), BPName);
	Data->SetStringField(TEXT("graphName"), GraphName);
	Data->SetStringField(TEXT("message"), FString::Printf(TEXT("Added %s node to '%s.%s'"), *NodeType, *BPName, *GraphName));

	return FCommandResult::Success(Data);
}

// ============================================
// connect_nodes — with improved error reporting
// ============================================
FCommandResult FBlueprintNodeHandler::HandleConnectNodes(const TSharedPtr<FJsonObject>& Args)
{
	FString BPName;
	if (!Args->TryGetStringField(TEXT("blueprint_name"), BPName))
		return FCommandResult::Fail(TEXT("Missing required 'blueprint_name' argument"));

	FString FromNodeId, FromPinName, ToNodeId, ToPinName;
	if (!Args->TryGetStringField(TEXT("from_node_id"), FromNodeId))
		return FCommandResult::Fail(TEXT("Missing required 'from_node_id' argument"));
	if (!Args->TryGetStringField(TEXT("from_pin"), FromPinName))
		return FCommandResult::Fail(TEXT("Missing required 'from_pin' argument"));
	if (!Args->TryGetStringField(TEXT("to_node_id"), ToNodeId))
		return FCommandResult::Fail(TEXT("Missing required 'to_node_id' argument"));
	if (!Args->TryGetStringField(TEXT("to_pin"), ToPinName))
		return FCommandResult::Fail(TEXT("Missing required 'to_pin' argument"));

	UBlueprint* Blueprint = FindBlueprintByName(BPName);
	if (!Blueprint)
		return FCommandResult::Fail(FString::Printf(TEXT("Blueprint '%s' not found"), *BPName));

	FString GraphName;
	if (!Args->TryGetStringField(TEXT("graph_name"), GraphName))
		GraphName = TEXT("EventGraph");

	UEdGraph* Graph = FindGraphInBlueprint(Blueprint, GraphName);
	if (!Graph)
		return FCommandResult::Fail(FString::Printf(TEXT("Graph '%s' not found"), *GraphName));

	// Find source node and pin
	UEdGraphNode* FromNode = FindNodeByGuid(Graph, FromNodeId);
	if (!FromNode)
		return FCommandResult::Fail(FString::Printf(TEXT("Source node '%s' not found"), *FromNodeId));

	UEdGraphPin* FromPin = FindPinByName(FromNode, FromPinName);
	if (!FromPin)
		return FCommandResult::Fail(BuildPinListError(FromNode, FromPinName, TEXT("source node")));

	// Find target node and pin
	UEdGraphNode* ToNode = FindNodeByGuid(Graph, ToNodeId);
	if (!ToNode)
		return FCommandResult::Fail(FString::Printf(TEXT("Target node '%s' not found"), *ToNodeId));

	UEdGraphPin* ToPin = FindPinByName(ToNode, ToPinName);
	if (!ToPin)
		return FCommandResult::Fail(BuildPinListError(ToNode, ToPinName, TEXT("target node")));

	// Try to connect
	const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();
	bool bConnected = Schema->TryCreateConnection(FromPin, ToPin);

	if (!bConnected)
	{
		// Provide detailed failure reason
		FString Reason;
		if (FromPin->Direction == ToPin->Direction)
		{
			Reason = FString::Printf(TEXT("Both pins are %s — need one input and one output"),
				FromPin->Direction == EGPD_Input ? TEXT("inputs") : TEXT("outputs"));
		}
		else if (FromPin->PinType.PinCategory != ToPin->PinType.PinCategory &&
				 FromPin->PinType.PinCategory != UEdGraphSchema_K2::PC_Exec &&
				 ToPin->PinType.PinCategory != UEdGraphSchema_K2::PC_Exec)
		{
			Reason = FString::Printf(TEXT("Type mismatch: '%s' (%s) vs '%s' (%s)"),
				*FromPin->PinName.ToString(), *FromPin->PinType.PinCategory.ToString(),
				*ToPin->PinName.ToString(), *ToPin->PinType.PinCategory.ToString());
		}
		else
		{
			Reason = TEXT("Pins are incompatible (check types and directions)");
		}

		return FCommandResult::Fail(FString::Printf(TEXT("Failed to connect '%s.%s' -> '%s.%s': %s"),
			*FromNodeId, *FromPinName, *ToNodeId, *ToPinName, *Reason));
	}

	Blueprint->MarkPackageDirty();
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("from"), FString::Printf(TEXT("%s.%s"), *FromNodeId, *FromPinName));
	Data->SetStringField(TEXT("to"), FString::Printf(TEXT("%s.%s"), *ToNodeId, *ToPinName));
	Data->SetStringField(TEXT("message"), FString::Printf(TEXT("Connected %s.%s -> %s.%s"), *FromNodeId, *FromPinName, *ToNodeId, *ToPinName));

	return FCommandResult::Success(Data);
}

// ============================================
// disconnect_nodes — break a connection between two pins
// Args: { "blueprint_name", "graph_name"?, "node_id", "pin_name", "target_node_id"?, "target_pin"? }
// If target_node_id+target_pin provided: break that specific connection.
// If only node_id+pin_name: break ALL connections on that pin.
// ============================================
FCommandResult FBlueprintNodeHandler::HandleDisconnectNodes(const TSharedPtr<FJsonObject>& Args)
{
	FString BPName;
	if (!Args->TryGetStringField(TEXT("blueprint_name"), BPName))
		return FCommandResult::Fail(TEXT("Missing required 'blueprint_name' argument"));

	FString NodeId, PinName;
	if (!Args->TryGetStringField(TEXT("node_id"), NodeId))
		return FCommandResult::Fail(TEXT("Missing required 'node_id' argument"));
	if (!Args->TryGetStringField(TEXT("pin_name"), PinName))
		return FCommandResult::Fail(TEXT("Missing required 'pin_name' argument"));

	UBlueprint* Blueprint = FindBlueprintByName(BPName);
	if (!Blueprint)
		return FCommandResult::Fail(FString::Printf(TEXT("Blueprint '%s' not found"), *BPName));

	FString GraphName;
	if (!Args->TryGetStringField(TEXT("graph_name"), GraphName))
		GraphName = TEXT("EventGraph");

	UEdGraph* Graph = FindGraphInBlueprint(Blueprint, GraphName);
	if (!Graph)
		return FCommandResult::Fail(FString::Printf(TEXT("Graph '%s' not found"), *GraphName));

	UEdGraphNode* Node = FindNodeByGuid(Graph, NodeId);
	if (!Node)
		return FCommandResult::Fail(FString::Printf(TEXT("Node '%s' not found"), *NodeId));

	UEdGraphPin* Pin = FindPinByName(Node, PinName);
	if (!Pin)
		return FCommandResult::Fail(BuildPinListError(Node, PinName, TEXT("node")));

	FString TargetNodeId, TargetPinName;
	bool bHasTarget = Args->TryGetStringField(TEXT("target_node_id"), TargetNodeId) &&
	                  Args->TryGetStringField(TEXT("target_pin"), TargetPinName);

	int32 BrokenCount = 0;

	if (bHasTarget)
	{
		// Break a specific connection
		UEdGraphNode* TargetNode = FindNodeByGuid(Graph, TargetNodeId);
		if (!TargetNode)
			return FCommandResult::Fail(FString::Printf(TEXT("Target node '%s' not found"), *TargetNodeId));

		UEdGraphPin* TargetPin = FindPinByName(TargetNode, TargetPinName);
		if (!TargetPin)
			return FCommandResult::Fail(BuildPinListError(TargetNode, TargetPinName, TEXT("target node")));

		if (Pin->LinkedTo.Contains(TargetPin))
		{
			Pin->BreakLinkTo(TargetPin);
			BrokenCount = 1;
		}
		else
		{
			return FCommandResult::Fail(FString::Printf(
				TEXT("No connection exists between '%s.%s' and '%s.%s'"),
				*NodeId, *PinName, *TargetNodeId, *TargetPinName));
		}
	}
	else
	{
		// Break ALL connections on this pin
		BrokenCount = Pin->LinkedTo.Num();
		Pin->BreakAllPinLinks();
	}

	Blueprint->MarkPackageDirty();
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetNumberField(TEXT("brokenConnections"), BrokenCount);
	Data->SetStringField(TEXT("pin"), FString::Printf(TEXT("%s.%s"), *NodeId, *PinName));
	Data->SetStringField(TEXT("message"), FString::Printf(
		TEXT("Disconnected %d connection(s) from %s.%s"), BrokenCount, *NodeId, *PinName));

	return FCommandResult::Success(Data);
}

// ============================================
// remove_node
// ============================================
FCommandResult FBlueprintNodeHandler::HandleRemoveNode(const TSharedPtr<FJsonObject>& Args)
{
	FString BPName, NodeId;
	if (!Args->TryGetStringField(TEXT("blueprint_name"), BPName))
		return FCommandResult::Fail(TEXT("Missing 'blueprint_name'"));
	if (!Args->TryGetStringField(TEXT("node_id"), NodeId))
		return FCommandResult::Fail(TEXT("Missing 'node_id'"));

	UBlueprint* Blueprint = FindBlueprintByName(BPName);
	if (!Blueprint)
		return FCommandResult::Fail(FString::Printf(TEXT("Blueprint '%s' not found"), *BPName));

	FString GraphName;
	if (!Args->TryGetStringField(TEXT("graph_name"), GraphName))
		GraphName = TEXT("EventGraph");

	UEdGraph* Graph = FindGraphInBlueprint(Blueprint, GraphName);
	if (!Graph)
		return FCommandResult::Fail(FString::Printf(TEXT("Graph '%s' not found"), *GraphName));

	UEdGraphNode* Node = FindNodeByGuid(Graph, NodeId);
	if (!Node)
		return FCommandResult::Fail(FString::Printf(TEXT("Node '%s' not found"), *NodeId));

	FString NodeTitle = Node->GetNodeTitle(ENodeTitleType::ListView).ToString();
	Graph->RemoveNode(Node);

	Blueprint->MarkPackageDirty();
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);

	return FCommandResult::SuccessMessage(FString::Printf(TEXT("Removed node '%s' (%s)"), *NodeId, *NodeTitle));
}

// ============================================
// get_node_pins — now uses unified PinToJson
// ============================================
FCommandResult FBlueprintNodeHandler::HandleGetNodePins(const TSharedPtr<FJsonObject>& Args)
{
	FString BPName, NodeId;
	if (!Args->TryGetStringField(TEXT("blueprint_name"), BPName))
		return FCommandResult::Fail(TEXT("Missing 'blueprint_name'"));
	if (!Args->TryGetStringField(TEXT("node_id"), NodeId))
		return FCommandResult::Fail(TEXT("Missing 'node_id'"));

	UBlueprint* Blueprint = FindBlueprintByName(BPName);
	if (!Blueprint)
		return FCommandResult::Fail(FString::Printf(TEXT("Blueprint '%s' not found"), *BPName));

	FString GraphName;
	if (!Args->TryGetStringField(TEXT("graph_name"), GraphName))
		GraphName = TEXT("EventGraph");

	UEdGraph* Graph = FindGraphInBlueprint(Blueprint, GraphName);
	if (!Graph)
		return FCommandResult::Fail(FString::Printf(TEXT("Graph '%s' not found"), *GraphName));

	UEdGraphNode* Node = FindNodeByGuid(Graph, NodeId);
	if (!Node)
		return FCommandResult::Fail(FString::Printf(TEXT("Node '%s' not found"), *NodeId));

	TArray<TSharedPtr<FJsonValue>> PinArray;
	for (UEdGraphPin* Pin : Node->Pins)
	{
		if (Pin)
		{
			PinArray.Add(MakeShared<FJsonValueObject>(PinToJson(Pin)));
		}
	}

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("nodeId"), NodeId);
	Data->SetStringField(TEXT("nodeTitle"), Node->GetNodeTitle(ENodeTitleType::ListView).ToString());
	Data->SetArrayField(TEXT("pins"), PinArray);
	Data->SetNumberField(TEXT("pinCount"), PinArray.Num());

	return FCommandResult::Success(Data);
}

// ============================================
// set_pin_default
// ============================================
FCommandResult FBlueprintNodeHandler::HandleSetPinDefault(const TSharedPtr<FJsonObject>& Args)
{
	FString BPName, NodeId, PinName, Value;
	if (!Args->TryGetStringField(TEXT("blueprint_name"), BPName))
		return FCommandResult::Fail(TEXT("Missing 'blueprint_name'"));
	if (!Args->TryGetStringField(TEXT("node_id"), NodeId))
		return FCommandResult::Fail(TEXT("Missing 'node_id'"));
	if (!Args->TryGetStringField(TEXT("pin_name"), PinName))
		return FCommandResult::Fail(TEXT("Missing 'pin_name'"));
	if (!Args->TryGetStringField(TEXT("value"), Value))
		return FCommandResult::Fail(TEXT("Missing 'value'"));

	UBlueprint* Blueprint = FindBlueprintByName(BPName);
	if (!Blueprint)
		return FCommandResult::Fail(FString::Printf(TEXT("Blueprint '%s' not found"), *BPName));

	FString GraphName;
	if (!Args->TryGetStringField(TEXT("graph_name"), GraphName))
		GraphName = TEXT("EventGraph");

	UEdGraph* Graph = FindGraphInBlueprint(Blueprint, GraphName);
	if (!Graph)
		return FCommandResult::Fail(FString::Printf(TEXT("Graph '%s' not found"), *GraphName));

	UEdGraphNode* Node = FindNodeByGuid(Graph, NodeId);
	if (!Node)
		return FCommandResult::Fail(FString::Printf(TEXT("Node '%s' not found"), *NodeId));

	UEdGraphPin* TargetPin = FindPinByName(Node, PinName);
	if (!TargetPin)
		return FCommandResult::Fail(BuildPinListError(Node, PinName, TEXT("node")));

	const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();
	Schema->TrySetDefaultValue(*TargetPin, Value);

	Blueprint->MarkPackageDirty();

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("nodeId"), NodeId);
	Data->SetStringField(TEXT("pinName"), PinName);
	Data->SetStringField(TEXT("value"), Value);
	Data->SetStringField(TEXT("message"), FString::Printf(TEXT("Set pin '%s' default to '%s'"), *PinName, *Value));

	return FCommandResult::Success(Data);
}

// ============================================
// batch_execute — execute multiple operations in a single call
// Args: {
//   "blueprint_name": "BP_MyActor",
//   "graph_name": "EventGraph",
//   "operations": [
//     { "op": "add_node", "ref": "n1", "node_type": "VariableSet", "variable_name": "bIsSprinting" },
//     { "op": "set_default", "ref": "n1", "pin": "bIsSprinting", "value": "true" },
//     { "op": "add_node", "ref": "n2", "node_type": "CallFunction", "function_name": "UpdateMovementSpeed" },
//     { "op": "connect", "from": "entry", "from_pin": "then", "to": "n1", "to_pin": "execute" },
//     { "op": "connect", "from": "n1", "from_pin": "then", "to": "n2", "to_pin": "execute" }
//   ],
//   "auto_layout": true  (optional, default true)
// }
// "ref" in add_node: assigns a local alias; "entry" is special — refers to FunctionEntry node
// ============================================
FCommandResult FBlueprintNodeHandler::HandleBatchExecute(const TSharedPtr<FJsonObject>& Args)
{
	FString BPName;
	if (!Args->TryGetStringField(TEXT("blueprint_name"), BPName))
		return FCommandResult::Fail(TEXT("Missing 'blueprint_name'"));

	UBlueprint* Blueprint = FindBlueprintByName(BPName);
	if (!Blueprint)
		return FCommandResult::Fail(FString::Printf(TEXT("Blueprint '%s' not found"), *BPName));

	FString GraphName;
	if (!Args->TryGetStringField(TEXT("graph_name"), GraphName))
		GraphName = TEXT("EventGraph");

	UEdGraph* Graph = FindGraphInBlueprint(Blueprint, GraphName);
	if (!Graph)
		return FCommandResult::Fail(FString::Printf(TEXT("Graph '%s' not found"), *GraphName));

	const TArray<TSharedPtr<FJsonValue>>* OpsArray = nullptr;
	if (!Args->TryGetArrayField(TEXT("operations"), OpsArray) || !OpsArray)
		return FCommandResult::Fail(TEXT("Missing 'operations' array"));

	// Ref map: alias -> node GUID
	TMap<FString, FString> RefToGuid;

	// Pre-populate "entry" ref if this is a function graph
	for (UEdGraphNode* Node : Graph->Nodes)
	{
		if (Node && Node->IsA<UK2Node_FunctionEntry>())
		{
			RefToGuid.Add(TEXT("entry"), Node->NodeGuid.ToString());
			break;
		}
	}

	// Also map event nodes by their title (e.g. "beginplay", "tick")
	for (UEdGraphNode* Node : Graph->Nodes)
	{
		if (Node && Node->IsA<UK2Node_Event>())
		{
			FString Title = Node->GetNodeTitle(ENodeTitleType::ListView).ToString().ToLower();
			// Try common names
			if (Title.Contains(TEXT("beginplay")) || Title.Contains(TEXT("开始运行")))
			{
				RefToGuid.Add(TEXT("beginplay"), Node->NodeGuid.ToString());
			}
			if (Title.Contains(TEXT("tick")))
			{
				RefToGuid.Add(TEXT("tick"), Node->NodeGuid.ToString());
			}
			if (Title.Contains(TEXT("overlap")) || Title.Contains(TEXT("重叠")))
			{
				RefToGuid.Add(TEXT("overlap"), Node->NodeGuid.ToString());
			}
		}
	}

	TArray<TSharedPtr<FJsonValue>> ResultArray;
	int32 SuccessCount = 0;
	int32 FailCount = 0;

	auto ResolveRef = [&RefToGuid](const FString& Ref) -> FString
	{
		if (const FString* Found = RefToGuid.Find(Ref.ToLower()))
		{
			return *Found;
		}
		// Might be a raw GUID already
		return Ref;
	};

	for (int32 i = 0; i < OpsArray->Num(); i++)
	{
		const TSharedPtr<FJsonObject>* OpObjPtr = nullptr;
		if (!(*OpsArray)[i]->TryGetObject(OpObjPtr) || !OpObjPtr)
		{
			TSharedPtr<FJsonObject> ErrObj = MakeShared<FJsonObject>();
			ErrObj->SetNumberField(TEXT("index"), i);
			ErrObj->SetBoolField(TEXT("success"), false);
			ErrObj->SetStringField(TEXT("error"), TEXT("Invalid operation object"));
			ResultArray.Add(MakeShared<FJsonValueObject>(ErrObj));
			FailCount++;
			continue;
		}

		const TSharedPtr<FJsonObject>& OpObj = *OpObjPtr;
		FString Op;
		OpObj->TryGetStringField(TEXT("op"), Op);
		Op = Op.ToLower();

		TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
		ResultObj->SetNumberField(TEXT("index"), i);
		ResultObj->SetStringField(TEXT("op"), Op);

		// ---- add_node ----
		if (Op == TEXT("add_node"))
		{
			FString Ref;
			OpObj->TryGetStringField(TEXT("ref"), Ref);

			// Forward ALL fields from the operation to HandleAddNode (except op/ref)
			// This ensures target, output_count, and any future parameters are passed through
			TSharedPtr<FJsonObject> SubArgs = MakeShared<FJsonObject>();
			for (const auto& Pair : OpObj->Values)
			{
				if (Pair.Key != TEXT("op") && Pair.Key != TEXT("ref"))
				{
					SubArgs->SetField(Pair.Key, Pair.Value);
				}
			}
			// Override blueprint_name and graph_name from parent batch context
			SubArgs->SetStringField(TEXT("blueprint_name"), BPName);
			SubArgs->SetStringField(TEXT("graph_name"), GraphName);
			// Don't pass position — let smart positioning handle it
			SubArgs->RemoveField(TEXT("position_x"));
			SubArgs->RemoveField(TEXT("position_y"));

			FCommandResult Result = HandleAddNode(SubArgs);
			ResultObj->SetBoolField(TEXT("success"), Result.bSuccess);

			if (Result.bSuccess && Result.Data.IsValid())
			{
				FString NodeId;
				Result.Data->TryGetStringField(TEXT("id"), NodeId);
				ResultObj->SetStringField(TEXT("id"), NodeId);
				ResultObj->SetStringField(TEXT("title"), Result.Data->GetStringField(TEXT("title")));

				// Compact pin summary: just name+direction list instead of full pin objects
				// This reduces response size dramatically for batch operations
				const TArray<TSharedPtr<FJsonValue>>* PinsPtr = nullptr;
				if (Result.Data->TryGetArrayField(TEXT("pins"), PinsPtr) && PinsPtr)
				{
					ResultObj->SetNumberField(TEXT("pinCount"), PinsPtr->Num());
					TArray<FString> PinNames;
					for (const auto& PinVal : *PinsPtr)
					{
						const TSharedPtr<FJsonObject>* PinObj;
						if (PinVal->TryGetObject(PinObj))
						{
							FString Name, Dir;
							(*PinObj)->TryGetStringField(TEXT("name"), Name);
							(*PinObj)->TryGetStringField(TEXT("direction"), Dir);
							PinNames.Add(FString::Printf(TEXT("%s(%s)"), *Name, *Dir.Left(3)));
						}
					}
					ResultObj->SetStringField(TEXT("pinSummary"), FString::Join(PinNames, TEXT(", ")));
				}

				// Register ref
				if (!Ref.IsEmpty())
				{
					RefToGuid.Add(Ref.ToLower(), NodeId);
					ResultObj->SetStringField(TEXT("ref"), Ref);
				}
				SuccessCount++;
			}
			else
			{
				ResultObj->SetStringField(TEXT("error"), Result.Error);
				FailCount++;
			}
		}
		// ---- connect ----
		else if (Op == TEXT("connect"))
		{
			FString FromRef, FromPin, ToRef, ToPin;
			OpObj->TryGetStringField(TEXT("from"), FromRef);
			OpObj->TryGetStringField(TEXT("from_pin"), FromPin);
			OpObj->TryGetStringField(TEXT("to"), ToRef);
			OpObj->TryGetStringField(TEXT("to_pin"), ToPin);

			TSharedPtr<FJsonObject> SubArgs = MakeShared<FJsonObject>();
			SubArgs->SetStringField(TEXT("blueprint_name"), BPName);
			SubArgs->SetStringField(TEXT("graph_name"), GraphName);
			SubArgs->SetStringField(TEXT("from_node_id"), ResolveRef(FromRef));
			SubArgs->SetStringField(TEXT("from_pin"), FromPin);
			SubArgs->SetStringField(TEXT("to_node_id"), ResolveRef(ToRef));
			SubArgs->SetStringField(TEXT("to_pin"), ToPin);

			FCommandResult Result = HandleConnectNodes(SubArgs);
			ResultObj->SetBoolField(TEXT("success"), Result.bSuccess);
			if (Result.bSuccess)
			{
				ResultObj->SetStringField(TEXT("message"), TEXT("Connected"));
				SuccessCount++;
			}
			else
			{
				ResultObj->SetStringField(TEXT("error"), Result.Error);
				FailCount++;
			}
		}
		// ---- set_default ----
		else if (Op == TEXT("set_default"))
		{
			FString Ref, PinName, Value;
			OpObj->TryGetStringField(TEXT("ref"), Ref);
			// Also support "node_id" directly
			if (Ref.IsEmpty()) OpObj->TryGetStringField(TEXT("node_id"), Ref);
			OpObj->TryGetStringField(TEXT("pin"), PinName);
			OpObj->TryGetStringField(TEXT("value"), Value);

			TSharedPtr<FJsonObject> SubArgs = MakeShared<FJsonObject>();
			SubArgs->SetStringField(TEXT("blueprint_name"), BPName);
			SubArgs->SetStringField(TEXT("graph_name"), GraphName);
			SubArgs->SetStringField(TEXT("node_id"), ResolveRef(Ref));
			SubArgs->SetStringField(TEXT("pin_name"), PinName);
			SubArgs->SetStringField(TEXT("value"), Value);

			FCommandResult Result = HandleSetPinDefault(SubArgs);
			ResultObj->SetBoolField(TEXT("success"), Result.bSuccess);
			if (Result.bSuccess)
			{
				ResultObj->SetStringField(TEXT("message"), FString::Printf(TEXT("Set %s=%s"), *PinName, *Value));
				SuccessCount++;
			}
			else
			{
				ResultObj->SetStringField(TEXT("error"), Result.Error);
				FailCount++;
			}
		}
		// ---- disconnect ----
		else if (Op == TEXT("disconnect"))
		{
			FString Ref, PinName, TargetRef, TargetPin;
			OpObj->TryGetStringField(TEXT("ref"), Ref);
			if (Ref.IsEmpty()) OpObj->TryGetStringField(TEXT("node_id"), Ref);
			OpObj->TryGetStringField(TEXT("pin"), PinName);
			OpObj->TryGetStringField(TEXT("target"), TargetRef);
			OpObj->TryGetStringField(TEXT("target_pin"), TargetPin);

			TSharedPtr<FJsonObject> SubArgs = MakeShared<FJsonObject>();
			SubArgs->SetStringField(TEXT("blueprint_name"), BPName);
			SubArgs->SetStringField(TEXT("graph_name"), GraphName);
			SubArgs->SetStringField(TEXT("node_id"), ResolveRef(Ref));
			SubArgs->SetStringField(TEXT("pin_name"), PinName);
			if (!TargetRef.IsEmpty())
				SubArgs->SetStringField(TEXT("target_node_id"), ResolveRef(TargetRef));
			if (!TargetPin.IsEmpty())
				SubArgs->SetStringField(TEXT("target_pin"), TargetPin);

			FCommandResult Result = HandleDisconnectNodes(SubArgs);
			ResultObj->SetBoolField(TEXT("success"), Result.bSuccess);
			if (Result.bSuccess)
			{
				ResultObj->SetStringField(TEXT("message"), TEXT("Disconnected"));
				SuccessCount++;
			}
			else
			{
				ResultObj->SetStringField(TEXT("error"), Result.Error);
				FailCount++;
			}
		}
		// ---- remove ----
		else if (Op == TEXT("remove"))
		{
			FString Ref;
			OpObj->TryGetStringField(TEXT("ref"), Ref);
			if (Ref.IsEmpty()) OpObj->TryGetStringField(TEXT("node_id"), Ref);

			TSharedPtr<FJsonObject> SubArgs = MakeShared<FJsonObject>();
			SubArgs->SetStringField(TEXT("blueprint_name"), BPName);
			SubArgs->SetStringField(TEXT("graph_name"), GraphName);
			SubArgs->SetStringField(TEXT("node_id"), ResolveRef(Ref));

			FCommandResult Result = HandleRemoveNode(SubArgs);
			ResultObj->SetBoolField(TEXT("success"), Result.bSuccess);
			ResultObj->SetStringField(Result.bSuccess ? TEXT("message") : TEXT("error"),
				Result.bSuccess ? TEXT("Removed") : Result.Error);
			Result.bSuccess ? SuccessCount++ : FailCount++;
		}
		else
		{
			ResultObj->SetBoolField(TEXT("success"), false);
			ResultObj->SetStringField(TEXT("error"), FString::Printf(TEXT("Unknown op '%s'. Use: add_node, connect, disconnect, set_default, remove"), *Op));
			FailCount++;
		}

		ResultArray.Add(MakeShared<FJsonValueObject>(ResultObj));
	}

	// Auto-layout after batch (default: true)
	bool bAutoLayout = true;
	Args->TryGetBoolField(TEXT("auto_layout"), bAutoLayout);
	if (bAutoLayout)
	{
		AutoLayoutGraph(Graph);
	}

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetNumberField(TEXT("totalOps"), OpsArray->Num());
	Data->SetNumberField(TEXT("success"), SuccessCount);
	Data->SetNumberField(TEXT("failed"), FailCount);
	Data->SetArrayField(TEXT("results"), ResultArray);

	// --- Auto-compile & diagnostics (like TypeScript LSP error reporting) ---
	// Default: auto_compile = true. Set to false to skip.
	bool bAutoCompile = true;
	Args->TryGetBoolField(TEXT("auto_compile"), bAutoCompile);

	if (bAutoCompile)
	{
		// Pre-compile safety: remove 0-pin orphaned nodes that could crash the compiler
		TArray<FString> BatchRemovedOrphans;
		auto CleanGraph = [&BatchRemovedOrphans](UEdGraph* G, bool bIsFunctionGraph)
		{
			if (!G) return;
			TArray<UEdGraphNode*> ToRemove;
			for (UEdGraphNode* N : G->Nodes)
			{
				if (!N || N->Pins.Num() > 0) continue;
				if (N->IsA(UK2Node_Event::StaticClass())) continue;
				if (bIsFunctionGraph && (N->IsA(UK2Node_FunctionEntry::StaticClass()) || N->IsA(UK2Node_FunctionResult::StaticClass()))) continue;
				ToRemove.Add(N);
			}
			for (UEdGraphNode* Bad : ToRemove)
			{
				FString Info = FString::Printf(TEXT("%s (id=%s)"),
					*Bad->GetNodeTitle(ENodeTitleType::ListView).ToString(),
					*Bad->NodeGuid.ToString());
				BatchRemovedOrphans.Add(Info);
				UE_LOG(LogTemp, Warning, TEXT("AIAssistant batch auto_compile: Removed 0-pin orphan '%s'"), *Info);
				G->RemoveNode(Bad);
			}
		};
		for (UEdGraph* G : Blueprint->UbergraphPages) CleanGraph(G, false);
		for (UEdGraph* G : Blueprint->FunctionGraphs) CleanGraph(G, true);

		FKismetEditorUtilities::CompileBlueprint(Blueprint);

		bool bHasErrors = Blueprint->Status == BS_Error;
		bool bHasWarnings = Blueprint->Status == BS_UpToDateWithWarnings;

		TSharedPtr<FJsonObject> CompileObj = MakeShared<FJsonObject>();
		CompileObj->SetBoolField(TEXT("compiled"), !bHasErrors);

		if (BatchRemovedOrphans.Num() > 0)
		{
			TArray<TSharedPtr<FJsonValue>> OrphanArr;
			for (const FString& S : BatchRemovedOrphans)
				OrphanArr.Add(MakeShared<FJsonValueString>(S));
			CompileObj->SetArrayField(TEXT("removedOrphanNodes"), OrphanArr);
			CompileObj->SetNumberField(TEXT("removedOrphanCount"), BatchRemovedOrphans.Num());
		}

		if (bHasErrors || bHasWarnings)
		{
			// Scan all graphs for compiler messages
			TArray<TSharedPtr<FJsonValue>> DiagArray;
			int32 ErrorCount = 0;
			int32 WarningCount = 0;

			auto ScanGraph = [&](UEdGraph* ScanGraphPtr, const FString& GName)
			{
				if (!ScanGraphPtr) return;
				for (UEdGraphNode* Node : ScanGraphPtr->Nodes)
				{
					if (!Node || !Node->bHasCompilerMessage) continue;

					TSharedPtr<FJsonObject> DiagObj = MakeShared<FJsonObject>();
					DiagObj->SetStringField(TEXT("graph"), GName);
					DiagObj->SetStringField(TEXT("nodeId"), Node->NodeGuid.ToString());
					DiagObj->SetStringField(TEXT("nodeTitle"), Node->GetNodeTitle(ENodeTitleType::ListView).ToString());

					bool bIsError = (Node->ErrorType <= EMessageSeverity::Error);
					DiagObj->SetStringField(TEXT("severity"), bIsError ? TEXT("Error") : TEXT("Warning"));
					DiagObj->SetStringField(TEXT("message"), Node->ErrorMsg);

					if (bIsError) ErrorCount++;
					else WarningCount++;

					DiagArray.Add(MakeShared<FJsonValueObject>(DiagObj));
				}
			};

			for (UEdGraph* G : Blueprint->UbergraphPages)
				ScanGraph(G, G ? G->GetName() : TEXT("Unknown"));
			for (UEdGraph* G : Blueprint->FunctionGraphs)
				ScanGraph(G, G ? G->GetName() : TEXT("Unknown"));

			CompileObj->SetNumberField(TEXT("errorCount"), ErrorCount);
			CompileObj->SetNumberField(TEXT("warningCount"), WarningCount);
			CompileObj->SetArrayField(TEXT("diagnostics"), DiagArray);
			CompileObj->SetStringField(TEXT("status"), bHasErrors ? TEXT("error") : TEXT("warning"));
		}
		else
		{
			CompileObj->SetStringField(TEXT("status"), TEXT("ok"));
			CompileObj->SetNumberField(TEXT("errorCount"), 0);
			CompileObj->SetNumberField(TEXT("warningCount"), 0);
		}

		Data->SetObjectField(TEXT("compile"), CompileObj);
	}

	Data->SetStringField(TEXT("message"), FString::Printf(TEXT("Batch: %d/%d succeeded"), SuccessCount, OpsArray->Num()));

	return FCommandResult::Success(Data);
}

// ============================================
// add_pin — dynamically add pins to nodes that support it (Sequence, MakeArray, SwitchEnum, etc.)
// Args: { "blueprint_name": "BP_MyActor", "graph_name": "EventGraph", "node_id": "GUID", "count": 1 }
// ============================================
FCommandResult FBlueprintNodeHandler::HandleAddPin(const TSharedPtr<FJsonObject>& Args)
{
	FString BPName;
	if (!Args->TryGetStringField(TEXT("blueprint_name"), BPName))
		return FCommandResult::Fail(TEXT("Missing 'blueprint_name'"));

	FString NodeIdStr;
	if (!Args->TryGetStringField(TEXT("node_id"), NodeIdStr))
		return FCommandResult::Fail(TEXT("Missing 'node_id'"));

	int32 Count = 1;
	Args->TryGetNumberField(TEXT("count"), Count);
	if (Count < 1) Count = 1;

	FString GraphName = TEXT("EventGraph");
	Args->TryGetStringField(TEXT("graph_name"), GraphName);

	UBlueprint* Blueprint = FindBlueprintByName(BPName);
	if (!Blueprint)
		return FCommandResult::Fail(FString::Printf(TEXT("Blueprint '%s' not found"), *BPName));

	UEdGraph* Graph = FindGraphInBlueprint(Blueprint, GraphName);
	if (!Graph)
		return FCommandResult::Fail(FString::Printf(TEXT("Graph '%s' not found"), *GraphName));

	UEdGraphNode* Node = FindNodeByGuid(Graph, NodeIdStr);
	if (!Node)
		return FCommandResult::Fail(FString::Printf(TEXT("Node '%s' not found"), *NodeIdStr));

	int32 PinsBefore = Node->Pins.Num();

	// Try Sequence node (K2Node_ExecutionSequence::AddInputPin adds a Then output)
	if (UK2Node_ExecutionSequence* SeqNode = Cast<UK2Node_ExecutionSequence>(Node))
	{
		for (int32 i = 0; i < Count; ++i)
		{
			SeqNode->AddInputPin();
		}
	}
	// Try MakeArray node
	else if (UK2Node_MakeArray* ArrayNode = Cast<UK2Node_MakeArray>(Node))
	{
		for (int32 i = 0; i < Count; ++i)
		{
			ArrayNode->AddInputPin();
		}
	}
	else
	{
		return FCommandResult::Fail(FString::Printf(
			TEXT("Node type '%s' does not support adding pins via add_pin. Supported types: Sequence, MakeArray."),
			*Node->GetClass()->GetName()));
	}

	int32 PinsAfter = Node->Pins.Num();
	int32 Added = PinsAfter - PinsBefore;

	Blueprint->MarkPackageDirty();
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);

	// Return updated node with all pins
	TSharedPtr<FJsonObject> Data = NodeToJson(Node);
	Data->SetNumberField(TEXT("pinsAdded"), Added);
	Data->SetStringField(TEXT("message"), FString::Printf(TEXT("Added %d pin(s) to '%s' (now %d pins)"),
		Added, *Node->GetNodeTitle(ENodeTitleType::ListView).ToString(), PinsAfter));

	return FCommandResult::Success(Data);
}

// ============================================
// auto_layout_graph — automatic node layout using BFS layering
// Args: { "blueprint_name": "BP_MyActor", "graph_name": "EventGraph" }
// ============================================
FCommandResult FBlueprintNodeHandler::HandleAutoLayoutGraph(const TSharedPtr<FJsonObject>& Args)
{
	FString BPName;
	if (!Args->TryGetStringField(TEXT("blueprint_name"), BPName))
		return FCommandResult::Fail(TEXT("Missing 'blueprint_name'"));

	UBlueprint* Blueprint = FindBlueprintByName(BPName);
	if (!Blueprint)
		return FCommandResult::Fail(FString::Printf(TEXT("Blueprint '%s' not found"), *BPName));

	FString GraphName;
	if (!Args->TryGetStringField(TEXT("graph_name"), GraphName))
		GraphName = TEXT("EventGraph");

	UEdGraph* Graph = FindGraphInBlueprint(Blueprint, GraphName);
	if (!Graph)
		return FCommandResult::Fail(FString::Printf(TEXT("Graph '%s' not found"), *GraphName));

	int32 NodesArranged = AutoLayoutGraph(Graph);

	Blueprint->MarkPackageDirty();

	return FCommandResult::SuccessMessage(FString::Printf(TEXT("Auto-layout arranged %d nodes in '%s'"), NodesArranged, *GraphName));
}

// ============================================
// AutoLayoutGraph — BFS-based layered layout algorithm
// ============================================
int32 FBlueprintNodeHandler::AutoLayoutGraph(UEdGraph* Graph) const
{
	if (!Graph || Graph->Nodes.Num() == 0) return 0;

	const int32 HORIZONTAL_SPACING = 350;
	const int32 VERTICAL_SPACING = 150;
	const int32 DATA_NODE_OFFSET_Y = -120; // Data nodes above their consumer

	// Step 1: Find all "root" nodes (FunctionEntry, Event, CustomEvent — they start exec chains)
	TArray<UEdGraphNode*> RootNodes;
	TSet<UEdGraphNode*> AllNodes;

	for (UEdGraphNode* Node : Graph->Nodes)
	{
		if (!Node) continue;
		AllNodes.Add(Node);

		if (Node->IsA<UK2Node_FunctionEntry>() || Node->IsA<UK2Node_Event>() || Node->IsA<UK2Node_CustomEvent>())
		{
			RootNodes.Add(Node);
		}
	}

	if (RootNodes.Num() == 0)
	{
		// No roots found, pick the leftmost node as starting point
		UEdGraphNode* Leftmost = nullptr;
		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (Node && (!Leftmost || Node->NodePosX < Leftmost->NodePosX))
			{
				Leftmost = Node;
			}
		}
		if (Leftmost) RootNodes.Add(Leftmost);
	}

	// Step 2: BFS along exec pins to assign layers
	TMap<UEdGraphNode*, int32> NodeLayer; // Node -> horizontal layer index
	TSet<UEdGraphNode*> Visited;

	auto IsExecPin = [](UEdGraphPin* Pin) -> bool
	{
		return Pin && Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec;
	};

	int32 RootOffsetY = 0;
	TMap<UEdGraphNode*, int32> NodeBaseY; // Track Y offset per root chain

	for (UEdGraphNode* Root : RootNodes)
	{
		// BFS from this root
		TArray<UEdGraphNode*> Queue;
		Queue.Add(Root);
		NodeLayer.Add(Root, 0);
		NodeBaseY.Add(Root, RootOffsetY);
		Visited.Add(Root);

		int32 MaxLayerInChain = 0;

		while (Queue.Num() > 0)
		{
			UEdGraphNode* Current = Queue[0];
			Queue.RemoveAt(0);

			int32 CurrentLayer = NodeLayer[Current];
			MaxLayerInChain = FMath::Max(MaxLayerInChain, CurrentLayer);

			// Follow output exec pins to find next nodes
			for (UEdGraphPin* Pin : Current->Pins)
			{
				if (!IsExecPin(Pin) || Pin->Direction != EGPD_Output) continue;

				for (UEdGraphPin* Linked : Pin->LinkedTo)
				{
					if (!Linked || !Linked->GetOwningNode()) continue;
					UEdGraphNode* NextNode = Linked->GetOwningNode();

					if (!Visited.Contains(NextNode))
					{
						Visited.Add(NextNode);
						NodeLayer.Add(NextNode, CurrentLayer + 1);
						NodeBaseY.Add(NextNode, RootOffsetY);
						Queue.Add(NextNode);
					}
				}
			}
		}

		// Add vertical spacing between different root chains
		RootOffsetY += (MaxLayerInChain > 0 ? 3 : 1) * VERTICAL_SPACING;
	}

	// Step 3: Handle unvisited data-only nodes (VariableGet etc.)
	// Place them near the nodes that consume them
	for (UEdGraphNode* Node : Graph->Nodes)
	{
		if (!Node || Visited.Contains(Node)) continue;

		// Find the first connected consumer node that was visited
		UEdGraphNode* Consumer = nullptr;
		for (UEdGraphPin* Pin : Node->Pins)
		{
			if (!Pin || Pin->Direction != EGPD_Output) continue;
			for (UEdGraphPin* Linked : Pin->LinkedTo)
			{
				if (Linked && Linked->GetOwningNode() && Visited.Contains(Linked->GetOwningNode()))
				{
					Consumer = Linked->GetOwningNode();
					break;
				}
			}
			if (Consumer) break;
		}

		if (Consumer)
		{
			int32 ConsumerLayer = NodeLayer[Consumer];
			NodeLayer.Add(Node, FMath::Max(0, ConsumerLayer - 1));
			NodeBaseY.Add(Node, NodeBaseY[Consumer] + DATA_NODE_OFFSET_Y);
		}
		else
		{
			// Orphan node — place at end
			NodeLayer.Add(Node, 0);
			NodeBaseY.Add(Node, RootOffsetY);
			RootOffsetY += VERTICAL_SPACING;
		}
		Visited.Add(Node);
	}

	// Step 4: Group nodes by layer and assign positions
	TMap<int32, TArray<UEdGraphNode*>> LayerGroups;
	for (auto& Pair : NodeLayer)
	{
		LayerGroups.FindOrAdd(Pair.Value).Add(Pair.Key);
	}

	// Sort each layer group by their base Y position
	for (auto& Pair : LayerGroups)
	{
		Pair.Value.Sort([&NodeBaseY](const UEdGraphNode& A, const UEdGraphNode& B)
		{
			return NodeBaseY.FindRef(const_cast<UEdGraphNode*>(&A)) < NodeBaseY.FindRef(const_cast<UEdGraphNode*>(&B));
		});
	}

	// Step 5: Apply positions
	int32 NodesArranged = 0;
	for (auto& Pair : LayerGroups)
	{
		int32 Layer = Pair.Key;
		TArray<UEdGraphNode*>& Nodes = Pair.Value;

		for (int32 i = 0; i < Nodes.Num(); i++)
		{
			UEdGraphNode* Node = Nodes[i];
			if (!Node) continue;

			Node->NodePosX = Layer * HORIZONTAL_SPACING;
			Node->NodePosY = NodeBaseY.FindRef(Node) + i * VERTICAL_SPACING;
			NodesArranged++;
		}
	}

	return NodesArranged;
}

// ============================================
// list_node_types — discover all available K2Node subclasses
// Args: { "filter": "Switch" } (optional)
// ============================================
FCommandResult FBlueprintNodeHandler::HandleListNodeTypes(const TSharedPtr<FJsonObject>& Args)
{
	FString Filter;
	Args->TryGetStringField(TEXT("filter"), Filter);

	TArray<TSharedPtr<FJsonValue>> TypeArray;

	for (TObjectIterator<UClass> It; It; ++It)
	{
		UClass* Class = *It;
		if (!Class || !Class->IsChildOf(UK2Node::StaticClass())) continue;
		if (Class == UK2Node::StaticClass()) continue; // skip base

		// Skip abstract classes that can't be instantiated
		if (Class->HasAnyClassFlags(CLASS_Abstract)) continue;

		FString ClassName = Class->GetName();

		// Apply filter
		if (!Filter.IsEmpty() && !ClassName.Contains(Filter, ESearchCase::IgnoreCase))
		{
			continue;
		}

		TSharedPtr<FJsonObject> Entry = MakeShared<FJsonObject>();
		Entry->SetStringField(TEXT("className"), ClassName);

		// Parent class
		UClass* SuperClass = Class->GetSuperClass();
		if (SuperClass)
		{
			Entry->SetStringField(TEXT("parent"), SuperClass->GetName());
		}

		// Rich metadata from UE5 reflection: display name, tooltip, menu category, keywords
		// SAFETY: Many CDO methods on graph nodes are NOT safe — they may call
		// GetGraph()/GetPackage()/FindMetaData() which dereference invalid Outers.
		// CDOs have UClass as Outer (not UEdGraph), and some internal functions
		// (like UK2Node_CallFunction::GetDefaultCategoryForFunction) call
		// UField::FindMetaData -> GetPackage() on function pointers that may be stale.
		// Wrap ALL CDO method calls in structured exception handling to prevent crashes.
		// 
		// Known crash paths:
		// - GetNodeTitle() -> GetGraph() -> null deref (CDO has no graph)
		// - GetTooltipText() -> GetDefaultCategoryForFunction() -> FindMetaData() -> GetPackage() -> SIGSEGV
		// - GetMenuCategory() -> same path as above
		// - GetKeywords() -> same path as above

		// Skip CDO-based metadata entirely — it's too dangerous and causes editor crashes.
		// Only use class-level metadata which is always safe.

		// Class-level ToolTip metadata (from UCLASS macro) — this is safe, reads from UClass metadata
		if (Class->HasMetaData(TEXT("ToolTip")))
		{
			const FString& ClassTooltip = Class->GetMetaData(TEXT("ToolTip"));
			if (!ClassTooltip.IsEmpty() && ClassTooltip.Len() < 500)
			{
				Entry->SetStringField(TEXT("description"), ClassTooltip);
			}
		}

		// DisplayName from class metadata (safe)
		if (Class->HasMetaData(TEXT("DisplayName")))
		{
			Entry->SetStringField(TEXT("displayName"), Class->GetMetaData(TEXT("DisplayName")));
		}

		// Keywords from class metadata (safe)
		if (Class->HasMetaData(TEXT("Keywords")))
		{
			Entry->SetStringField(TEXT("keywords"), Class->GetMetaData(TEXT("Keywords")));
		}

		TypeArray.Add(MakeShared<FJsonValueObject>(Entry));
	}

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetArrayField(TEXT("nodeTypes"), TypeArray);
	Data->SetNumberField(TEXT("count"), TypeArray.Num());
	if (!Filter.IsEmpty())
	{
		Data->SetStringField(TEXT("filter"), Filter);
	}

	return FCommandResult::Success(Data);
}

// ============================================
// list_blueprint_classes — discover all UBlueprintFunctionLibrary subclasses
// Args: { "filter": "Math" } or { "blueprint_name": "BP_Hero" }
// ============================================
FCommandResult FBlueprintNodeHandler::HandleListBlueprintClasses(const TSharedPtr<FJsonObject>& Args)
{
	FString Filter;
	Args->TryGetStringField(TEXT("filter"), Filter);

	FString BPName;
	Args->TryGetStringField(TEXT("blueprint_name"), BPName);

	TArray<TSharedPtr<FJsonValue>> ClassArray;

	// 1. All UBlueprintFunctionLibrary subclasses (the "library" classes)
	for (TObjectIterator<UClass> It; It; ++It)
	{
		UClass* Class = *It;
		if (!Class) continue;

		// Must be a function library subclass
		static UClass* FuncLibBase = FindObject<UClass>(nullptr, TEXT("/Script/Engine.BlueprintFunctionLibrary"));
		if (!FuncLibBase) break;

		if (!Class->IsChildOf(FuncLibBase)) continue;
		if (Class == FuncLibBase) continue;
		if (Class->HasAnyClassFlags(CLASS_Abstract)) continue;

		FString ClassName = Class->GetName();
		if (!Filter.IsEmpty() && !ClassName.Contains(Filter, ESearchCase::IgnoreCase))
		{
			continue;
		}

		// Count BlueprintCallable functions
		int32 FuncCount = 0;
		for (TFieldIterator<UFunction> FuncIt(Class, EFieldIteratorFlags::ExcludeSuper); FuncIt; ++FuncIt)
		{
			if (FuncIt->HasAnyFunctionFlags(FUNC_BlueprintCallable | FUNC_BlueprintPure))
			{
				FuncCount++;
			}
		}

		TSharedPtr<FJsonObject> Entry = MakeShared<FJsonObject>();
		Entry->SetStringField(TEXT("name"), ClassName);
		Entry->SetNumberField(TEXT("functionCount"), FuncCount);

		// Category from metadata
		const FString& CategoryMeta = Class->GetMetaData(TEXT("Category"));
		if (!CategoryMeta.IsEmpty())
		{
			Entry->SetStringField(TEXT("category"), CategoryMeta);
		}

		// ToolTip — engine's own description of what this library does
		const FString& ClassTooltip = Class->GetMetaData(TEXT("ToolTip"));
		if (!ClassTooltip.IsEmpty() && ClassTooltip.Len() < 500)
		{
			Entry->SetStringField(TEXT("description"), ClassTooltip);
		}

		// DisplayName if available
		const FString& DisplayMeta = Class->GetMetaData(TEXT("DisplayName"));
		if (!DisplayMeta.IsEmpty())
		{
			Entry->SetStringField(TEXT("displayName"), DisplayMeta);
		}

		ClassArray.Add(MakeShared<FJsonValueObject>(Entry));
	}

	// 2. If blueprint_name is given, also include its parent class chain
	if (!BPName.IsEmpty())
	{
		UBlueprint* Blueprint = FindBlueprintByName(BPName);
		if (Blueprint && Blueprint->ParentClass)
		{
			for (UClass* C = Blueprint->ParentClass; C; C = C->GetSuperClass())
			{
				FString ClassName = C->GetName();
				if (!Filter.IsEmpty() && !ClassName.Contains(Filter, ESearchCase::IgnoreCase))
				{
					continue;
				}

				int32 FuncCount = 0;
				for (TFieldIterator<UFunction> FuncIt(C, EFieldIteratorFlags::ExcludeSuper); FuncIt; ++FuncIt)
				{
					if (FuncIt->HasAnyFunctionFlags(FUNC_BlueprintCallable | FUNC_BlueprintPure))
					{
						FuncCount++;
					}
				}

				if (FuncCount == 0) continue;

				TSharedPtr<FJsonObject> Entry = MakeShared<FJsonObject>();
				Entry->SetStringField(TEXT("name"), ClassName);
				Entry->SetNumberField(TEXT("functionCount"), FuncCount);
				Entry->SetStringField(TEXT("source"), TEXT("blueprint_parent_chain"));

				const FString& Tip = C->GetMetaData(TEXT("ToolTip"));
				if (!Tip.IsEmpty() && Tip.Len() < 500)
				{
					Entry->SetStringField(TEXT("description"), Tip);
				}

				ClassArray.Add(MakeShared<FJsonValueObject>(Entry));
			}
		}
	}

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetArrayField(TEXT("classes"), ClassArray);
	Data->SetNumberField(TEXT("count"), ClassArray.Num());
	if (!Filter.IsEmpty())
	{
		Data->SetStringField(TEXT("filter"), Filter);
	}

	return FCommandResult::Success(Data);
}

// ============================================
// list_functions — list all BlueprintCallable functions in a class
// Args: { "class_name": "KismetMathLibrary", "filter": "Add" }
//   or: { "blueprint_name": "BP_Hero", "filter": "Jump" }
// ============================================
FCommandResult FBlueprintNodeHandler::HandleListFunctions(const TSharedPtr<FJsonObject>& Args)
{
	FString ClassName;
	Args->TryGetStringField(TEXT("class_name"), ClassName);

	FString BPName;
	Args->TryGetStringField(TEXT("blueprint_name"), BPName);

	FString Filter;
	Args->TryGetStringField(TEXT("filter"), Filter);

	if (ClassName.IsEmpty() && BPName.IsEmpty())
	{
		return FCommandResult::Fail(TEXT("Provide 'class_name' (e.g. KismetMathLibrary) or 'blueprint_name'. Use list_blueprint_classes to discover available classes."));
	}

	// Collect the target UClass(es)
	TArray<UClass*> TargetClasses;

	if (!ClassName.IsEmpty())
	{
		// Find UClass by name
		UClass* Found = nullptr;
		for (TObjectIterator<UClass> It; It; ++It)
		{
			if (It->GetName() == ClassName)
			{
				Found = *It;
				break;
			}
		}
		if (!Found)
		{
			return FCommandResult::Fail(FString::Printf(TEXT("Class '%s' not found. Use list_blueprint_classes to discover available classes."), *ClassName));
		}
		TargetClasses.Add(Found);
	}

	if (!BPName.IsEmpty())
	{
		UBlueprint* Blueprint = FindBlueprintByName(BPName);
		if (!Blueprint)
		{
			return FCommandResult::Fail(FString::Printf(TEXT("Blueprint '%s' not found"), *BPName));
		}
		// Use GetSearchClasses to include parent chain + component classes
		TArray<UClass*> BPClasses = GetSearchClasses(Blueprint);
		for (UClass* C : BPClasses)
		{
			TargetClasses.AddUnique(C);
		}
		// Also include the skeleton generated class for BP-defined functions
		if (Blueprint->SkeletonGeneratedClass)
		{
			TargetClasses.AddUnique(Blueprint->SkeletonGeneratedClass);
		}
	}

	// Optional max_results limit — prevents massive responses for classes with hundreds of functions
	int32 MaxResults = 0;  // 0 = no limit
	Args->TryGetNumberField(TEXT("max_results"), MaxResults);

	TArray<TSharedPtr<FJsonValue>> FuncArray;
	bool bTruncated = false;
	int32 TotalFound = 0;

	for (UClass* TargetClass : TargetClasses)
	{
		if (bTruncated) break;

		for (TFieldIterator<UFunction> FuncIt(TargetClass, EFieldIteratorFlags::ExcludeSuper); FuncIt; ++FuncIt)
		{
			UFunction* Func = *FuncIt;
			if (!Func) continue;

			bool bCallable = Func->HasAnyFunctionFlags(FUNC_BlueprintCallable);
			bool bPure = Func->HasAnyFunctionFlags(FUNC_BlueprintPure);

			if (!bCallable && !bPure) continue;

			FString FuncName = Func->GetName();

			// Apply filter
			if (!Filter.IsEmpty())
			{
				// Check name and display name
				FString DisplayName = Func->GetMetaData(TEXT("DisplayName"));
				if (!FuncName.Contains(Filter, ESearchCase::IgnoreCase) &&
					!DisplayName.Contains(Filter, ESearchCase::IgnoreCase))
				{
					continue;
				}
			}

			TotalFound++;

			// Check max_results limit
			if (MaxResults > 0 && FuncArray.Num() >= MaxResults)
			{
				bTruncated = true;
				break;
			}

			TSharedPtr<FJsonObject> Entry = MakeShared<FJsonObject>();
			Entry->SetStringField(TEXT("name"), FuncName);

			// Display name from metadata
			FString DisplayName = Func->GetMetaData(TEXT("DisplayName"));
			if (!DisplayName.IsEmpty())
			{
				Entry->SetStringField(TEXT("displayName"), DisplayName);
			}

			// Category
			FString Category = Func->GetMetaData(TEXT("Category"));
			if (!Category.IsEmpty())
			{
				Entry->SetStringField(TEXT("category"), Category);
			}

			Entry->SetBoolField(TEXT("isPure"), bPure);

			// ToolTip — UE5 engine's own description of what this function does
			const FString& FuncTooltip = Func->GetMetaData(TEXT("ToolTip"));
			if (!FuncTooltip.IsEmpty() && FuncTooltip.Len() < 800)
			{
				Entry->SetStringField(TEXT("tooltip"), FuncTooltip);
			}

			// CompactNodeTitle (short name shown on compact nodes, e.g. "+" for Add)
			const FString& CompactTitle = Func->GetMetaData(TEXT("CompactNodeTitle"));
			if (!CompactTitle.IsEmpty())
			{
				Entry->SetStringField(TEXT("compactTitle"), CompactTitle);
			}

			// Keywords for search
			const FString& Keywords = Func->GetMetaData(TEXT("Keywords"));
			if (!Keywords.IsEmpty())
			{
				Entry->SetStringField(TEXT("keywords"), Keywords);
			}

			// Parameters
			TArray<TSharedPtr<FJsonValue>> ParamArray;
			FString ReturnType;

			for (TFieldIterator<FProperty> PropIt(Func); PropIt; ++PropIt)
			{
				FProperty* Prop = *PropIt;
				if (!Prop) continue;

				if (Prop->HasAnyPropertyFlags(CPF_ReturnParm))
				{
					ReturnType = Prop->GetCPPType();
					continue;
				}

				if (Prop->HasAnyPropertyFlags(CPF_Parm))
				{
					TSharedPtr<FJsonObject> ParamObj = MakeShared<FJsonObject>();
					ParamObj->SetStringField(TEXT("name"), Prop->GetName());
					ParamObj->SetStringField(TEXT("type"), Prop->GetCPPType());

					if (Prop->HasAnyPropertyFlags(CPF_OutParm))
					{
						ParamObj->SetBoolField(TEXT("isOut"), true);
					}

					// Default value from metadata
					FString MetaDefault = Func->GetMetaData(*FString::Printf(TEXT("CPP_Default_%s"), *Prop->GetName()));
					if (!MetaDefault.IsEmpty())
					{
						ParamObj->SetStringField(TEXT("default"), MetaDefault);
					}

					// Parameter tooltip (from UPARAM or comment)
					if (Prop->HasMetaData(TEXT("ToolTip")))
					{
						const FString& ParamTooltip = Prop->GetMetaData(TEXT("ToolTip"));
						if (!ParamTooltip.IsEmpty())
						{
							ParamObj->SetStringField(TEXT("tooltip"), ParamTooltip);
						}
					}

					ParamArray.Add(MakeShared<FJsonValueObject>(ParamObj));
				}
			}

			Entry->SetArrayField(TEXT("params"), ParamArray);
			if (!ReturnType.IsEmpty())
			{
				Entry->SetStringField(TEXT("returnType"), ReturnType);
			}

			Entry->SetStringField(TEXT("ownerClass"), TargetClass->GetName());

			FuncArray.Add(MakeShared<FJsonValueObject>(Entry));
		}
	}

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetArrayField(TEXT("functions"), FuncArray);
	Data->SetNumberField(TEXT("count"), FuncArray.Num());
	if (bTruncated)
	{
		Data->SetBoolField(TEXT("truncated"), true);
		Data->SetNumberField(TEXT("maxResults"), MaxResults);
		Data->SetStringField(TEXT("hint"), FString::Printf(TEXT("Showing %d of %d+ results. Use 'filter' to narrow or increase 'max_results'."), FuncArray.Num(), TotalFound));
	}
	if (!Filter.IsEmpty())
	{
		Data->SetStringField(TEXT("filter"), Filter);
	}

	return FCommandResult::Success(Data);
}

// ============================================
// Helpers
// ============================================

UBlueprint* FBlueprintNodeHandler::FindBlueprintByName(const FString& Name) const
{
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

	TArray<FAssetData> AssetList;
	AssetRegistry.GetAssetsByClass(UBlueprint::StaticClass()->GetClassPathName(), AssetList, true);

	for (const FAssetData& Asset : AssetList)
	{
		if (Asset.AssetName.ToString().Equals(Name, ESearchCase::IgnoreCase))
		{
			return Cast<UBlueprint>(Asset.GetAsset());
		}
	}

	FString Path = FString::Printf(TEXT("/Game/Blueprints/%s.%s"), *Name, *Name);
	return LoadObject<UBlueprint>(nullptr, *Path);
}

UEdGraph* FBlueprintNodeHandler::FindGraphInBlueprint(UBlueprint* Blueprint, const FString& GraphName) const
{
	if (!Blueprint) return nullptr;

	// Search UbergraphPages (EventGraph, etc.)
	for (UEdGraph* Graph : Blueprint->UbergraphPages)
	{
		if (Graph && Graph->GetName().Equals(GraphName, ESearchCase::IgnoreCase))
		{
			return Graph;
		}
	}

	// Search FunctionGraphs
	for (UEdGraph* Graph : Blueprint->FunctionGraphs)
	{
		if (Graph && Graph->GetName().Equals(GraphName, ESearchCase::IgnoreCase))
		{
			return Graph;
		}
	}

	// Default: return first UbergraphPage if asking for EventGraph
	if (GraphName.Equals(TEXT("EventGraph"), ESearchCase::IgnoreCase) && Blueprint->UbergraphPages.Num() > 0)
	{
		return Blueprint->UbergraphPages[0];
	}

	return nullptr;
}

UEdGraphNode* FBlueprintNodeHandler::FindNodeByGuid(UEdGraph* Graph, const FString& GuidString) const
{
	if (!Graph) return nullptr;

	FGuid SearchGuid;
	FGuid::Parse(GuidString, SearchGuid);

	for (UEdGraphNode* Node : Graph->Nodes)
	{
		if (Node && Node->NodeGuid == SearchGuid)
		{
			return Node;
		}
	}

	return nullptr;
}
