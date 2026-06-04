// Copyright AI Assistant. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ICommandHandler.h"

class UAnimBlueprint;
class UEdGraph;
class UAnimStateNode;
class UAnimationStateMachineGraph;

/**
 * Handles Animation Blueprint commands: creating anim blueprints, state machines,
 * states, transitions, and assigning animation assets.
 *
 * Commands:
 *   create_anim_blueprint      - Create a new Animation Blueprint asset
 *   get_anim_blueprint_info    - Read full structure (state machines, variables, skeleton)
 *   add_anim_state_machine     - Add a new state machine to the AnimGraph
 *   add_anim_state             - Add a state to a state machine
 *   add_anim_transition        - Add a transition between two states
 *   set_anim_state_animation   - Set the animation played in a state
 *   compile_anim_blueprint     - Compile an Animation Blueprint
 */
class FAnimationCommandHandler : public ICommandHandler
{
public:
	virtual TArray<FString> GetSupportedCommands() const override;
	virtual FCommandResult Execute(const FString& Command, const TSharedPtr<FJsonObject>& Args) override;

private:
	FCommandResult HandleCreateAnimBlueprint(const TSharedPtr<FJsonObject>& Args);
	FCommandResult HandleGetAnimBlueprintInfo(const TSharedPtr<FJsonObject>& Args);
	FCommandResult HandleAddAnimStateMachine(const TSharedPtr<FJsonObject>& Args);
	FCommandResult HandleAddAnimState(const TSharedPtr<FJsonObject>& Args);
	FCommandResult HandleAddAnimTransition(const TSharedPtr<FJsonObject>& Args);
	FCommandResult HandleSetAnimStateAnimation(const TSharedPtr<FJsonObject>& Args);
	FCommandResult HandleCompileAnimBlueprint(const TSharedPtr<FJsonObject>& Args);

	/** Find an Animation Blueprint by name */
	UAnimBlueprint* FindAnimBlueprintByName(const FString& Name) const;

	/** Find a state machine graph within an Anim Blueprint by name */
	UAnimationStateMachineGraph* FindStateMachineGraph(UAnimBlueprint* AnimBP, const FString& StateMachineName) const;

	/** Find a state node within a state machine graph */
	UAnimStateNode* FindStateNode(UAnimationStateMachineGraph* SMGraph, const FString& StateName) const;

	/** Find a skeleton asset by name or path */
	class USkeleton* FindSkeletonByName(const FString& NameOrPath) const;
};
