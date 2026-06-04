// Copyright AI Assistant. All Rights Reserved.

#include "Commands/GenericPropertyHandler.h"
#include "Editor.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/SCS_Node.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Character.h"
#include "Components/ActorComponent.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "UObject/UnrealType.h"
#include "UObject/TextProperty.h"
#include "UObject/PropertyIterator.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "Misc/Guid.h"
// No extra includes needed for execute_python — uses GEditor->Exec

TArray<FString> FGenericPropertyHandler::GetSupportedCommands() const
{
	return {
		TEXT("get_component_property"),
		TEXT("set_component_property"),
		TEXT("list_components"),
		TEXT("list_properties"),
		TEXT("create_asset"),
		TEXT("get_asset_property"),
		TEXT("set_asset_property"),
		TEXT("call_function"),
		TEXT("get_object"),
		TEXT("modify_array_property"),
		TEXT("execute_python"),
	};
}

FCommandResult FGenericPropertyHandler::Execute(const FString& Command, const TSharedPtr<FJsonObject>& Args)
{
	if (Command == TEXT("get_component_property"))  return HandleGetComponentProperty(Args);
	if (Command == TEXT("set_component_property"))  return HandleSetComponentProperty(Args);
	if (Command == TEXT("list_components"))          return HandleListComponents(Args);
	if (Command == TEXT("list_properties"))          return HandleListProperties(Args);
	if (Command == TEXT("create_asset"))             return HandleCreateAsset(Args);
	if (Command == TEXT("get_asset_property"))       return HandleGetAssetProperty(Args);
	if (Command == TEXT("set_asset_property"))       return HandleSetAssetProperty(Args);
	if (Command == TEXT("call_function"))             return HandleCallFunction(Args);
	if (Command == TEXT("get_object"))                return HandleGetObject(Args);
	if (Command == TEXT("modify_array_property"))     return HandleModifyArrayProperty(Args);
	if (Command == TEXT("execute_python"))            return HandleExecutePython(Args);

	return FCommandResult::Fail(FString::Printf(TEXT("GenericPropertyHandler: Unknown command '%s'"), *Command));
}

// ============================================
// get_component_property
// Read any property from a component on a Blueprint CDO.
// Args: {
//   "blueprint_name": "BP_MyCharacter",
//   "component_name": "CharacterMovement0",   // or class-based: "CharacterMovementComponent"
//   "property_name": "MaxWalkSpeed"            // or omit to get ALL properties
// }
// ============================================
FCommandResult FGenericPropertyHandler::HandleGetComponentProperty(const TSharedPtr<FJsonObject>& Args)
{
	FString BPName, CompName;
	if (!Args->TryGetStringField(TEXT("blueprint_name"), BPName))
		return FCommandResult::Fail(TEXT("Missing 'blueprint_name'"));
	if (!Args->TryGetStringField(TEXT("component_name"), CompName))
		return FCommandResult::Fail(TEXT("Missing 'component_name'"));

	UBlueprint* Blueprint = FindBlueprintByName(BPName);
	if (!Blueprint)
		return FCommandResult::Fail(FString::Printf(TEXT("Blueprint '%s' not found"), *BPName));

	UActorComponent* Component = FindComponentOnCDO(Blueprint, CompName);
	if (!Component)
		return FCommandResult::Fail(FString::Printf(TEXT("Component '%s' not found on '%s'. Use 'list_components' to see available components."), *CompName, *BPName));

	FString PropertyName;
	if (Args->TryGetStringField(TEXT("property_name"), PropertyName) && !PropertyName.IsEmpty())
	{
		// Read a single property
		FString Value, Type;
		if (!ReadPropertyValue(Component, PropertyName, Value, Type))
		{
			return FCommandResult::Fail(FString::Printf(TEXT("Property '%s' not found on '%s'. Use 'list_properties' to see available properties."), *PropertyName, *CompName));
		}

		TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
		Data->SetStringField(TEXT("blueprintName"), BPName);
		Data->SetStringField(TEXT("componentName"), CompName);
		Data->SetStringField(TEXT("componentClass"), Component->GetClass()->GetName());
		Data->SetStringField(TEXT("propertyName"), PropertyName);
		Data->SetStringField(TEXT("propertyType"), Type);
		Data->SetStringField(TEXT("value"), Value);
		return FCommandResult::Success(Data);
	}
	else
	{
		// Read ALL properties
		TSharedPtr<FJsonObject> Data = PropertiesToJson(Component);
		Data->SetStringField(TEXT("blueprintName"), BPName);
		Data->SetStringField(TEXT("componentName"), CompName);
		Data->SetStringField(TEXT("componentClass"), Component->GetClass()->GetName());
		return FCommandResult::Success(Data);
	}
}

// ============================================
// set_component_property
// Args: {
//   "blueprint_name": "BP_MyCharacter",
//   "component_name": "CharacterMovement0",
//   "property_name": "MaxWalkSpeed",
//   "value": "600.0"
// }
// Supports batch: "properties": { "MaxWalkSpeed": "600", "JumpZVelocity": "420" }
// ============================================
FCommandResult FGenericPropertyHandler::HandleSetComponentProperty(const TSharedPtr<FJsonObject>& Args)
{
	FString BPName, CompName;
	if (!Args->TryGetStringField(TEXT("blueprint_name"), BPName))
		return FCommandResult::Fail(TEXT("Missing 'blueprint_name'"));
	if (!Args->TryGetStringField(TEXT("component_name"), CompName))
		return FCommandResult::Fail(TEXT("Missing 'component_name'"));

	UBlueprint* Blueprint = FindBlueprintByName(BPName);
	if (!Blueprint)
		return FCommandResult::Fail(FString::Printf(TEXT("Blueprint '%s' not found"), *BPName));

	UActorComponent* Component = FindComponentOnCDO(Blueprint, CompName);
	if (!Component)
		return FCommandResult::Fail(FString::Printf(TEXT("Component '%s' not found on '%s'"), *CompName, *BPName));

	TArray<FString> ChangedProperties;

	// Check for batch mode: "properties": { "A": "1", "B": "2" }
	const TSharedPtr<FJsonObject>* PropsObj;
	if (Args->TryGetObjectField(TEXT("properties"), PropsObj))
	{
		for (auto& Pair : (*PropsObj)->Values)
		{
			FString PropValue;
			if (Pair.Value->TryGetString(PropValue))
			{
				FString Error;
				if (WritePropertyValue(Component, Pair.Key, PropValue, Error))
				{
					ChangedProperties.Add(FString::Printf(TEXT("%s=%s"), *Pair.Key, *PropValue));
				}
				else
				{
					return FCommandResult::Fail(FString::Printf(TEXT("Failed to set '%s': %s"), *Pair.Key, *Error));
				}
			}
		}
	}
	else
	{
		// Single property mode
		FString PropertyName, Value;
		if (!Args->TryGetStringField(TEXT("property_name"), PropertyName))
			return FCommandResult::Fail(TEXT("Missing 'property_name' (or use 'properties' object for batch)"));
		if (!Args->TryGetStringField(TEXT("value"), Value))
			return FCommandResult::Fail(TEXT("Missing 'value'"));

		FString Error;
		if (!WritePropertyValue(Component, PropertyName, Value, Error))
		{
			return FCommandResult::Fail(FString::Printf(TEXT("Failed to set '%s': %s"), *PropertyName, *Error));
		}
		ChangedProperties.Add(FString::Printf(TEXT("%s=%s"), *PropertyName, *Value));
	}

	Component->PostEditChange();
	Blueprint->MarkPackageDirty();
	FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("blueprintName"), BPName);
	Data->SetStringField(TEXT("componentName"), CompName);
	Data->SetNumberField(TEXT("propertiesChanged"), ChangedProperties.Num());
	TArray<TSharedPtr<FJsonValue>> Arr;
	for (const FString& S : ChangedProperties)
		Arr.Add(MakeShared<FJsonValueString>(S));
	Data->SetArrayField(TEXT("changes"), Arr);
	Data->SetStringField(TEXT("message"), FString::Printf(TEXT("Set %d properties on '%s.%s'"), ChangedProperties.Num(), *BPName, *CompName));
	return FCommandResult::Success(Data);
}

