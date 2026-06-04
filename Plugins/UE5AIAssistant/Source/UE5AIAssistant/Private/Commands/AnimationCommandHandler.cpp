// Copyright AI Assistant. All Rights Reserved.

#include "Commands/AnimationCommandHandler.h"
#include "Editor.h"
#include "Animation/AnimBlueprint.h"
#include "Animation/AnimBlueprintGeneratedClass.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimSequence.h"
#include "Animation/Skeleton.h"
#include "AnimationStateMachineGraph.h"
#include "AnimationStateMachineSchema.h"
#include "AnimStateNode.h"
#include "AnimStateEntryNode.h"
#include "AnimStateTransitionNode.h"
#include "AnimGraphNode_StateMachine.h"
#include "AnimationGraph.h"
#include "AnimationGraphSchema.h"
#include "AnimGraphNode_SequencePlayer.h"
#include "AnimGraphNode_Root.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"

TArray<FString> FAnimationCommandHandler::GetSupportedCommands() const
{
	return {
		TEXT("create_anim_blueprint"),
		TEXT("get_anim_blueprint_info"),
		TEXT("add_anim_state_machine"),
		TEXT("add_anim_state"),
		TEXT("add_anim_transition"),
		TEXT("set_anim_state_animation"),
		TEXT("compile_anim_blueprint"),
	};
}

FCommandResult FAnimationCommandHandler::Execute(const FString& Command, const TSharedPtr<FJsonObject>& Args)
{
	if (Command == TEXT("create_anim_blueprint"))      return HandleCreateAnimBlueprint(Args);
	if (Command == TEXT("get_anim_blueprint_info"))    return HandleGetAnimBlueprintInfo(Args);
	if (Command == TEXT("add_anim_state_machine"))     return HandleAddAnimStateMachine(Args);
	if (Command == TEXT("add_anim_state"))             return HandleAddAnimState(Args);
	if (Command == TEXT("add_anim_transition"))        return HandleAddAnimTransition(Args);
	if (Command == TEXT("set_anim_state_animation"))   return HandleSetAnimStateAnimation(Args);
	if (Command == TEXT("compile_anim_blueprint"))     return HandleCompileAnimBlueprint(Args);

	return FCommandResult::Fail(FString::Printf(TEXT("AnimationHandler: Unknown command '%s'"), *Command));
}

