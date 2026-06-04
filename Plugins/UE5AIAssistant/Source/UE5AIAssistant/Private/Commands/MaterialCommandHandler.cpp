// Copyright AI Assistant. All Rights Reserved.

#include "Commands/MaterialCommandHandler.h"
#include "Editor.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Materials/MaterialExpressionConstant.h"
#include "Materials/MaterialExpressionConstant3Vector.h"
#include "Materials/MaterialExpressionConstant4Vector.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Materials/MaterialExpressionTextureSample.h"
#include "Materials/MaterialExpressionTextureCoordinate.h"
#include "Materials/MaterialExpressionMultiply.h"
#include "Materials/MaterialExpressionAdd.h"
#include "Materials/MaterialExpressionLinearInterpolate.h"
#include "Materials/MaterialExpressionFresnel.h"
#include "Materials/MaterialExpressionPanner.h"
#include "Materials/MaterialExpressionTime.h"
#include "MaterialEditingLibrary.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "Engine/StaticMeshActor.h"
#include "Components/StaticMeshComponent.h"
#include "EngineUtils.h"
#include "Factories/MaterialFactoryNew.h"
#include "UObject/UObjectIterator.h"

TArray<FString> FMaterialCommandHandler::GetSupportedCommands() const
{
	return {
		TEXT("create_material"),
		TEXT("add_material_expression"),
		TEXT("connect_material_expressions"),
		TEXT("apply_material_to_actor"),
		TEXT("get_available_materials"),
	};
}

FCommandResult FMaterialCommandHandler::Execute(const FString& Command, const TSharedPtr<FJsonObject>& Args)
{
	if (Command == TEXT("create_material"))               return HandleCreateMaterial(Args);
	if (Command == TEXT("add_material_expression"))       return HandleAddExpression(Args);
	if (Command == TEXT("connect_material_expressions"))  return HandleConnectExpressions(Args);
	if (Command == TEXT("apply_material_to_actor"))       return HandleApplyMaterial(Args);
	if (Command == TEXT("get_available_materials"))       return HandleGetAvailableMaterials(Args);

	return FCommandResult::Fail(FString::Printf(TEXT("MaterialHandler: Unknown command '%s'"), *Command));
}

// ============================================
// create_material
// Args: { "name": "M_RedMetal", "shading_model": "DefaultLit" }
// ============================================
FCommandResult FMaterialCommandHandler::HandleCreateMaterial(const TSharedPtr<FJsonObject>& Args)
{
	FString Name;
	if (!Args->TryGetStringField(TEXT("name"), Name))
	{
		return FCommandResult::Fail(TEXT("Missing required 'name' argument"));
	}

	FString PackagePath = TEXT("/Game/Materials");
	FString PackageName = FString::Printf(TEXT("%s/%s"), *PackagePath, *Name);

	UPackage* Package = CreatePackage(*PackageName);
	if (!Package)
	{
		return FCommandResult::Fail(TEXT("Failed to create package"));
	}

	UMaterialFactoryNew* Factory = NewObject<UMaterialFactoryNew>();
	UMaterial* NewMaterial = Cast<UMaterial>(Factory->FactoryCreateNew(
		UMaterial::StaticClass(),
		Package,
		FName(*Name),
		RF_Public | RF_Standalone,
		nullptr,
		GWarn
	));

	if (!NewMaterial)
	{
		return FCommandResult::Fail(TEXT("Failed to create material"));
	}

	NewMaterial->MarkPackageDirty();
	FAssetRegistryModule::AssetCreated(NewMaterial);

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("name"), Name);
	Data->SetStringField(TEXT("path"), NewMaterial->GetPathName());
	Data->SetStringField(TEXT("message"), FString::Printf(TEXT("Created material '%s'"), *Name));

	return FCommandResult::Success(Data);
}