// ============================================
// list_components
// Args: { "blueprint_name": "BP_MyCharacter" }
// ============================================
FCommandResult FGenericPropertyHandler::HandleListComponents(const TSharedPtr<FJsonObject>& Args)
{
	FString BPName;
	if (!Args->TryGetStringField(TEXT("blueprint_name"), BPName))
		return FCommandResult::Fail(TEXT("Missing 'blueprint_name'"));

	UBlueprint* Blueprint = FindBlueprintByName(BPName);
	if (!Blueprint)
		return FCommandResult::Fail(FString::Printf(TEXT("Blueprint '%s' not found"), *BPName));

	UObject* CDO = Blueprint->GeneratedClass ? Blueprint->GeneratedClass->GetDefaultObject() : nullptr;
	if (!CDO)
		return FCommandResult::Fail(TEXT("Blueprint has no generated class / CDO"));

	AActor* ActorCDO = Cast<AActor>(CDO);
	TArray<TSharedPtr<FJsonValue>> CompArray;

	if (ActorCDO)
	{
		TArray<UActorComponent*> Components;
		ActorCDO->GetComponents(Components);

		for (UActorComponent* Comp : Components)
		{
			if (!Comp) continue;
			TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
			Obj->SetStringField(TEXT("name"), Comp->GetName());
			Obj->SetStringField(TEXT("class"), Comp->GetClass()->GetName());

			// Count editable properties
			int32 PropCount = 0;
			for (TFieldIterator<FProperty> It(Comp->GetClass()); It; ++It)
			{
				if (It->HasAnyPropertyFlags(CPF_Edit))
					PropCount++;
			}
			Obj->SetNumberField(TEXT("editableProperties"), PropCount);
			CompArray.Add(MakeShared<FJsonValueObject>(Obj));
		}
	}

	// Also include SCS components (Blueprint-added)
	if (Blueprint->SimpleConstructionScript)
	{
		for (USCS_Node* Node : Blueprint->SimpleConstructionScript->GetAllNodes())
		{
			if (!Node || !Node->ComponentTemplate) continue;

			// Check if already listed from CDO
			bool bAlreadyListed = false;
			for (const auto& Existing : CompArray)
			{
				FString ExistingName;
				if (Existing->AsObject()->TryGetStringField(TEXT("name"), ExistingName) &&
					ExistingName == Node->GetVariableName().ToString())
				{
					bAlreadyListed = true;
					break;
				}
			}
			if (bAlreadyListed) continue;

			TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
			Obj->SetStringField(TEXT("name"), Node->GetVariableName().ToString());
			Obj->SetStringField(TEXT("class"), Node->ComponentTemplate->GetClass()->GetName());
			Obj->SetStringField(TEXT("source"), TEXT("SCS"));

			// Count editable properties on SCS template (same as CDO components)
			int32 PropCount = 0;
			for (TFieldIterator<FProperty> PropIt(Node->ComponentTemplate->GetClass()); PropIt; ++PropIt)
			{
				if (PropIt->HasAnyPropertyFlags(CPF_Edit))
					PropCount++;
			}
			Obj->SetNumberField(TEXT("editableProperties"), PropCount);
			CompArray.Add(MakeShared<FJsonValueObject>(Obj));
		}
	}

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("blueprintName"), BPName);
	Data->SetNumberField(TEXT("componentCount"), CompArray.Num());
	Data->SetArrayField(TEXT("components"), CompArray);
	return FCommandResult::Success(Data);
}

// ============================================
// list_properties
// Args: {
//   "blueprint_name": "BP_MyCharacter",
//   "component_name": "CharacterMovement0",   // OR
//   "asset_name": "IA_Move",                   // for asset properties
//   "filter": "Speed"                           // optional substring filter
// }
// ============================================
FCommandResult FGenericPropertyHandler::HandleListProperties(const TSharedPtr<FJsonObject>& Args)
{
	UObject* TargetObject = nullptr;
	FString TargetName;

	// Determine target: component on blueprint, node in graph, or standalone asset/object
	FString BPName, CompName;
	if (Args->TryGetStringField(TEXT("blueprint_name"), BPName) &&
		Args->TryGetStringField(TEXT("component_name"), CompName))
	{
		// Component-on-CDO path (original behavior)
		UBlueprint* Blueprint = FindBlueprintByName(BPName);
		if (!Blueprint)
			return FCommandResult::Fail(FString::Printf(TEXT("Blueprint '%s' not found"), *BPName));
		TargetObject = FindComponentOnCDO(Blueprint, CompName);
		if (!TargetObject)
			return FCommandResult::Fail(FString::Printf(TEXT("Component '%s' not found"), *CompName));
		TargetName = FString::Printf(TEXT("%s.%s"), *BPName, *CompName);
	}
	else
	{
		// Universal resolution: object_path, asset_name, or node_id+blueprint_name
		FString Error;
		TargetObject = ResolveTargetObject(Args, Error);
		if (!TargetObject)
			return FCommandResult::Fail(Error);
		TargetName = TargetObject->GetName();
	}

	FString Filter;
	Args->TryGetStringField(TEXT("filter"), Filter);

	// For UEdGraphNode targets, show all UPROPERTY fields (nodes often lack CPF_Edit)
	bool bIsGraphNode = TargetObject->IsA<UEdGraphNode>();
	bool bIncludeAll = bIsGraphNode;
	Args->TryGetBoolField(TEXT("include_all"), bIncludeAll);

	TArray<TSharedPtr<FJsonValue>> PropArray;
	for (TFieldIterator<FProperty> It(TargetObject->GetClass()); It; ++It)
	{
		FProperty* Prop = *It;

		// Skip non-editable unless include_all is set
		if (!bIncludeAll && !Prop->HasAnyPropertyFlags(CPF_Edit))
			continue;

		// Always skip transient / deprecated / delegate types for readability
		if (Prop->HasAnyPropertyFlags(CPF_Deprecated))
			continue;

		FString PropName = Prop->GetName();
		if (!Filter.IsEmpty() && !PropName.Contains(Filter, ESearchCase::IgnoreCase))
			continue;

		TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
		Obj->SetStringField(TEXT("name"), PropName);
		Obj->SetStringField(TEXT("type"), Prop->GetCPPType());

		FString Category = Prop->GetMetaData(TEXT("Category"));
		if (!Category.IsEmpty())
			Obj->SetStringField(TEXT("category"), Category);

		// Property flags for AI context
		if (Prop->HasAnyPropertyFlags(CPF_Edit))
			Obj->SetBoolField(TEXT("editable"), true);

		// Read current value (safely)
		FString Value;
		void* ValuePtr = Prop->ContainerPtrToValuePtr<void>(TargetObject);
		Prop->ExportTextItem_Direct(Value, ValuePtr, nullptr, TargetObject, PPF_None);
		if (!Value.IsEmpty())
			Obj->SetStringField(TEXT("currentValue"), Value);

		PropArray.Add(MakeShared<FJsonValueObject>(Obj));
	}

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("target"), TargetName);
	Data->SetStringField(TEXT("targetClass"), TargetObject->GetClass()->GetName());
	Data->SetStringField(TEXT("targetPath"), TargetObject->GetPathName());
	Data->SetNumberField(TEXT("propertyCount"), PropArray.Num());
	Data->SetArrayField(TEXT("properties"), PropArray);
	return FCommandResult::Success(Data);
}