// ============================================
// create_anim_blueprint
// Args: {
//   "name": "ABP_MyCharacter",
//   "skeleton_path": "/Game/Characters/Mannequin/Skeleton",
//   "parent_class": "AnimInstance"  (optional)
// }
// ============================================
FCommandResult FAnimationCommandHandler::HandleCreateAnimBlueprint(const TSharedPtr<FJsonObject>& Args)
{
	FString Name;
	if (!Args->TryGetStringField(TEXT("name"), Name))
	{
		return FCommandResult::Fail(TEXT("Missing required 'name' argument"));
	}

	FString SkeletonPath;
	if (!Args->TryGetStringField(TEXT("skeleton_path"), SkeletonPath))
	{
		return FCommandResult::Fail(TEXT("Missing required 'skeleton_path' argument. Provide the path to a Skeleton asset (e.g. '/Game/Characters/Mannequin/Mesh/SK_Mannequin_Skeleton')"));
	}

	USkeleton* Skeleton = FindSkeletonByName(SkeletonPath);
	if (!Skeleton)
	{
		return FCommandResult::Fail(FString::Printf(TEXT("Skeleton '%s' not found. Provide full path or asset name."), *SkeletonPath));
	}

	// Resolve parent class (default: UAnimInstance)
	FString ParentClassName;
	if (!Args->TryGetStringField(TEXT("parent_class"), ParentClassName))
	{
		ParentClassName = TEXT("AnimInstance");
	}

	UClass* ParentClass = UAnimInstance::StaticClass();
	if (!ParentClassName.Equals(TEXT("AnimInstance"), ESearchCase::IgnoreCase))
	{
		// Try to find custom parent class
		FString SearchName = ParentClassName;
		if (!SearchName.StartsWith(TEXT("U")))
		{
			SearchName = FString::Printf(TEXT("U%s"), *ParentClassName);
		}
		UClass* FoundClass = FindObject<UClass>(nullptr, *FString::Printf(TEXT("/Script/Engine.%s"), *SearchName));
		if (FoundClass && FoundClass->IsChildOf(UAnimInstance::StaticClass()))
		{
			ParentClass = FoundClass;
		}
	}

	// Create the Animation Blueprint
	FString PackagePath = TEXT("/Game/Blueprints/Animation");
	FString PackageName = FString::Printf(TEXT("%s/%s"), *PackagePath, *Name);

	UPackage* Package = CreatePackage(*PackageName);
	if (!Package)
	{
		return FCommandResult::Fail(TEXT("Failed to create package for Animation Blueprint"));
	}

	UBlueprint* CreatedBP = FKismetEditorUtilities::CreateBlueprint(
		ParentClass,
		Package,
		FName(*Name),
		BPTYPE_Normal,
		UAnimBlueprint::StaticClass(),
		UAnimBlueprintGeneratedClass::StaticClass()
	);

	UAnimBlueprint* NewAnimBP = Cast<UAnimBlueprint>(CreatedBP);
	if (!NewAnimBP)
	{
		return FCommandResult::Fail(TEXT("Failed to create Animation Blueprint"));
	}

	// Use TargetSkeleton for now — compatible with UE 5.x
	NewAnimBP->TargetSkeleton = Skeleton;
	NewAnimBP->MarkPackageDirty();
	FAssetRegistryModule::AssetCreated(NewAnimBP);

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("name"), Name);
	Data->SetStringField(TEXT("skeleton"), Skeleton->GetName());
	Data->SetStringField(TEXT("skeletonPath"), Skeleton->GetPathName());
	Data->SetStringField(TEXT("parentClass"), ParentClass->GetName());
	Data->SetStringField(TEXT("path"), NewAnimBP->GetPathName());
	Data->SetStringField(TEXT("message"), FString::Printf(TEXT("Created Animation Blueprint '%s' with skeleton '%s'"), *Name, *Skeleton->GetName()));

	return FCommandResult::Success(Data);
}