// ============================================
// add_material_expression
// Args: { "material_name": "M_RedMetal", "expression_type": "VectorParameter", "param_name": "BaseColor", "position_x": -300, "position_y": 0 }
// ============================================
FCommandResult FMaterialCommandHandler::HandleAddExpression(const TSharedPtr<FJsonObject>& Args)
{
	FString MatName;
	if (!Args->TryGetStringField(TEXT("material_name"), MatName))
	{
		return FCommandResult::Fail(TEXT("Missing required 'material_name' argument"));
	}

	FString ExprType;
	if (!Args->TryGetStringField(TEXT("expression_type"), ExprType))
	{
		return FCommandResult::Fail(TEXT("Missing required 'expression_type' argument"));
	}

	// Find the material
	FString MatPath = FString::Printf(TEXT("/Game/Materials/%s.%s"), *MatName, *MatName);
	UMaterial* Material = LoadObject<UMaterial>(nullptr, *MatPath);
	if (!Material)
	{
		// Try searching asset registry
		FAssetRegistryModule& ARModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
		TArray<FAssetData> Assets;
		ARModule.Get().GetAssetsByClass(UMaterial::StaticClass()->GetClassPathName(), Assets, true);
		for (const FAssetData& Asset : Assets)
		{
			if (Asset.AssetName.ToString().Equals(MatName, ESearchCase::IgnoreCase))
			{
				Material = Cast<UMaterial>(Asset.GetAsset());
				break;
			}
		}
	}

	if (!Material)
	{
		return FCommandResult::Fail(FString::Printf(TEXT("Material '%s' not found"), *MatName));
	}

	// Dynamic expression class lookup — search all UMaterialExpression subclasses
	UClass* ExprClass = nullptr;

	for (TObjectIterator<UClass> It; It; ++It)
	{
		if (!It->IsChildOf(UMaterialExpression::StaticClass())) continue;
		if (It->HasAnyClassFlags(CLASS_Abstract)) continue;

		FString ClassName = It->GetName();
		// Strip "MaterialExpression" prefix for matching
		FString ShortName = ClassName;
		ShortName.RemoveFromStart(TEXT("MaterialExpression"));

		if (ClassName.Equals(ExprType, ESearchCase::IgnoreCase) ||
			ShortName.Equals(ExprType, ESearchCase::IgnoreCase) ||
			(TEXT("MaterialExpression") + ExprType).Equals(ClassName, ESearchCase::IgnoreCase))
		{
			ExprClass = *It;
			break;
		}
	}

	if (!ExprClass)
	{
		return FCommandResult::Fail(FString::Printf(
			TEXT("Expression type '%s' not found. Pass any UMaterialExpression subclass name (e.g. Constant, VectorParameter, ScalarParameter, TextureSample, Multiply, Add, Lerp, Fresnel, Panner, Time, etc.)"),
			*ExprType));
	}

	UMaterialExpression* NewExpr = UMaterialEditingLibrary::CreateMaterialExpression(Material, ExprClass);
	if (!NewExpr)
	{
		return FCommandResult::Fail(TEXT("Failed to create material expression"));
	}

	// Set position
	int32 PosX = -300, PosY = 0;
	Args->TryGetNumberField(TEXT("position_x"), PosX);
	Args->TryGetNumberField(TEXT("position_y"), PosY);
	NewExpr->MaterialExpressionEditorX = PosX;
	NewExpr->MaterialExpressionEditorY = PosY;

	// Set parameter name if applicable
	FString ParamName;
	if (Args->TryGetStringField(TEXT("param_name"), ParamName))
	{
		if (UMaterialExpressionScalarParameter* ScalarParam = Cast<UMaterialExpressionScalarParameter>(NewExpr))
		{
			ScalarParam->ParameterName = FName(*ParamName);
		}
		else if (UMaterialExpressionVectorParameter* VecParam = Cast<UMaterialExpressionVectorParameter>(NewExpr))
		{
			VecParam->ParameterName = FName(*ParamName);
		}
	}

	Material->MarkPackageDirty();

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("materialName"), MatName);
	Data->SetStringField(TEXT("expressionType"), ExprType);
	Data->SetStringField(TEXT("expressionName"), NewExpr->GetName());
	Data->SetNumberField(TEXT("expressionIndex"), Material->GetExpressions().Num() - 1);
	Data->SetStringField(TEXT("message"), FString::Printf(TEXT("Added %s expression to '%s'"), *ExprType, *MatName));

	return FCommandResult::Success(Data);
}