// ============================================
// create_asset
// Generic asset creation by class name.
// Args: {
//   "asset_name": "IA_Move",
//   "asset_class": "InputAction",
//   "package_path": "/Game/Input"     (optional, defaults to /Game/<ClassName>s)
// }
// ============================================
FCommandResult FGenericPropertyHandler::HandleCreateAsset(const TSharedPtr<FJsonObject>& Args)
{
	FString AssetName, AssetClassName;
	if (!Args->TryGetStringField(TEXT("asset_name"), AssetName))
		return FCommandResult::Fail(TEXT("Missing 'asset_name'"));
	if (!Args->TryGetStringField(TEXT("asset_class"), AssetClassName))
		return FCommandResult::Fail(TEXT("Missing 'asset_class'"));

	// Resolve the UClass
	UClass* AssetClass = nullptr;

	// Try common prefixes
	TArray<FString> SearchNames = {
		AssetClassName,
		FString::Printf(TEXT("U%s"), *AssetClassName),
	};

	// Search across all loaded modules
	TArray<FString> SearchModules = {
		TEXT("/Script/Engine"),
		TEXT("/Script/EnhancedInput"),
		TEXT("/Script/CoreUObject"),
		TEXT("/Script/InputCore"),
		TEXT("/Script/UMG"),
		TEXT("/Script/AIModule"),
		TEXT("/Script/Niagara"),
		TEXT("/Script/AudioMixer"),
		TEXT("/Script/MediaAssets"),
		TEXT("/Script/GameplayAbilities"),
	};

	for (const FString& ModulePath : SearchModules)
	{
		for (const FString& ClassName : SearchNames)
		{
			AssetClass = FindFirstObject<UClass>(*ClassName, EFindFirstObjectOptions::NativeFirst);
			if (AssetClass) break;
		}
		if (AssetClass) break;
	}

	// Fallback: try FindFirstObjectSafe for classes in any module
	if (!AssetClass)
	{
		for (const FString& ClassName : SearchNames)
		{
			AssetClass = FindFirstObject<UClass>(*ClassName, EFindFirstObjectOptions::NativeFirst);
			if (AssetClass) break;
		}
	}

	if (!AssetClass)
	{
		return FCommandResult::Fail(FString::Printf(TEXT("Class '%s' not found. Examples: InputAction, InputMappingContext, SoundCue, DataTable, CurveFloat"), *AssetClassName));
	}

	FString PackagePath;
	if (!Args->TryGetStringField(TEXT("package_path"), PackagePath))
	{
		PackagePath = FString::Printf(TEXT("/Game/%ss"), *AssetClassName);
	}

	// Check if asset already exists
	FString FullPath = FString::Printf(TEXT("%s/%s.%s"), *PackagePath, *AssetName, *AssetName);
	FAssetRegistryModule& AssetRegMod = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	FAssetData ExistingAsset = AssetRegMod.Get().GetAssetByObjectPath(FSoftObjectPath(FullPath));
	if (ExistingAsset.IsValid())
	{
		return FCommandResult::Fail(FString::Printf(TEXT("Asset '%s' already exists at '%s'"), *AssetName, *PackagePath));
	}

	// Use IAssetTools to create the asset properly (via factory system)
	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();

	// Find a factory for this class
	UFactory* Factory = nullptr;
	for (TObjectIterator<UClass> It; It; ++It)
	{
		if (It->IsChildOf(UFactory::StaticClass()) && !It->HasAnyClassFlags(CLASS_Abstract))
		{
			UFactory* TestFactory = It->GetDefaultObject<UFactory>();
			if (TestFactory && TestFactory->SupportedClass == AssetClass && TestFactory->CanCreateNew())
			{
				Factory = NewObject<UFactory>(GetTransientPackage(), *It);
				break;
			}
		}
	}

	UObject* NewAsset = nullptr;
	if (Factory)
	{
		// Use factory path — correct initialization with subobjects
		NewAsset = AssetTools.CreateAsset(AssetName, PackagePath, AssetClass, Factory);
	}
	else
	{
		// Fallback for classes without a dedicated factory
		UPackage* Package = CreatePackage(*FString::Printf(TEXT("%s/%s"), *PackagePath, *AssetName));
		if (!Package)
			return FCommandResult::Fail(TEXT("Failed to create package"));

		NewAsset = NewObject<UObject>(Package, AssetClass, FName(*AssetName), RF_Public | RF_Standalone);
		if (NewAsset)
		{
			NewAsset->MarkPackageDirty();
			FAssetRegistryModule::AssetCreated(NewAsset);
		}
	}

	if (!NewAsset)
		return FCommandResult::Fail(FString::Printf(TEXT("Failed to create asset of class '%s'"), *AssetClassName));

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("name"), AssetName);
	Data->SetStringField(TEXT("class"), AssetClass->GetName());
	Data->SetStringField(TEXT("path"), NewAsset->GetPathName());
	Data->SetStringField(TEXT("message"), FString::Printf(TEXT("Created %s '%s' at %s"), *AssetClass->GetName(), *AssetName, *PackagePath));
	return FCommandResult::Success(Data);
}

// ============================================
// get_asset_property
// Args: {
//   "asset_name": "IA_Move",
//   "property_name": "ValueType"     // or omit to get ALL properties
// }
// ============================================
FCommandResult FGenericPropertyHandler::HandleGetAssetProperty(const TSharedPtr<FJsonObject>& Args)
{
	FString Error;
	UObject* Asset = ResolveTargetObject(Args, Error);
	if (!Asset)
		return FCommandResult::Fail(Error);

	FString PropertyName;
	if (Args->TryGetStringField(TEXT("property_name"), PropertyName) && !PropertyName.IsEmpty())
	{
		FString Value, Type;
		if (!ReadPropertyValue(Asset, PropertyName, Value, Type))
			return FCommandResult::Fail(FString::Printf(TEXT("Property '%s' not found on '%s' (%s)"), *PropertyName, *Asset->GetName(), *Asset->GetClass()->GetName()));

		TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
		Data->SetStringField(TEXT("objectName"), Asset->GetName());
		Data->SetStringField(TEXT("objectClass"), Asset->GetClass()->GetName());
		Data->SetStringField(TEXT("propertyName"), PropertyName);
		Data->SetStringField(TEXT("propertyType"), Type);
		Data->SetStringField(TEXT("value"), Value);
		return FCommandResult::Success(Data);
	}
	else
	{
		TSharedPtr<FJsonObject> Data = PropertiesToJson(Asset);
		Data->SetStringField(TEXT("objectName"), Asset->GetName());
		Data->SetStringField(TEXT("objectClass"), Asset->GetClass()->GetName());
		Data->SetStringField(TEXT("objectPath"), Asset->GetPathName());
		return FCommandResult::Success(Data);
	}
}