// ============================================
// get_anim_blueprint_info
// Args: { "blueprint_name": "ABP_MyCharacter" }
// ============================================
FCommandResult FAnimationCommandHandler::HandleGetAnimBlueprintInfo(const TSharedPtr<FJsonObject>& Args)
{
	FString BPName;
	if (!Args->TryGetStringField(TEXT("blueprint_name"), BPName))
	{
		return FCommandResult::Fail(TEXT("Missing required 'blueprint_name' argument"));
	}

	UAnimBlueprint* AnimBP = FindAnimBlueprintByName(BPName);
	if (!AnimBP)
	{
		return FCommandResult::Fail(FString::Printf(TEXT("Animation Blueprint '%s' not found"), *BPName));
	}

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("name"), BPName);
	Data->SetStringField(TEXT("path"), AnimBP->GetPathName());
	Data->SetStringField(TEXT("parentClass"), AnimBP->ParentClass ? AnimBP->ParentClass->GetName() : TEXT("None"));

	// Skeleton info
	if (AnimBP->TargetSkeleton)
	{
		Data->SetStringField(TEXT("skeleton"), AnimBP->TargetSkeleton->GetName());
		Data->SetStringField(TEXT("skeletonPath"), AnimBP->TargetSkeleton->GetPathName());
	}

	// Variables
	TArray<TSharedPtr<FJsonValue>> VarArray;
	for (const FBPVariableDescription& Var : AnimBP->NewVariables)
	{
		TSharedPtr<FJsonObject> VarObj = MakeShared<FJsonObject>();
		VarObj->SetStringField(TEXT("name"), Var.VarName.ToString());
		VarObj->SetStringField(TEXT("type"), Var.VarType.PinCategory.ToString());
		VarArray.Add(MakeShared<FJsonValueObject>(VarObj));
	}
	Data->SetArrayField(TEXT("variables"), VarArray);

	// Enumerate AnimGraph nodes to find state machines
	TArray<TSharedPtr<FJsonValue>> SMArray;
	for (UEdGraph* Graph : AnimBP->FunctionGraphs)
	{
		if (!Graph) continue;

		for (UEdGraphNode* Node : Graph->Nodes)
		{
			UAnimGraphNode_StateMachine* SMNode = Cast<UAnimGraphNode_StateMachine>(Node);
			if (!SMNode) continue;

			UAnimationStateMachineGraph* SMGraph = SMNode->EditorStateMachineGraph;
			if (!SMGraph) continue;

			TSharedPtr<FJsonObject> SMObj = MakeShared<FJsonObject>();
			SMObj->SetStringField(TEXT("name"), SMGraph->GetName());
			SMObj->SetStringField(TEXT("nodeId"), SMNode->NodeGuid.ToString());

			// List states in this state machine
			{
				TArray<TSharedPtr<FJsonValue>> StateArray;
				for (UEdGraphNode* SMChildNode : SMGraph->Nodes)
				{
					UAnimStateNode* StateNode = Cast<UAnimStateNode>(SMChildNode);
					if (!StateNode) continue;

					TSharedPtr<FJsonObject> StateObj = MakeShared<FJsonObject>();
					StateObj->SetStringField(TEXT("name"), StateNode->GetStateName());
					StateObj->SetStringField(TEXT("nodeId"), StateNode->NodeGuid.ToString());
					StateArray.Add(MakeShared<FJsonValueObject>(StateObj));
				}
				SMObj->SetArrayField(TEXT("states"), StateArray);
				SMObj->SetNumberField(TEXT("stateCount"), StateArray.Num());

				// Count transitions
				int32 TransitionCount = 0;
				for (UEdGraphNode* SMChildNode : SMGraph->Nodes)
				{
					if (Cast<UAnimStateTransitionNode>(SMChildNode))
					{
						TransitionCount++;
					}
				}
				SMObj->SetNumberField(TEXT("transitionCount"), TransitionCount);
			}

			SMArray.Add(MakeShared<FJsonValueObject>(SMObj));
		}
	}

	// Also check UbergraphPages for state machines
	for (UEdGraph* Graph : AnimBP->UbergraphPages)
	{
		if (!Graph) continue;
		for (UEdGraphNode* Node : Graph->Nodes)
		{
			UAnimGraphNode_StateMachine* SMNode = Cast<UAnimGraphNode_StateMachine>(Node);
			if (!SMNode) continue;

			UAnimationStateMachineGraph* SMGraph = SMNode->EditorStateMachineGraph;
			if (!SMGraph) continue;

			TSharedPtr<FJsonObject> SMObj = MakeShared<FJsonObject>();
			SMObj->SetStringField(TEXT("name"), SMGraph->GetName());
			SMObj->SetStringField(TEXT("nodeId"), SMNode->NodeGuid.ToString());
			if (SMGraph)
			{
				TArray<TSharedPtr<FJsonValue>> StateArray;
				for (UEdGraphNode* SMChildNode : SMGraph->Nodes)
				{
					UAnimStateNode* StateNode = Cast<UAnimStateNode>(SMChildNode);
					if (!StateNode) continue;

					TSharedPtr<FJsonObject> StateObj = MakeShared<FJsonObject>();
					StateObj->SetStringField(TEXT("name"), StateNode->GetStateName());
					StateObj->SetStringField(TEXT("nodeId"), StateNode->NodeGuid.ToString());
					StateArray.Add(MakeShared<FJsonValueObject>(StateObj));
				}
				SMObj->SetArrayField(TEXT("states"), StateArray);
				SMObj->SetNumberField(TEXT("stateCount"), StateArray.Num());

				int32 TransitionCount = 0;
				for (UEdGraphNode* SMChildNode : SMGraph->Nodes)
				{
					if (Cast<UAnimStateTransitionNode>(SMChildNode))
						TransitionCount++;
				}
				SMObj->SetNumberField(TEXT("transitionCount"), TransitionCount);
			}

			SMArray.Add(MakeShared<FJsonValueObject>(SMObj));
		}
	}

	Data->SetArrayField(TEXT("stateMachines"), SMArray);
	Data->SetNumberField(TEXT("stateMachineCount"), SMArray.Num());

	return FCommandResult::Success(Data);
}

