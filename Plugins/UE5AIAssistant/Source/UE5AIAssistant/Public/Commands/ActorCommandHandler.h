// Copyright AI Assistant. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ICommandHandler.h"

/**
 * Handles all Actor-related commands.
 * 
 * Commands:
 *   get_actors_in_level  - List all actors in the current level
 *   get_selected_actors  - Get currently selected actors in editor
 *   find_actors_by_name  - Find actors matching a name pattern
 *   spawn_actor          - Create a new actor in the level
 *   delete_actor         - Remove an actor from the level
 *   set_actor_transform  - Set actor location/rotation/scale
 *   get_actor_properties - Get all properties of an actor
 *   set_actor_property   - Set a specific property on an actor
 *   attach_actor         - Attach one actor to another (parent-child)
 *   detach_actor         - Detach an actor from its parent
 */
class FActorCommandHandler : public ICommandHandler
{
public:
	virtual TArray<FString> GetSupportedCommands() const override;
	virtual FCommandResult Execute(const FString& Command, const TSharedPtr<FJsonObject>& Args) override;

private:
	FCommandResult HandleGetActorsInLevel(const TSharedPtr<FJsonObject>& Args);
	FCommandResult HandleGetSelectedActors(const TSharedPtr<FJsonObject>& Args);
	FCommandResult HandleFindActorsByName(const TSharedPtr<FJsonObject>& Args);
	FCommandResult HandleSpawnActor(const TSharedPtr<FJsonObject>& Args);
	FCommandResult HandleDeleteActor(const TSharedPtr<FJsonObject>& Args);
	FCommandResult HandleSetActorTransform(const TSharedPtr<FJsonObject>& Args);
	FCommandResult HandleGetActorProperties(const TSharedPtr<FJsonObject>& Args);
	FCommandResult HandleSetActorProperty(const TSharedPtr<FJsonObject>& Args);
	FCommandResult HandleAttachActor(const TSharedPtr<FJsonObject>& Args);
	FCommandResult HandleDetachActor(const TSharedPtr<FJsonObject>& Args);

	/** Helper: Find an actor in the current world by label or name */
	AActor* FindActorByName(const FString& Name) const;

	/** Helper: Get the current editor world */
	UWorld* GetEditorWorld() const;

	/** Helper: Serialize actor info to JSON */
	TSharedPtr<FJsonObject> ActorToJson(AActor* Actor) const;

	/** Helper: Parse a [X, Y, Z] JSON array to FVector */
	bool JsonArrayToVector(const TSharedPtr<FJsonObject>& Args, const FString& FieldName, FVector& OutVector) const;
};