// ============================================
// set_asset_property
// Args: {
//   "asset_name": "IA_Move",
//   "property_name": "ValueType",
//   "value": "Axis2D"
// }
// Or batch: "properties": { "ValueType": "Axis2D", ... }
// ============================================
FCommandResult FGenericPropertyHandler::HandleSetAssetProperty(const TSharedPtr<FJsonObject>& Args)
{
	FString ResolveError;
	UObject* Asset = ResolveTargetObject(Args, ResolveError);
	if (!Asset)
		return FCommandResult::Fail(ResolveError);

	TArray<FString> ChangedProperties;

	const TSharedPtr<FJsonObject>* PropsObj;
	if (Args->TryGetObjectField(TEXT("properties"), PropsObj))
	{
		for (auto& Pair : (*PropsObj)->Values)
		{
			FString PropValue;
			if (Pair.Value->TryGetString(PropValue))
			{
				FString Error;
				if (WritePropertyValue(Asset, Pair.Key, PropValue, Error))
					ChangedProperties.Add(FString::Printf(TEXT("%s=%s"), *Pair.Key, *PropValue));
				else
					return FCommandResult::Fail(FString::Printf(TEXT("Failed to set '%s': %s"), *Pair.Key, *Error));
			}
		}
	}
	else
	{
		FString PropertyName, Value;
		if (!Args->TryGetStringField(TEXT("property_name"), PropertyName))
			return FCommandResult::Fail(TEXT("Missing 'property_name'"));
		if (!Args->TryGetStringField(TEXT("value"), Value))
			return FCommandResult::Fail(TEXT("Missing 'value'"));

		FString Error;
		if (!WritePropertyValue(Asset, PropertyName, Value, Error))
			return FCommandResult::Fail(FString::Printf(TEXT("Failed to set '%s': %s"), *PropertyName, *Error));
		ChangedProperties.Add(FString::Printf(TEXT("%s=%s"), *PropertyName, *Value));
	}

	Asset->PostEditChange();
	Asset->MarkPackageDirty();

	// Auto-reconstruct: If the target is a UEdGraphNode, call ReconstructNode()
	// to refresh its pins after property changes. This is critical for nodes like
	// K2Node_EnhancedInputAction where setting InputAction must update the
	// ActionValue pin type (bool vs Vector2D etc.), otherwise compilation crashes
	// in ExpandNode with SIGSEGV when pin types don't match.
	bool bReconstructed = false;
	UEdGraphNode* GraphNode = Cast<UEdGraphNode>(Asset);
	if (GraphNode)
	{
		GraphNode->ReconstructNode();
		bReconstructed = true;
		UE_LOG(LogTemp, Log, TEXT("[UE5AI] set_asset_property: Auto-reconstructed node '%s' (%d pins)"),
			*GraphNode->GetClass()->GetName(), GraphNode->Pins.Num());
	}

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("objectName"), Asset->GetName());
	Data->SetStringField(TEXT("objectClass"), Asset->GetClass()->GetName());
	Data->SetNumberField(TEXT("propertiesChanged"), ChangedProperties.Num());
	if (bReconstructed)
	{
		Data->SetBoolField(TEXT("reconstructed"), true);
		Data->SetNumberField(TEXT("pinCount"), GraphNode ? GraphNode->Pins.Num() : 0);
	}
	TArray<TSharedPtr<FJsonValue>> Arr;
	for (const FString& S : ChangedProperties)
		Arr.Add(MakeShared<FJsonValueString>(S));
	Data->SetArrayField(TEXT("changes"), Arr);
	Data->SetStringField(TEXT("message"), FString::Printf(TEXT("Set %d properties on '%s'%s"), ChangedProperties.Num(), *Asset->GetName(), bReconstructed ? TEXT(" (reconstructed)") : TEXT("")));
	return FCommandResult::Success(Data);
}

// ============================================
// Helpers: Reflection
// ============================================

bool FGenericPropertyHandler::ReadPropertyValue(UObject* Object, const FString& PropertyPath, FString& OutValue, FString& OutType) const
{
	if (!Object) return false;

	FProperty* Prop = Object->GetClass()->FindPropertyByName(FName(*PropertyPath));
	if (!Prop)
	{
		// Try case-insensitive search
		for (TFieldIterator<FProperty> It(Object->GetClass()); It; ++It)
		{
			if (It->GetName().Equals(PropertyPath, ESearchCase::IgnoreCase))
			{
				Prop = *It;
				break;
			}
		}
	}
	if (!Prop) return false;

	void* ValuePtr = Prop->ContainerPtrToValuePtr<void>(Object);
	Prop->ExportTextItem_Direct(OutValue, ValuePtr, nullptr, Object, PPF_None);
	OutType = Prop->GetCPPType();
	return true;
}

bool FGenericPropertyHandler::WritePropertyValue(UObject* Object, const FString& PropertyPath, const FString& Value, FString& OutError) const
{
	if (!Object)
	{
		OutError = TEXT("Null object");
		return false;
	}

	FProperty* Prop = Object->GetClass()->FindPropertyByName(FName(*PropertyPath));
	if (!Prop)
	{
		// Case-insensitive fallback
		for (TFieldIterator<FProperty> It(Object->GetClass()); It; ++It)
		{
			if (It->GetName().Equals(PropertyPath, ESearchCase::IgnoreCase))
			{
				Prop = *It;
				break;
			}
		}
	}
	if (!Prop)
	{
		OutError = FString::Printf(TEXT("Property '%s' not found on %s"), *PropertyPath, *Object->GetClass()->GetName());
		return false;
	}

	void* ValuePtr = Prop->ContainerPtrToValuePtr<void>(Object);
	if (!Prop->ImportText_Direct(*Value, ValuePtr, Object, PPF_None))
	{
		OutError = FString::Printf(TEXT("Failed to parse value '%s' for property '%s' (%s)"), *Value, *PropertyPath, *Prop->GetCPPType());
		return false;
	}

	return true;
}

TSharedPtr<FJsonObject> FGenericPropertyHandler::PropertiesToJson(UObject* Object, bool bEditableOnly) const
{
	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	if (!Object) return Result;

	TSharedPtr<FJsonObject> PropsObj = MakeShared<FJsonObject>();
	int32 Count = 0;

	for (TFieldIterator<FProperty> It(Object->GetClass()); It; ++It)
	{
		FProperty* Prop = *It;
		if (bEditableOnly && !Prop->HasAnyPropertyFlags(CPF_Edit))
			continue;

		FString Value;
		void* ValuePtr = Prop->ContainerPtrToValuePtr<void>(Object);
		Prop->ExportTextItem_Direct(Value, ValuePtr, nullptr, Object, PPF_None);

		PropsObj->SetStringField(Prop->GetName(), Value);
		Count++;
	}

	Result->SetObjectField(TEXT("properties"), PropsObj);
	Result->SetNumberField(TEXT("propertyCount"), Count);
	return Result;
}

// ============================================
// Helpers: Finders
// ============================================