// ============================================
// add_anim_state_machine
// Args: {
//   "blueprint_name": "ABP_MyCharacter",
//   "state_machine_name": "Locomotion"
// }
// ============================================
FCommandResult FAnimationCommandHandler::HandleAddAnimStateMachine(const TSharedPtr<FJsonObject>& Args)
{
	FString BPName;
	if (!Args->TryGetStringField(TEXT("blueprint_name"), BPName))
	{
		return FCommandResult::Fail(TEXT("Missing required 'blueprint_name' argument"));
	}

	FString SMName;
	if (!Args->TryGetStringField(TEXT("state_machine_name"), SMName))
	{
		return FCommandResult::Fail(TEXT("Missing required 'state_machine_name' argument"));
	}

	UAnimBlueprint* AnimBP = FindAnimBlueprintByName(BPName);
	if (!AnimBP)
	{
		return FCommandResult::Fail(FString::Printf(TEXT("Animation Blueprint '%s' not found"), *BPName));
	}

	// Find the AnimGraph (first function graph that is an AnimationGraph)
	UEdGraph* AnimGraph = nullptr;
	for (UEdGraph* Graph : AnimBP->FunctionGraphs)
	{
		if (Graph && Graph->IsA<UAnimationGraph>())
		{
			AnimGraph = Graph;
			break;
		}
	}

	if (!AnimGraph)
	{
		return FCommandResult::Fail(TEXT("No AnimGraph found in Animation Blueprint"));
	}

	// Create the state machine node
	UAnimGraphNode_StateMachine* SMNode = NewObject<UAnimGraphNode_StateMachine>(AnimGraph);
	SMNode->NodePosX = 200;
	SMNode->NodePosY = 0;
	AnimGraph->AddNode(SMNode, true, false);
	SMNode->AllocateDefaultPins();

	// Rename the state machine
	UAnimationStateMachineGraph* SMGraph = SMNode->EditorStateMachineGraph;
	if (SMGraph)
	{
		SMGraph->Rename(*SMName);
	}

	AnimBP->MarkPackageDirty();
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(AnimBP);

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("blueprintName"), BPName);
	Data->SetStringField(TEXT("stateMachineName"), SMName);
	Data->SetStringField(TEXT("nodeId"), SMNode->NodeGuid.ToString());
	Data->SetStringField(TEXT("message"), FString::Printf(TEXT("Added state machine '%s' to '%s'"), *SMName, *BPName));

	return FCommandResult::Success(Data);
}

// ============================================
// add_anim_state
// Args: {
//   "blueprint_name": "ABP_MyCharacter",
//   "state_machine_name": "Locomotion",
//   "state_name": "Idle"
// }
// ============================================
FCommandResult FAnimationCommandHandler::HandleAddAnimState(const TSharedPtr<FJsonObject>& Args)
{
	FString BPName, SMName, StateName;
	if (!Args->TryGetStringField(TEXT("blueprint_name"), BPName))
		return FCommandResult::Fail(TEXT("Missing required 'blueprint_name' argument"));
	if (!Args->TryGetStringField(TEXT("state_machine_name"), SMName))
		return FCommandResult::Fail(TEXT("Missing required 'state_machine_name' argument"));
	if (!Args->TryGetStringField(TEXT("state_name"), StateName))
		return FCommandResult::Fail(TEXT("Missing required 'state_name' argument"));

	UAnimBlueprint* AnimBP = FindAnimBlueprintByName(BPName);
	if (!AnimBP)
		return FCommandResult::Fail(FString::Printf(TEXT("Animation Blueprint '%s' not found"), *BPName));

	UAnimationStateMachineGraph* SMGraph = FindStateMachineGraph(AnimBP, SMName);
	if (!SMGraph)
		return FCommandResult::Fail(FString::Printf(TEXT("State machine '%s' not found in '%s'"), *SMName, *BPName));

	// Check if state already exists
	if (FindStateNode(SMGraph, StateName))
	{
		return FCommandResult::Fail(FString::Printf(TEXT("State '%s' already exists in '%s'"), *StateName, *SMName));
	}

	// Create a new state node
	// (Schema is implicitly used by FGraphNodeCreator via the graph)
	
	// Calculate position - offset based on existing state count
	int32 ExistingStates = 0;
	for (UEdGraphNode* Node : SMGraph->Nodes)
	{
		if (Cast<UAnimStateNode>(Node))
			ExistingStates++;
	}

	FGraphNodeCreator<UAnimStateNode> NodeCreator(*SMGraph);
	UAnimStateNode* NewState = NodeCreator.CreateNode(false);
	NewState->NodePosX = 300 + (ExistingStates * 250);
	NewState->NodePosY = 0;
	NodeCreator.Finalize();

	// Rename the state by renaming its bound graph
	if (NewState->BoundGraph)
	{
		NewState->BoundGraph->Rename(*StateName);
	}

	// Note: AllocateDefaultPins() is already called by NodeCreator.Finalize(), do NOT call again

	AnimBP->MarkPackageDirty();
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(AnimBP);

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("blueprintName"), BPName);
	Data->SetStringField(TEXT("stateMachineName"), SMName);
	Data->SetStringField(TEXT("stateName"), StateName);
	Data->SetStringField(TEXT("nodeId"), NewState->NodeGuid.ToString());
	Data->SetStringField(TEXT("message"), FString::Printf(TEXT("Added state '%s' to state machine '%s'"), *StateName, *SMName));

	return FCommandResult::Success(Data);
}