// ============================================
// connect_material_expressions
// Args: { "material_name": "M_RedMetal", "from_expression_index": 0, "from_output_index": 0, "to_property": "BaseColor" }
//   OR  { "material_name": "M_RedMetal", "from_expression_index": 0, "from_output_index": 0, "to_expression_index": 1, "to_input_index": 0 }
// ============================================
FCommandResult FMaterialCommandHandler::HandleConnectExpressions(const TSharedPtr<FJsonObject>& Args)
{
	FString MatName;
	if (!Args->TryGetStringField(TEXT("material_name"), MatName))
	{
		return FCommandResult::Fail(TEXT("Missing required 'material_name' argument"));
	}

	FString MatPath = FString::Printf(TEXT("/Game/Materials/%s.%s"), *MatName, *MatName);
	UMaterial* Material = LoadObject<UMaterial>(nullptr, *MatPath);
	if (!Material)
	{
		return FCommandResult::Fail(FString::Printf(TEXT("Material '%s' not found"), *MatName));
	}

	int32 FromIndex = 0, FromOutputIndex = 0;
	Args->TryGetNumberField(TEXT("from_expression_index"), FromIndex);
	Args->TryGetNumberField(TEXT("from_output_index"), FromOutputIndex);

	auto Expressions = Material->GetExpressions();
	if (FromIndex < 0 || FromIndex >= Expressions.Num())
	{
		return FCommandResult::Fail(FString::Printf(TEXT("from_expression_index %d out of range (0-%d)"), FromIndex, Expressions.Num() - 1));
	}

	// Resolve from_output_name for multi-output expressions (e.g. Texture Sample: RGB, R, G, B, A)
	FString FromOutputName;
	if (!Args->TryGetStringField(TEXT("from_output_name"), FromOutputName))
	{
		FromOutputName = TEXT(""); // default = first output
	}

	FString ToProperty;
	if (Args->TryGetStringField(TEXT("to_property"), ToProperty))
	{
		// Dynamic EMaterialProperty resolution via UEnum reflection
		UEnum* MatPropEnum = StaticEnum<EMaterialProperty>();
		EMaterialProperty MatProp = MP_BaseColor; // default

		bool bFoundProp = false;
		if (MatPropEnum)
		{
			// Try: "BaseColor" -> "MP_BaseColor"
			FString WithPrefix = FString::Printf(TEXT("MP_%s"), *ToProperty);
			int64 EnumValue = MatPropEnum->GetValueByNameString(WithPrefix);
			if (EnumValue == INDEX_NONE)
			{
				// Try raw name
				EnumValue = MatPropEnum->GetValueByNameString(ToProperty);
			}
			if (EnumValue == INDEX_NONE)
			{
				// Try case-insensitive search
				for (int32 i = 0; i < MatPropEnum->NumEnums(); i++)
				{
					FString EnumName = MatPropEnum->GetNameStringByIndex(i);
					FString ShortEnum = EnumName;
					ShortEnum.RemoveFromStart(TEXT("MP_"));

					if (EnumName.Equals(ToProperty, ESearchCase::IgnoreCase) ||
						ShortEnum.Equals(ToProperty, ESearchCase::IgnoreCase))
					{
						EnumValue = MatPropEnum->GetValueByIndex(i);
						break;
					}
				}
			}

			if (EnumValue != INDEX_NONE)
			{
				MatProp = static_cast<EMaterialProperty>(EnumValue);
				bFoundProp = true;
			}
		}

		if (!bFoundProp)
		{
			// Build valid list dynamically
			TArray<FString> ValidNames;
			if (MatPropEnum)
			{
				for (int32 i = 0; i < MatPropEnum->NumEnums() - 1; i++)
				{
					FString EName = MatPropEnum->GetNameStringByIndex(i);
					EName.RemoveFromStart(TEXT("MP_"));
					if (!EName.IsEmpty() && EName != TEXT("MAX"))
					{
						ValidNames.Add(EName);
					}
				}
			}
			return FCommandResult::Fail(FString::Printf(
				TEXT("Material property '%s' not found. Valid: %s"),
				*ToProperty, *FString::Join(ValidNames, TEXT(", "))));
		}

		// Connect to material property — single call with correct MatProp
		bool bConnected = UMaterialEditingLibrary::ConnectMaterialProperty(
			Expressions[FromIndex], FromOutputName, MatProp);

		if (!bConnected)
		{
			return FCommandResult::Fail(TEXT("Failed to connect expression to material property"));
		}

		Material->MarkPackageDirty();
		return FCommandResult::SuccessMessage(FString::Printf(TEXT("Connected expression[%d] -> %s"), FromIndex, *ToProperty));
	}
	else
	{
		// Connect to another expression
		int32 ToIndex = 0;
		Args->TryGetNumberField(TEXT("to_expression_index"), ToIndex);

		FString ToInputName;
		if (!Args->TryGetStringField(TEXT("to_input_name"), ToInputName))
		{
			ToInputName = TEXT(""); // default = first input
		}

		if (ToIndex < 0 || ToIndex >= Expressions.Num())
		{
			return FCommandResult::Fail(FString::Printf(TEXT("to_expression_index %d out of range"), ToIndex));
		}

		// Use output/input names for precise pin connections
		bool bConnected = UMaterialEditingLibrary::ConnectMaterialExpressions(
			Expressions[FromIndex], FromOutputName, Expressions[ToIndex], ToInputName);

		if (!bConnected)
		{
			return FCommandResult::Fail(TEXT("Failed to connect expressions"));
		}

		Material->MarkPackageDirty();
		return FCommandResult::SuccessMessage(FString::Printf(TEXT("Connected expression[%d].%s -> expression[%d].%s"),
			FromIndex, FromOutputName.IsEmpty() ? TEXT("(default)") : *FromOutputName,
			ToIndex, ToInputName.IsEmpty() ? TEXT("(default)") : *ToInputName));
	}
}