UBlueprint* FGenericPropertyHandler::FindBlueprintByName(const FString& Name) const
{
	FAssetRegistryModule& Mod = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	IAssetRegistry& Registry = Mod.Get();

	TArray<FAssetData> Assets;
	Registry.GetAssetsByClass(UBlueprint::StaticClass()->GetClassPathName(), Assets, true);

	for (const FAssetData& Asset : Assets)
	{
		if (Asset.AssetName.ToString().Equals(Name, ESearchCase::IgnoreCase))
			return Cast<UBlueprint>(Asset.GetAsset());
	}

	FString Path = FString::Printf(TEXT("/Game/Blueprints/%s.%s"), *Name, *Name);
	return LoadObject<UBlueprint>(nullptr, *Path);
}

UObject* FGenericPropertyHandler::FindAssetByName(const FString& Name) const
{
	FAssetRegistryModule& Mod = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	IAssetRegistry& Registry = Mod.Get();

	// First try: exact name match via FARFilter (fast, indexed lookup)
	FARFilter Filter;
	Filter.PackagePaths.Add(FName(TEXT("/Game")));
	Filter.bRecursivePaths = true;

	TArray<FAssetData> Assets;
	Registry.GetAssets(Filter, Assets);

	for (const FAssetData& Asset : Assets)
	{
		if (Asset.AssetName.ToString().Equals(Name, ESearchCase::IgnoreCase))
			return Asset.GetAsset();
	}

	// Fallback: try direct path patterns
	TArray<FString> PathPatterns = {
		FString::Printf(TEXT("/Game/%s.%s"), *Name, *Name),
		FString::Printf(TEXT("/Game/Blueprints/%s.%s"), *Name, *Name),
		FString::Printf(TEXT("/Game/Input/%s.%s"), *Name, *Name),
		FString::Printf(TEXT("/Game/Input/Actions/%s.%s"), *Name, *Name),
	};

	for (const FString& Path : PathPatterns)
	{
		UObject* Loaded = LoadObject<UObject>(nullptr, *Path);
		if (Loaded) return Loaded;
	}

	return nullptr;
}

// ============================================
// Universal object resolution
// Supports: object_path, asset_name, node_id + blueprint_name [+ graph_name]
// ============================================

UObject* FGenericPropertyHandler::ResolveTargetObject(const TSharedPtr<FJsonObject>& Args, FString& OutError) const
{
	FString ObjectPath, AssetName, NodeId;

	// Path 1: Direct object path (highest priority)
	if (Args->TryGetStringField(TEXT("object_path"), ObjectPath))
	{
		UObject* Obj = StaticFindObject(UObject::StaticClass(), nullptr, *ObjectPath);
		if (!Obj) Obj = LoadObject<UObject>(nullptr, *ObjectPath);
		if (!Obj) OutError = FString::Printf(TEXT("Object not found at path '%s'"), *ObjectPath);
		return Obj;
	}

	// Path 2: Node in a Blueprint graph (node_id + blueprint_name)
	if (Args->TryGetStringField(TEXT("node_id"), NodeId))
	{
		FString BPName;
		if (!Args->TryGetStringField(TEXT("blueprint_name"), BPName))
		{
			OutError = TEXT("'node_id' requires 'blueprint_name'");
			return nullptr;
		}

		UBlueprint* BP = FindBlueprintByName(BPName);
		if (!BP)
		{
			OutError = FString::Printf(TEXT("Blueprint '%s' not found"), *BPName);
			return nullptr;
		}

		FString GraphName;
		if (!Args->TryGetStringField(TEXT("graph_name"), GraphName))
			GraphName = TEXT("EventGraph");

		UEdGraph* Graph = FindGraphInBlueprint(BP, GraphName);
		if (!Graph)
		{
			OutError = FString::Printf(TEXT("Graph '%s' not found in Blueprint '%s'"), *GraphName, *BPName);
			return nullptr;
		}

		UEdGraphNode* Node = FindNodeByGuid(Graph, NodeId);
		if (!Node)
		{
			OutError = FString::Printf(TEXT("Node '%s' not found in graph '%s'"), *NodeId, *GraphName);
			return nullptr;
		}
		return Node;
	}

	// Path 3: Asset by name
	if (Args->TryGetStringField(TEXT("asset_name"), AssetName))
	{
		UObject* Asset = FindAssetByName(AssetName);
		if (!Asset) OutError = FString::Printf(TEXT("Asset '%s' not found"), *AssetName);
		return Asset;
	}

	OutError = TEXT("Provide 'object_path', 'asset_name', or 'node_id'+'blueprint_name' to identify the target object");
	return nullptr;
}

UEdGraph* FGenericPropertyHandler::FindGraphInBlueprint(UBlueprint* Blueprint, const FString& GraphName) const
{
	if (!Blueprint) return nullptr;

	// Search UbergraphPages (EventGraph, etc.)
	for (UEdGraph* Graph : Blueprint->UbergraphPages)
	{
		if (Graph && Graph->GetName().Equals(GraphName, ESearchCase::IgnoreCase))
			return Graph;
	}

	// Search FunctionGraphs
	for (UEdGraph* Graph : Blueprint->FunctionGraphs)
	{
		if (Graph && Graph->GetName().Equals(GraphName, ESearchCase::IgnoreCase))
			return Graph;
	}

	// Default: return first UbergraphPage if asking for EventGraph
	if (GraphName.Equals(TEXT("EventGraph"), ESearchCase::IgnoreCase) && Blueprint->UbergraphPages.Num() > 0)
		return Blueprint->UbergraphPages[0];

	return nullptr;
}

UEdGraphNode* FGenericPropertyHandler::FindNodeByGuid(UEdGraph* Graph, const FString& GuidString) const
{
	if (!Graph) return nullptr;

	FGuid SearchGuid;
	FGuid::Parse(GuidString, SearchGuid);

	for (UEdGraphNode* Node : Graph->Nodes)
	{
		if (Node && Node->NodeGuid == SearchGuid)
			return Node;
	}

	return nullptr;
}

// ============================================
// FindComponentOnCDO
// ============================================

UActorComponent* FGenericPropertyHandler::FindComponentOnCDO(UBlueprint* Blueprint, const FString& ComponentName) const
{
	if (!Blueprint) return nullptr;

	// --- Phase 1: Search CDO instantiated components (inherited, e.g. CharMoveComp, CapsuleComponent) ---
	if (Blueprint->GeneratedClass)
	{
		UObject* CDO = Blueprint->GeneratedClass->GetDefaultObject();
		AActor* ActorCDO = Cast<AActor>(CDO);
		if (ActorCDO)
		{
			TArray<UActorComponent*> Components;
			ActorCDO->GetComponents(Components);

			// Exact name match
			for (UActorComponent* Comp : Components)
			{
				if (!Comp) continue;
				if (Comp->GetName().Equals(ComponentName, ESearchCase::IgnoreCase))
					return Comp;
			}
			// Class name match
			for (UActorComponent* Comp : Components)
			{
				if (!Comp) continue;
				if (Comp->GetClass()->GetName().Equals(ComponentName, ESearchCase::IgnoreCase))
					return Comp;
			}
			// Partial match
			for (UActorComponent* Comp : Components)
			{
				if (!Comp) continue;
				if (Comp->GetName().Contains(ComponentName) || Comp->GetClass()->GetName().Contains(ComponentName))
					return Comp;
			}
		}
	}

	// --- Phase 2: Search SCS template components (Blueprint-added via add_component_to_blueprint) ---
	if (Blueprint->SimpleConstructionScript)
	{
		for (USCS_Node* Node : Blueprint->SimpleConstructionScript->GetAllNodes())
		{
			if (!Node || !Node->ComponentTemplate) continue;

			FString VarName = Node->GetVariableName().ToString();
			FString ClassName = Node->ComponentTemplate->GetClass()->GetName();

			// Exact match on variable name or component template name
			if (VarName.Equals(ComponentName, ESearchCase::IgnoreCase) ||
				Node->ComponentTemplate->GetName().Equals(ComponentName, ESearchCase::IgnoreCase))
			{
				return Node->ComponentTemplate;
			}
			// Class name match
			if (ClassName.Equals(ComponentName, ESearchCase::IgnoreCase))
			{
				return Node->ComponentTemplate;
			}
		}

		// Partial match fallback for SCS
		for (USCS_Node* Node : Blueprint->SimpleConstructionScript->GetAllNodes())
		{
			if (!Node || !Node->ComponentTemplate) continue;

			FString VarName = Node->GetVariableName().ToString();
			FString ClassName = Node->ComponentTemplate->GetClass()->GetName();

			if (VarName.Contains(ComponentName) || ClassName.Contains(ComponentName))
			{
				return Node->ComponentTemplate;
			}
		}
	}

	return nullptr;
}