// ============================================
// add_anim_transition
// Args: {
//   "blueprint_name": "ABP_MyCharacter",
//   "state_machine_name": "Locomotion",
//   "from_state": "Idle",
//   "to_state": "Walk"
// }
// ============================================
FCommandResult FAnimationCommandHandler::HandleAddAnimTransition(const TSharedPtr<FJsonObject>& Args)
{
	FString BPName, SMName, FromStateName, ToStateName;
	if (!Args->TryGetStringField(TEXT("blueprint_name"), BPName))
		return FCommandResult::Fail(TEXT("Missing required 'blueprint_name'"));
	if (!Args->TryGetStringField(TEXT("state_machine_name"), SMName))
		return FCommandResult::Fail(TEXT("Missing required 'state_machine_name'"));
	if (!Args->TryGetStringField(TEXT("from_state"), FromStateName))
		return FCommandResult::Fail(TEXT("Missing required 'from_state'"));
	if (!Args->TryGetStringField(TEXT("to_state"), ToStateName))
		return FCommandResult::Fail(TEXT("Missing required 'to_state'"));

	UAnimBlueprint* AnimBP = FindAnimBlueprintByName(BPName);
	if (!AnimBP)
		return FCommandResult::Fail(FString::Printf(TEXT("Animation Blueprint '%s' not found"), *BPName));

	UAnimationStateMachineGraph* SMGraph = FindStateMachineGraph(AnimBP, SMName);
	if (!SMGraph)
		return FCommandResult::Fail(FString::Printf(TEXT("State machine '%s' not found in '%s'"), *SMName, *BPName));

	UAnimStateNode* FromState = FindStateNode(SMGraph, FromStateName);
	if (!FromState)
		return FCommandResult::Fail(FString::Printf(TEXT("State '%s' not found in '%s'"), *FromStateName, *SMName));

	UAnimStateNode* ToState = FindStateNode(SMGraph, ToStateName);
	if (!ToState)
		return FCommandResult::Fail(FString::Printf(TEXT("State '%s' not found in '%s'"), *ToStateName, *SMName));

	// Create transition node
	FGraphNodeCreator<UAnimStateTransitionNode> TransitionCreator(*SMGraph);
	UAnimStateTransitionNode* TransitionNode = TransitionCreator.CreateNode(false);

	// Position transition between the two states
	TransitionNode->NodePosX = (FromState->NodePosX + ToState->NodePosX) / 2;
	TransitionNode->NodePosY = (FromState->NodePosY + ToState->NodePosY) / 2 - 50;
	TransitionCreator.Finalize();

	// Connect the nodes: FromState -> Transition -> ToState
	// Find output pin of FromState and input pin of Transition
	UEdGraphPin* FromOutputPin = nullptr;
	for (UEdGraphPin* Pin : FromState->Pins)
	{
		if (Pin && Pin->Direction == EGPD_Output)
		{
			FromOutputPin = Pin;
			break;
		}
	}

	UEdGraphPin* TransitionInputPin = nullptr;
	UEdGraphPin* TransitionOutputPin = nullptr;
	for (UEdGraphPin* Pin : TransitionNode->Pins)
	{
		if (Pin)
		{
			if (Pin->Direction == EGPD_Input)
				TransitionInputPin = Pin;
			else if (Pin->Direction == EGPD_Output)
				TransitionOutputPin = Pin;
		}
	}

	UEdGraphPin* ToInputPin = nullptr;
	for (UEdGraphPin* Pin : ToState->Pins)
	{
		if (Pin && Pin->Direction == EGPD_Input)
		{
			ToInputPin = Pin;
			break;
		}
	}

	// Make connections
	if (FromOutputPin && TransitionInputPin)
	{
		FromOutputPin->MakeLinkTo(TransitionInputPin);
	}
	if (TransitionOutputPin && ToInputPin)
	{
		TransitionOutputPin->MakeLinkTo(ToInputPin);
	}

	AnimBP->MarkPackageDirty();
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(AnimBP);

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("blueprintName"), BPName);
	Data->SetStringField(TEXT("stateMachineName"), SMName);
	Data->SetStringField(TEXT("fromState"), FromStateName);
	Data->SetStringField(TEXT("toState"), ToStateName);
	Data->SetStringField(TEXT("transitionNodeId"), TransitionNode->NodeGuid.ToString());
	Data->SetStringField(TEXT("message"), FString::Printf(TEXT("Added transition '%s' -> '%s' in '%s'"), *FromStateName, *ToStateName, *SMName));

	return FCommandResult::Success(Data);
}

