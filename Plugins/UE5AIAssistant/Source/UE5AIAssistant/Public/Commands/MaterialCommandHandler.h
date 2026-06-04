// Copyright AI Assistant. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ICommandHandler.h"

/**
 * Handles Material editing commands.
 *
 * Commands:
 *   create_material                - Create a new Material asset
 *   add_material_expression        - Add an expression node to a Material
 *   connect_material_expressions   - Connect two material expression pins
 *   apply_material_to_actor        - Apply a material to an actor's mesh
 *   get_available_materials        - List all materials in the project
 */
class FMaterialCommandHandler : public ICommandHandler
{
public:
	virtual TArray<FString> GetSupportedCommands() const override;
	virtual FCommandResult Execute(const FString& Command, const TSharedPtr<FJsonObject>& Args) override;

private:
	FCommandResult HandleCreateMaterial(const TSharedPtr<FJsonObject>& Args);
	FCommandResult HandleAddExpression(const TSharedPtr<FJsonObject>& Args);
	FCommandResult HandleConnectExpressions(const TSharedPtr<FJsonObject>& Args);
	FCommandResult HandleApplyMaterial(const TSharedPtr<FJsonObject>& Args);
	FCommandResult HandleGetAvailableMaterials(const TSharedPtr<FJsonObject>& Args);
};