// ============================================
// call_function
// Invoke any BlueprintCallable UFunction on any UObject via reflection.
// Args: {
//   "asset_name": "MyDataTable"  (or "object_path": "/Game/...")
//   "function_name": "AddRow",
//   "args": { "param1": "value1", "param2": "value2" }
// }
// ============================================
FCommandResult FGenericPropertyHandler::HandleCallFunction(const TSharedPtr<FJsonObject>& Args)
{
	// Resolve target object via universal resolver
	FString ResolveError;
	UObject* TargetObject = ResolveTargetObject(Args, ResolveError);
	if (!TargetObject)
	{
		return FCommandResult::Fail(ResolveError);
	}

	FString FunctionName;
	if (!Args->TryGetStringField(TEXT("function_name"), FunctionName))
	{
		return FCommandResult::Fail(TEXT("Missing 'function_name'"));
	}

	// Find the UFunction
	UFunction* Func = TargetObject->FindFunction(FName(*FunctionName));
	if (!Func)
	{
		// Try case-insensitive
		for (TFieldIterator<UFunction> It(TargetObject->GetClass()); It; ++It)
		{
			if (It->GetName().Equals(FunctionName, ESearchCase::IgnoreCase))
			{
				Func = *It;
				break;
			}
		}
	}

	if (!Func)
	{
		// List available functions for helpful error
		TArray<FString> Available;
		for (TFieldIterator<UFunction> It(TargetObject->GetClass()); It; ++It)
		{
			if (It->HasAnyFunctionFlags(FUNC_BlueprintCallable | FUNC_Public))
			{
				Available.Add(It->GetName());
			}
		}
		Available.Sort();
		FString AvailStr = FString::Join(Available, TEXT(", "));
		return FCommandResult::Fail(FString::Printf(TEXT("Function '%s' not found on %s. Available: %s"), *FunctionName, *TargetObject->GetClass()->GetName(), *AvailStr));
	}

	// Allocate parameter buffer
	uint8* ParamBuffer = (uint8*)FMemory::Malloc(Func->ParmsSize);
	FMemory::Memzero(ParamBuffer, Func->ParmsSize);
	Func->InitializeStruct(ParamBuffer);

	// Fill input parameters from args
	const TSharedPtr<FJsonObject>* FuncArgs = nullptr;
	Args->TryGetObjectField(TEXT("args"), FuncArgs);

	if (FuncArgs)
	{
		for (TFieldIterator<FProperty> ParamIt(Func); ParamIt; ++ParamIt)
		{
			FProperty* Param = *ParamIt;
			if (!Param->HasAnyPropertyFlags(CPF_Parm) || Param->HasAnyPropertyFlags(CPF_ReturnParm | CPF_OutParm))
				continue;

			FString ParamValue;
			if ((*FuncArgs)->TryGetStringField(Param->GetName(), ParamValue))
			{
				void* ValuePtr = Param->ContainerPtrToValuePtr<void>(ParamBuffer);
				Param->ImportText_Direct(*ParamValue, ValuePtr, TargetObject, PPF_None);
			}
		}
	}

	// Call the function
	TargetObject->ProcessEvent(Func, ParamBuffer);

	// Collect output/return values
	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("object"), TargetObject->GetName());
	Data->SetStringField(TEXT("objectClass"), TargetObject->GetClass()->GetName());
	Data->SetStringField(TEXT("function"), Func->GetName());

	TSharedPtr<FJsonObject> OutputObj = MakeShared<FJsonObject>();
	for (TFieldIterator<FProperty> ParamIt(Func); ParamIt; ++ParamIt)
	{
		FProperty* Param = *ParamIt;
		if (Param->HasAnyPropertyFlags(CPF_ReturnParm | CPF_OutParm))
		{
			FString OutVal;
			void* ValuePtr = Param->ContainerPtrToValuePtr<void>(ParamBuffer);
			Param->ExportTextItem_Direct(OutVal, ValuePtr, nullptr, TargetObject, PPF_None);
			OutputObj->SetStringField(Param->GetName(), OutVal);
		}
	}
	Data->SetObjectField(TEXT("returnValues"), OutputObj);
	Data->SetStringField(TEXT("message"), FString::Printf(TEXT("Called %s::%s"), *TargetObject->GetClass()->GetName(), *Func->GetName()));

	// Cleanup
	Func->DestroyStruct(ParamBuffer);
	FMemory::Free(ParamBuffer);

	return FCommandResult::Success(Data);
}