// ============================================
// set_anim_state_animation
// Args: {
//   "blueprint_name": "ABP_MyCharacter",
//   "state_machine_name": "Locomotion",
//   "state_name": "Idle",
//   "animation_asset": "Idle_Anim"
// }
// ============================================
FCommandResult FAnimationCommandHandler::HandleSetAnimStateAnimation(const TSharedPtr<FJsonObject>& Args)
{
	FString BPName, SMName, StateName, AnimAssetName;
	if (!Args->TryGetStringField(TEXT("blueprint_name"), BPName))
		return FCommandResult::Fail(TEXT("Missing required 'blueprint_name'"));
	if (!Args->TryGetStringField(TEXT("state_machine_name"), SMName))
		return FCommandResult::Fail(TEXT("Missing required 'state_machine_name'"));
	if (!Args->TryGetStringField(TEXT("state_name"), StateName))
		return FCommandResult::Fail(TEXT("Missing required 'state_name'"));
	if (!Args->TryGetStringField(TEXT("animation_asset"), AnimAssetName))
		return FCommandResult::Fail(TEXT("Missing required 'animation_asset'"));

	UAnimBlueprint* AnimBP = FindAnimBlueprintByName(BPName);
	if (!AnimBP)
		return FCommandResult::Fail(FString::Printf(TEXT("Animation Blueprint '%s' not found"), *BPName));

	UAnimationStateMachineGraph* SMGraph = FindStateMachineGraph(AnimBP, SMName);
	if (!SMGraph)
		return FCommandResult::Fail(FString::Printf(TEXT("State machine '%s' not found in '%s'"), *SMName, *BPName));

	UAnimStateNode* StateNode = FindStateNode(SMGraph, StateName);
	if (!StateNode)
		return FCommandResult::Fail(FString::Printf(TEXT("State '%s' not found in '%s'"), *StateName, *SMName));

	// Find the animation asset
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

	UAnimSequence* AnimSequence = nullptr;
	TArray<FAssetData> AnimAssets;
	AssetRegistry.GetAssetsByClass(UAnimSequence::StaticClass()->GetClassPathName(), AnimAssets, true);

	for (const FAssetData& Asset : AnimAssets)
	{
		if (Asset.AssetName.ToString().Equals(AnimAssetName, ESearchCase::IgnoreCase))
		{
			AnimSequence = Cast<UAnimSequence>(Asset.GetAsset());
			break;
		}
	}

	if (!AnimSequence)
	{
		return FCommandResult::Fail(FString::Printf(TEXT("Animation asset '%s' not found. Search for available animations using 'get_assets_by_class' with class 'AnimSequence'."), *AnimAssetName));
	}

	// Get the state's bound graph and add/update a SequencePlayer node
	UEdGraph* StateGraph = StateNode->BoundGraph;
	if (!StateGraph)
	{
		return FCommandResult::Fail(TEXT("State has no bound graph"));
	}

	// Check if there's already a SequencePlayer in the state
	UAnimGraphNode_SequencePlayer* SeqPlayer = nullptr;
	for (UEdGraphNode* Node : StateGraph->Nodes)
	{
		SeqPlayer = Cast<UAnimGraphNode_SequencePlayer>(Node);
		if (SeqPlayer) break;
	}

	if (!SeqPlayer)
	{
		// Create a new SequencePlayer node
		SeqPlayer = NewObject<UAnimGraphNode_SequencePlayer>(StateGraph);
		SeqPlayer->NodePosX = -200;
		SeqPlayer->NodePosY = 0;
		StateGraph->AddNode(SeqPlayer, true, false);
		SeqPlayer->AllocateDefaultPins();
	}

	// Set the animation sequence
	SeqPlayer->Node.SetSequence(AnimSequence);

	// Try to connect to the result node (AnimGraphNode_Root)
	UAnimGraphNode_Root* ResultNode = nullptr;
	for (UEdGraphNode* Node : StateGraph->Nodes)
	{
		ResultNode = Cast<UAnimGraphNode_Root>(Node);
		if (ResultNode) break;
	}

	if (ResultNode && SeqPlayer)
	{
		// Find the output pose pin of the SequencePlayer
		UEdGraphPin* OutputPin = nullptr;
		for (UEdGraphPin* Pin : SeqPlayer->Pins)
		{
			if (Pin && Pin->Direction == EGPD_Output && Pin->PinType.PinCategory == TEXT("struct"))
			{
				OutputPin = Pin;
				break;
			}
		}

		// Find the input pin of the result node
		UEdGraphPin* InputPin = nullptr;
		for (UEdGraphPin* Pin : ResultNode->Pins)
		{
			if (Pin && Pin->Direction == EGPD_Input && Pin->PinType.PinCategory == TEXT("struct"))
			{
				InputPin = Pin;
				break;
			}
		}

		if (OutputPin && InputPin)
		{
			// Clear existing connections
			InputPin->BreakAllPinLinks();
			const UEdGraphSchema* Schema = StateGraph->GetSchema();
			Schema->TryCreateConnection(OutputPin, InputPin);
		}
	}

	AnimBP->MarkPackageDirty();
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(AnimBP);

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("blueprintName"), BPName);
	Data->SetStringField(TEXT("stateMachineName"), SMName);
	Data->SetStringField(TEXT("stateName"), StateName);
	Data->SetStringField(TEXT("animationAsset"), AnimSequence->GetName());
	Data->SetStringField(TEXT("animationPath"), AnimSequence->GetPathName());
	Data->SetStringField(TEXT("message"), FString::Printf(TEXT("Set animation '%s' on state '%s' in '%s'"), *AnimSequence->GetName(), *StateName, *SMName));

	return FCommandResult::Success(Data);
}