// ============================================
// apply_material_to_actor
// Args: { "actor_name": "MyCube", "material_name": "M_RedMetal", "slot_index": 0 }
// ============================================
FCommandResult FMaterialCommandHandler::HandleApplyMaterial(const TSharedPtr<FJsonObject>& Args)
{
	FString ActorName, MatName;
	if (!Args->TryGetStringField(TEXT("actor_name"), ActorName))
		return FCommandResult::Fail(TEXT("Missing required 'actor_name' argument"));
	if (!Args->TryGetStringField(TEXT("material_name"), MatName))
		return FCommandResult::Fail(TEXT("Missing required 'material_name' argument"));

	int32 SlotIndex = 0;
	Args->TryGetNumberField(TEXT("slot_index"), SlotIndex);

	// Find the material
	UMaterialInterface* Material = nullptr;
	{
		FString MatPath = FString::Printf(TEXT("/Game/Materials/%s.%s"), *MatName, *MatName);
		Material = LoadObject<UMaterialInterface>(nullptr, *MatPath);
	}

	if (!Material)
	{
		// Search asset registry
		FAssetRegistryModule& ARModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
		TArray<FAssetData> Assets;
		ARModule.Get().GetAssetsByClass(UMaterial::StaticClass()->GetClassPathName(), Assets, true);
		ARModule.Get().GetAssetsByClass(UMaterialInstanceConstant::StaticClass()->GetClassPathName(), Assets, true);
		for (const FAssetData& Asset : Assets)
		{
			if (Asset.AssetName.ToString().Equals(MatName, ESearchCase::IgnoreCase))
			{
				Material = Cast<UMaterialInterface>(Asset.GetAsset());
				break;
			}
		}
	}

	if (!Material)
	{
		return FCommandResult::Fail(FString::Printf(TEXT("Material '%s' not found"), *MatName));
	}

	// Find the actor
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
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

	// Apply material to static mesh component
	UStaticMeshComponent* MeshComp = TargetActor->FindComponentByClass<UStaticMeshComponent>();
	if (!MeshComp)
	{
		return FCommandResult::Fail(FString::Printf(TEXT("Actor '%s' has no StaticMeshComponent"), *ActorName));
	}

	MeshComp->SetMaterial(SlotIndex, Material);
	TargetActor->MarkPackageDirty();

	return FCommandResult::SuccessMessage(FString::Printf(TEXT("Applied material '%s' to '%s' (slot %d)"), *MatName, *ActorName, SlotIndex));
}

// ============================================
// get_available_materials
// Args: { "filter": "" }   (optional substring filter)
// ============================================
FCommandResult FMaterialCommandHandler::HandleGetAvailableMaterials(const TSharedPtr<FJsonObject>& Args)
{
	FString Filter;
	Args->TryGetStringField(TEXT("filter"), Filter);

	FAssetRegistryModule& ARModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	TArray<FAssetData> Assets;
	ARModule.Get().GetAssetsByClass(UMaterial::StaticClass()->GetClassPathName(), Assets, true);
	ARModule.Get().GetAssetsByClass(UMaterialInstanceConstant::StaticClass()->GetClassPathName(), Assets, true);

	TArray<TSharedPtr<FJsonValue>> MatArray;
	for (const FAssetData& Asset : Assets)
	{
		FString AssetPath = Asset.GetObjectPathString();

		// Only show /Game/ assets (skip engine assets) unless filter matches
		if (!Filter.IsEmpty())
		{
			if (!Asset.AssetName.ToString().Contains(Filter, ESearchCase::IgnoreCase) &&
				!AssetPath.Contains(Filter, ESearchCase::IgnoreCase))
			{
				continue;
			}
		}
		else if (!AssetPath.StartsWith(TEXT("/Game/")))
		{
			continue;
		}

		TSharedPtr<FJsonObject> MatObj = MakeShared<FJsonObject>();
		MatObj->SetStringField(TEXT("name"), Asset.AssetName.ToString());
		MatObj->SetStringField(TEXT("path"), AssetPath);
		MatObj->SetStringField(TEXT("class"), Asset.AssetClassPath.GetAssetName().ToString());
		MatArray.Add(MakeShared<FJsonValueObject>(MatObj));

		if (MatArray.Num() >= 100) break; // Cap results
	}

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetArrayField(TEXT("materials"), MatArray);
	Data->SetNumberField(TEXT("count"), MatArray.Num());

	return FCommandResult::Success(Data);
}