// ============================================
// get_object
// Get any UObject by path, with optional property/sub-object dump.
// Args: {
//   "object_path": "/Game/DataTables/DT_Items.DT_Items"   (or)
//   "asset_name": "DT_Items",
//   "list_functions": false,   (optional — list callable functions)
//   "list_subobjects": false   (optional — list Outer chain sub-objects)
// }
// ============================================
FCommandResult FGenericPropertyHandler::HandleGetObject(const TSharedPtr<FJsonObject>& Args)
{
	FString ResolveError;
	UObject* Object = ResolveTargetObject(Args, ResolveError);
	if (!Object)
	{
		return FCommandResult::Fail(ResolveError);
	}

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("name"), Object->GetName());
	Data->SetStringField(TEXT("class"), Object->GetClass()->GetName());
	Data->SetStringField(TEXT("path"), Object->GetPathName());
	Data->SetStringField(TEXT("outerChain"), Object->GetFullName());

	// All editable properties
	TArray<TSharedPtr<FJsonValue>> PropsArray;
	for (TFieldIterator<FProperty> PropIt(Object->GetClass()); PropIt; ++PropIt)
	{
		FProperty* Prop = *PropIt;
		if (!Prop->HasAnyPropertyFlags(CPF_Edit)) continue;

		FString Value;
		void* ValuePtr = Prop->ContainerPtrToValuePtr<void>(Object);
		Prop->ExportTextItem_Direct(Value, ValuePtr, nullptr, Object, PPF_None);

		TSharedPtr<FJsonObject> PropObj = MakeShared<FJsonObject>();
		PropObj->SetStringField(TEXT("name"), Prop->GetName());
		PropObj->SetStringField(TEXT("type"), Prop->GetCPPType());
		PropObj->SetStringField(TEXT("value"), Value);

		// Mark array properties
		if (Prop->IsA<FArrayProperty>())
		{
			PropObj->SetBoolField(TEXT("isArray"), true);
		}

		PropsArray.Add(MakeShared<FJsonValueObject>(PropObj));
	}
	Data->SetArrayField(TEXT("properties"), PropsArray);
	Data->SetNumberField(TEXT("propertyCount"), PropsArray.Num());

	// Optional: list callable functions
	bool bListFunctions = false;
	Args->TryGetBoolField(TEXT("list_functions"), bListFunctions);
	if (bListFunctions)
	{
		TArray<TSharedPtr<FJsonValue>> FuncArray;
		for (TFieldIterator<UFunction> FuncIt(Object->GetClass()); FuncIt; ++FuncIt)
		{
			UFunction* Func = *FuncIt;
			if (!Func->HasAnyFunctionFlags(FUNC_BlueprintCallable | FUNC_Public)) continue;

			TSharedPtr<FJsonObject> FuncObj = MakeShared<FJsonObject>();
			FuncObj->SetStringField(TEXT("name"), Func->GetName());

			TArray<TSharedPtr<FJsonValue>> Params;
			FString ReturnType;
			for (TFieldIterator<FProperty> ParamIt(Func); ParamIt; ++ParamIt)
			{
				if (ParamIt->HasAnyPropertyFlags(CPF_ReturnParm))
				{
					ReturnType = ParamIt->GetCPPType();
				}
				else if (ParamIt->HasAnyPropertyFlags(CPF_Parm))
				{
					TSharedPtr<FJsonObject> PObj = MakeShared<FJsonObject>();
					PObj->SetStringField(TEXT("name"), ParamIt->GetName());
					PObj->SetStringField(TEXT("type"), ParamIt->GetCPPType());
					PObj->SetBoolField(TEXT("isOutput"), ParamIt->HasAnyPropertyFlags(CPF_OutParm));
					Params.Add(MakeShared<FJsonValueObject>(PObj));
				}
			}
			FuncObj->SetArrayField(TEXT("params"), Params);
			if (!ReturnType.IsEmpty()) FuncObj->SetStringField(TEXT("returnType"), ReturnType);
			FuncArray.Add(MakeShared<FJsonValueObject>(FuncObj));
		}
		Data->SetArrayField(TEXT("functions"), FuncArray);
		Data->SetNumberField(TEXT("functionCount"), FuncArray.Num());
	}

	return FCommandResult::Success(Data);
}

// ============================================
// modify_array_property
// Add/remove/get items in TArray properties on any UObject.
// Args: {
//   "asset_name": "DT_Items"  (or "object_path")
//   "property_name": "RowMap"  (or deeper: "SomeArray")
//   "action": "add" | "remove" | "get" | "clear" | "count"
//   "value": "..."            (for add — the text representation of the element)
//   "index": 0                (for remove — which index to remove)
// }
// ============================================
FCommandResult FGenericPropertyHandler::HandleModifyArrayProperty(const TSharedPtr<FJsonObject>& Args)
{
	// Resolve target object via universal resolver
	FString ResolveError;
	UObject* Object = ResolveTargetObject(Args, ResolveError);
	if (!Object) return FCommandResult::Fail(ResolveError);

	FString PropName, Action;
	if (!Args->TryGetStringField(TEXT("property_name"), PropName))
		return FCommandResult::Fail(TEXT("Missing 'property_name'"));
	if (!Args->TryGetStringField(TEXT("action"), Action))
		return FCommandResult::Fail(TEXT("Missing 'action' (add/remove/get/clear/count)"));

	// Find the array property
	FArrayProperty* ArrayProp = nullptr;
	for (TFieldIterator<FProperty> It(Object->GetClass()); It; ++It)
	{
		if (It->GetName().Equals(PropName, ESearchCase::IgnoreCase))
		{
			ArrayProp = CastField<FArrayProperty>(*It);
			break;
		}
	}

	if (!ArrayProp)
	{
		return FCommandResult::Fail(FString::Printf(TEXT("Property '%s' not found or is not an array on %s"), *PropName, *Object->GetClass()->GetName()));
	}

	FScriptArrayHelper ArrayHelper(ArrayProp, ArrayProp->ContainerPtrToValuePtr<void>(Object));
	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("object"), Object->GetName());
	Data->SetStringField(TEXT("property"), PropName);
	Data->SetStringField(TEXT("action"), Action);

	if (Action.Equals(TEXT("count"), ESearchCase::IgnoreCase))
	{
		Data->SetNumberField(TEXT("count"), ArrayHelper.Num());
	}
	else if (Action.Equals(TEXT("get"), ESearchCase::IgnoreCase))
	{
		TArray<TSharedPtr<FJsonValue>> Items;
		FProperty* InnerProp = ArrayProp->Inner;
		for (int32 i = 0; i < ArrayHelper.Num(); i++)
		{
			FString Value;
			InnerProp->ExportTextItem_Direct(Value, ArrayHelper.GetRawPtr(i), nullptr, Object, PPF_None);
			Items.Add(MakeShared<FJsonValueString>(Value));
		}
		Data->SetArrayField(TEXT("items"), Items);
		Data->SetNumberField(TEXT("count"), Items.Num());
	}
	else if (Action.Equals(TEXT("add"), ESearchCase::IgnoreCase))
	{
		FString Value;
		if (!Args->TryGetStringField(TEXT("value"), Value))
			return FCommandResult::Fail(TEXT("Missing 'value' for add action"));

		int32 NewIndex = ArrayHelper.AddValue();
		FProperty* InnerProp = ArrayProp->Inner;
		if (!InnerProp->ImportText_Direct(*Value, ArrayHelper.GetRawPtr(NewIndex), Object, PPF_None))
		{
			ArrayHelper.RemoveValues(NewIndex, 1);
			return FCommandResult::Fail(FString::Printf(TEXT("Failed to parse value '%s' for array element type %s"), *Value, *InnerProp->GetCPPType()));
		}

		Object->MarkPackageDirty();
		Data->SetNumberField(TEXT("newIndex"), NewIndex);
		Data->SetNumberField(TEXT("newCount"), ArrayHelper.Num());
		Data->SetStringField(TEXT("message"), FString::Printf(TEXT("Added item at index %d"), NewIndex));
	}
	else if (Action.Equals(TEXT("remove"), ESearchCase::IgnoreCase))
	{
		int32 Index = 0;
		if (!Args->TryGetNumberField(TEXT("index"), Index))
			return FCommandResult::Fail(TEXT("Missing 'index' for remove action"));

		if (Index < 0 || Index >= ArrayHelper.Num())
			return FCommandResult::Fail(FString::Printf(TEXT("Index %d out of range [0, %d)"), Index, ArrayHelper.Num()));

		ArrayHelper.RemoveValues(Index, 1);
		Object->MarkPackageDirty();
		Data->SetNumberField(TEXT("newCount"), ArrayHelper.Num());
		Data->SetStringField(TEXT("message"), FString::Printf(TEXT("Removed item at index %d"), Index));
	}
	else if (Action.Equals(TEXT("clear"), ESearchCase::IgnoreCase))
	{
		int32 OldCount = ArrayHelper.Num();
		ArrayHelper.EmptyValues();
		Object->MarkPackageDirty();
		Data->SetNumberField(TEXT("removedCount"), OldCount);
		Data->SetStringField(TEXT("message"), FString::Printf(TEXT("Cleared %d items"), OldCount));
	}
	else
	{
		return FCommandResult::Fail(FString::Printf(TEXT("Unknown action '%s'. Use: add, remove, get, clear, count"), *Action));
	}

	return FCommandResult::Success(Data);
}