// ============================================
// compile_anim_blueprint
// Args: { "blueprint_name": "ABP_MyCharacter" }
// ============================================
FCommandResult FAnimationCommandHandler::HandleCompileAnimBlueprint(const TSharedPtr<FJsonObject>& Args)
{
	FString BPName;
	if (!Args->TryGetStringField(TEXT("blueprint_name"), BPName))
	{
		return FCommandResult::Fail(TEXT("Missing required 'blueprint_name' argument"));
	}

	UAnimBlueprint* AnimBP = FindAnimBlueprintByName(BPName);
	if (!AnimBP)
	{
		return FCommandResult::Fail(FString::Printf(TEXT("Animation Blueprint '%s' not found"), *BPName));
	}

	FKismetEditorUtilities::CompileBlueprint(AnimBP);

	bool bHasErrors = AnimBP->Status == BS_Error;

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("name"), BPName);
	Data->SetBoolField(TEXT("compiled"), !bHasErrors);
	Data->SetBoolField(TEXT("hasErrors"), bHasErrors);
	Data->SetStringField(TEXT("message"),
		bHasErrors
			? FString::Printf(TEXT("Animation Blueprint '%s' compiled with errors"), *BPName)
			: FString::Printf(TEXT("Animation Blueprint '%s' compiled successfully"), *BPName));

	return FCommandResult::Success(Data);
}

// ============================================
// Helpers
// ============================================

UAnimBlueprint* FAnimationCommandHandler::FindAnimBlueprintByName(const FString& Name) const
{
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

	TArray<FAssetData> AssetList;
	AssetRegistry.GetAssetsByClass(UAnimBlueprint::StaticClass()->GetClassPathName(), AssetList, true);

	for (const FAssetData& Asset : AssetList)
	{
		if (Asset.AssetName.ToString().Equals(Name, ESearchCase::IgnoreCase))
		{
			return Cast<UAnimBlueprint>(Asset.GetAsset());
		}
	}

	// Also search as regular blueprint (UAnimBlueprint is a subclass)
	TArray<FAssetData> BPList;
	AssetRegistry.GetAssetsByClass(UBlueprint::StaticClass()->GetClassPathName(), BPList, true);
	for (const FAssetData& Asset : BPList)
	{
		if (Asset.AssetName.ToString().Equals(Name, ESearchCase::IgnoreCase))
		{
			UAnimBlueprint* AnimBP = Cast<UAnimBlueprint>(Asset.GetAsset());
			if (AnimBP) return AnimBP;
		}
	}

	// Direct load attempt
	FString Path = FString::Printf(TEXT("/Game/Blueprints/Animation/%s.%s"), *Name, *Name);
	return LoadObject<UAnimBlueprint>(nullptr, *Path);
}

UAnimationStateMachineGraph* FAnimationCommandHandler::FindStateMachineGraph(UAnimBlueprint* AnimBP, const FString& StateMachineName) const
{
	if (!AnimBP) return nullptr;

	// Search in FunctionGraphs (AnimGraph is stored here)
	for (UEdGraph* Graph : AnimBP->FunctionGraphs)
	{
		if (!Graph) continue;
		for (UEdGraphNode* Node : Graph->Nodes)
		{
			UAnimGraphNode_StateMachine* SMNode = Cast<UAnimGraphNode_StateMachine>(Node);
			if (!SMNode) continue;

			UAnimationStateMachineGraph* SMGraph = SMNode->EditorStateMachineGraph;
			if (SMGraph && SMGraph->GetName().Equals(StateMachineName, ESearchCase::IgnoreCase))
			{
				return SMGraph;
			}
		}
	}

	// Also search UbergraphPages
	for (UEdGraph* Graph : AnimBP->UbergraphPages)
	{
		if (!Graph) continue;
		for (UEdGraphNode* Node : Graph->Nodes)
		{
			UAnimGraphNode_StateMachine* SMNode = Cast<UAnimGraphNode_StateMachine>(Node);
			if (!SMNode) continue;

			UAnimationStateMachineGraph* SMGraph = SMNode->EditorStateMachineGraph;
			if (SMGraph && SMGraph->GetName().Equals(StateMachineName, ESearchCase::IgnoreCase))
			{
				return SMGraph;
			}
		}
	}

	return nullptr;
}

UAnimStateNode* FAnimationCommandHandler::FindStateNode(UAnimationStateMachineGraph* SMGraph, const FString& StateName) const
{
	if (!SMGraph) return nullptr;

	for (UEdGraphNode* Node : SMGraph->Nodes)
	{
		UAnimStateNode* StateNode = Cast<UAnimStateNode>(Node);
		if (StateNode && StateNode->GetStateName().Equals(StateName, ESearchCase::IgnoreCase))
		{
			return StateNode;
		}
	}

	return nullptr;
}

USkeleton* FAnimationCommandHandler::FindSkeletonByName(const FString& NameOrPath) const
{
	// Try direct load first (if full path provided)
	USkeleton* Skeleton = LoadObject<USkeleton>(nullptr, *NameOrPath);
	if (Skeleton) return Skeleton;

	// Try with .asset_name suffix
	FString PathWithSuffix = FString::Printf(TEXT("%s.%s"), *NameOrPath, *FPaths::GetBaseFilename(NameOrPath));
	Skeleton = LoadObject<USkeleton>(nullptr, *PathWithSuffix);
	if (Skeleton) return Skeleton;

	// Search by name in asset registry
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

	TArray<FAssetData> AssetList;
	AssetRegistry.GetAssetsByClass(USkeleton::StaticClass()->GetClassPathName(), AssetList, true);

	for (const FAssetData& Asset : AssetList)
	{
		if (Asset.AssetName.ToString().Equals(NameOrPath, ESearchCase::IgnoreCase) ||
			Asset.GetObjectPathString().Contains(NameOrPath))
		{
			return Cast<USkeleton>(Asset.GetAsset());
		}
	}

	return nullptr;
}