// ============================================
// execute_python
// Execute an editor Python script string. Ultimate escape hatch for any UE5 operation.
// Args: {
//   "script": "unreal.EditorAssetLibrary.rename_asset('/Game/Old', '/Game/New')"
// }
// ============================================

/** Custom FOutputDevice that captures all Serialize() calls into a TArray<FString> */
class FLogCapture : public FOutputDevice
{
public:
	TArray<FString> Lines;

	virtual void Serialize(const TCHAR* V, ELogVerbosity::Type Verbosity, const FName& Category) override
	{
		// Capture everything (LogPython, LogBlueprintCompiler, etc.)
		Lines.Add(FString::Printf(TEXT("[%s] %s"), *Category.ToString(), V));
	}
};

FCommandResult FGenericPropertyHandler::HandleExecutePython(const TSharedPtr<FJsonObject>& Args)
{
	FString Script;
	if (!Args->TryGetStringField(TEXT("script"), Script))
	{
		return FCommandResult::Fail(TEXT("Missing 'script' argument"));
	}

	if (!GEditor)
	{
		return FCommandResult::Fail(TEXT("GEditor not available"));
	}

	UWorld* World = GEditor->GetEditorWorldContext().World();

	// Wrap the user script with stdout/stderr capture to ensure print() output is always captured.
	// Python's print() in UE5 goes through sys.stdout which writes to LogPython,
	// but sometimes the log capture misses it due to buffering or thread timing.
	// This wrapper explicitly captures stdout and writes it via unreal.log_warning()
	// which is more reliably captured by our FLogCapture.
	FString WrappedScript = FString::Printf(
		TEXT("import sys as _sys, io as _io\n")
		TEXT("_old_stdout, _old_stderr = _sys.stdout, _sys.stderr\n")
		TEXT("_sys.stdout = _cap_out = _io.StringIO()\n")
		TEXT("_sys.stderr = _cap_err = _io.StringIO()\n")
		TEXT("_exc_info = None\n")
		TEXT("try:\n")
		TEXT("    exec(compile(%s, '<ai_script>', 'exec'))\n")
		TEXT("except Exception as _e:\n")
		TEXT("    _exc_info = _e\n")
		TEXT("finally:\n")
		TEXT("    _sys.stdout = _old_stdout\n")
		TEXT("    _sys.stderr = _old_stderr\n")
		TEXT("    _out_text = _cap_out.getvalue()\n")
		TEXT("    _err_text = _cap_err.getvalue()\n")
		TEXT("    if _out_text:\n")
		TEXT("        for _line in _out_text.rstrip().split('\\n'):\n")
		TEXT("            import unreal; unreal.log_warning('__PYOUT__' + _line)\n")
		TEXT("    if _err_text:\n")
		TEXT("        for _line in _err_text.rstrip().split('\\n'):\n")
		TEXT("            import unreal; unreal.log_error('__PYERR__' + _line)\n")
		TEXT("    if _exc_info:\n")
		TEXT("        import traceback, unreal\n")
		TEXT("        _tb = ''.join(traceback.format_exception(type(_exc_info), _exc_info, _exc_info.__traceback__))\n")
		TEXT("        for _line in _tb.rstrip().split('\\n'):\n")
		TEXT("            unreal.log_error('__PYERR__' + _line)\n"),
		*QuoteForPython(Script));

	FString FullCmd = FString::Printf(TEXT("py %s"), *WrappedScript);

	// Capture log output using custom FOutputDevice
	FLogCapture LogCapture;
	GLog->AddOutputDevice(&LogCapture);

	bool bExec = GEditor->Exec(World, *FullCmd, *GLog);

	GLog->RemoveOutputDevice(&LogCapture);

	// Build captured output string
	FString CapturedOutput = FString::Join(LogCapture.Lines, TEXT("\n"));

	// Extract Python output — look for both standard LogPython lines and our __PYOUT__/__PYERR__ markers
	TArray<FString> PythonOutput;
	TArray<FString> PythonErrors;
	for (const FString& Line : LogCapture.Lines)
	{
		// Check for our explicit markers first (most reliable)
		int32 OutIdx = Line.Find(TEXT("__PYOUT__"));
		if (OutIdx != INDEX_NONE)
		{
			PythonOutput.Add(Line.Mid(OutIdx + 9));
			continue;
		}
		int32 ErrIdx = Line.Find(TEXT("__PYERR__"));
		if (ErrIdx != INDEX_NONE)
		{
			PythonErrors.Add(Line.Mid(ErrIdx + 9));
			continue;
		}
		// Fallback: standard LogPython lines (for direct print() that was captured)
		if (Line.StartsWith(TEXT("[LogPython]")))
		{
			FString Clean = Line.Mid(12).TrimStartAndEnd();
			// Skip lines we already captured via markers
			if (!Clean.IsEmpty() && !Clean.StartsWith(TEXT("__PYOUT__")) && !Clean.StartsWith(TEXT("__PYERR__")))
				PythonOutput.Add(Clean);
		}
	}

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("script"), Script);
	Data->SetBoolField(TEXT("executed"), bExec);

	// Python-specific output (print() results)
	if (PythonOutput.Num() > 0)
	{
		TArray<TSharedPtr<FJsonValue>> OutputArr;
		for (const FString& S : PythonOutput)
			OutputArr.Add(MakeShared<FJsonValueString>(S));
		Data->SetArrayField(TEXT("output"), OutputArr);
		Data->SetStringField(TEXT("outputText"), FString::Join(PythonOutput, TEXT("\n")));
	}

	// Python errors (stderr + exceptions)
	if (PythonErrors.Num() > 0)
	{
		TArray<TSharedPtr<FJsonValue>> ErrArr;
		for (const FString& S : PythonErrors)
			ErrArr.Add(MakeShared<FJsonValueString>(S));
		Data->SetArrayField(TEXT("errors"), ErrArr);
		Data->SetStringField(TEXT("errorText"), FString::Join(PythonErrors, TEXT("\n")));
	}

	// Full log (all categories) — useful for debugging
	if (LogCapture.Lines.Num() > 0)
	{
		Data->SetStringField(TEXT("fullLog"), CapturedOutput);
		Data->SetNumberField(TEXT("logLineCount"), LogCapture.Lines.Num());
	}

	Data->SetStringField(TEXT("message"), bExec
		? FString::Printf(TEXT("Python script executed. %d log lines captured."), LogCapture.Lines.Num())
		: TEXT("Script execution returned false — check 'fullLog' for errors"));
	return FCommandResult::Success(Data);
}

/** Helper: escape a Python script string for embedding in compile() call */
FString FGenericPropertyHandler::QuoteForPython(const FString& Script) const
{
	// Triple-quote the script to handle all internal quotes and newlines safely
	// Escape any triple-quote sequences inside the script
	FString Escaped = Script.Replace(TEXT("\\"), TEXT("\\\\"))
	                        .Replace(TEXT("\"\"\""), TEXT("\\\"\\\"\\\""));
	return FString::Printf(TEXT("\"\"\"%s\"\"\""), *Escaped);
}
