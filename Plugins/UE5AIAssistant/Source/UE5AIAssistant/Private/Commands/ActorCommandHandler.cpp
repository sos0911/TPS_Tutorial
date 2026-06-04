// Copyright AI Assistant. All Rights Reserved.

#include "Commands/ActorCommandHandler.h"
#include "Editor.h"
#include "Engine/World.h"
#include "Engine/StaticMeshActor.h"
#include "EngineUtils.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/PointLight.h"
#include "Engine/SpotLight.h"
#include "Engine/DirectionalLight.h"
#include "Camera/CameraActor.h"
#include "Selection.h"
#include "Engine/StaticMesh.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "UObject/UObjectIterator.h"

TArray<FString> FActorCommandHandler::GetSupportedCommands() const
{
	return {
		TEXT("get_actors_in_level"),
		TEXT("get_selected_actors"),
		TEXT("find_actors_by_name"),
		TEXT("spawn_actor"),
		TEXT("delete_actor"),
		TEXT("set_actor_transform"),
		TEXT("get_actor_properties"),
		TEXT("set_actor_property"),
		TEXT("attach_actor"),
		TEXT("detach_actor"),
	};
}

FCommandResult FActorCommandHandler::Execute(const FString& Command, const TSharedPtr<FJsonObject>& Args)
{
	if (Command == TEXT("get_actors_in_level"))     return HandleGetActorsInLevel(Args);
	if (Command == TEXT("get_selected_actors"))     return HandleGetSelectedActors(Args);
	if (Command == TEXT("find_actors_by_name"))     return HandleFindActorsByName(Args);
	if (Command == TEXT("spawn_actor"))             return HandleSpawnActor(Args);
	if (Command == TEXT("delete_actor"))            return HandleDeleteActor(Args);
	if (Command == TEXT("set_actor_transform"))     return HandleSetActorTransform(Args);
	if (Command == TEXT("get_actor_properties"))    return HandleGetActorProperties(Args);
	if (Command == TEXT("set_actor_property"))      return HandleSetActorProperty(Args);
	if (Command == TEXT("attach_actor"))            return HandleAttachActor(Args);
	if (Command == TEXT("detach_actor"))            return HandleDetachActor(Args);

	return FCommandResult::Fail(FString::Printf(TEXT("ActorHandler: Unknown command '%s'"), *Command));
}

// ============================================
// get_actors_in_level
// Args: (none)
// ============================================
FCommandResult FActorCommandHandler::HandleGetActorsInLevel(const TSharedPtr<FJsonObject>& Args)
{
	UWorld* World = GetEditorWorld();
	if (!World)
	{
		return FCommandResult::Fail(TEXT("No editor world available"));
	}

	TArray<TSharedPtr<FJsonValue>> ActorArray;

	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (!Actor || Actor->IsHidden())
		{
			continue;
		}

		ActorArray.Add(MakeShared<FJsonValueObject>(ActorToJson(Actor)));
	}

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetArrayField(TEXT("actors"), ActorArray);
	Data->SetNumberField(TEXT("count"), ActorArray.Num());

	return FCommandResult::Success(Data);
}

// ============================================
// get_selected_actors
// Args: (none)
// ============================================
FCommandResult FActorCommandHandler::HandleGetSelectedActors(const TSharedPtr<FJsonObject>& Args)
{
	if (!GEditor)
	{
		return FCommandResult::Fail(TEXT("GEditor not available"));
	}

	USelection* Selection = GEditor->GetSelectedActors();
	if (!Selection)
	{
		return FCommandResult::Fail(TEXT("No selection available"));
	}

	TArray<TSharedPtr<FJsonValue>> ActorArray;

	for (int32 i = 0; i < Selection->Num(); i++)
	{
		AActor* Actor = Cast<AActor>(Selection->GetSelectedObject(i));
		if (Actor)
		{
			ActorArray.Add(MakeShared<FJsonValueObject>(ActorToJson(Actor)));
		}
	}

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetArrayField(TEXT("actors"), ActorArray);
	Data->SetNumberField(TEXT("count"), ActorArray.Num());

	return FCommandResult::Success(Data);
}

// ============================================
// find_actors_by_name
// Args: { "pattern": "SM_*" }
// ============================================
FCommandResult FActorCommandHandler::HandleFindActorsByName(const TSharedPtr<FJsonObject>& Args)
{
	FString Pattern;
	if (!Args->TryGetStringField(TEXT("pattern"), Pattern))
	{
		return FCommandResult::Fail(TEXT("Missing required 'pattern' argument"));
	}

	UWorld* World = GetEditorWorld();
	if (!World)
	{
		return FCommandResult::Fail(TEXT("No editor world available"));
	}

	// Convert simple wildcard to search
	FString SearchTerm = Pattern.Replace(TEXT("*"), TEXT(""));

	TArray<TSharedPtr<FJsonValue>> ActorArray;

	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (!Actor) continue;

		FString ActorLabel = Actor->GetActorLabel();
		FString ActorName = Actor->GetName();

		if (ActorLabel.Contains(SearchTerm, ESearchCase::IgnoreCase) ||
			ActorName.Contains(SearchTerm, ESearchCase::IgnoreCase))
		{
			ActorArray.Add(MakeShared<FJsonValueObject>(ActorToJson(Actor)));
		}
	}

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetArrayField(TEXT("actors"), ActorArray);
	Data->SetNumberField(TEXT("count"), ActorArray.Num());

	return FCommandResult::Success(Data);
}

// ============================================
// spawn_actor
// Args: { "name": "MyCube", "type": "CUBE", "location": [x,y,z], "rotation": [p,y,r] }
// ============================================
FCommandResult FActorCommandHandler::HandleSpawnActor(const TSharedPtr<FJsonObject>& Args)
{
	FString Name;
	if (!Args->TryGetStringField(TEXT("name"), Name))
	{
		return FCommandResult::Fail(TEXT("Missing required 'name' argument"));
	}

	FString Type;
	if (!Args->TryGetStringField(TEXT("type"), Type))
	{
		return FCommandResult::Fail(TEXT("Missing required 'type' argument"));
	}

	Type = Type.ToUpper();

	FVector Location = FVector::ZeroVector;
	JsonArrayToVector(Args, TEXT("location"), Location);

	FVector RotationVec = FVector::ZeroVector;
	JsonArrayToVector(Args, TEXT("rotation"), RotationVec);
	FRotator Rotation(RotationVec.X, RotationVec.Y, RotationVec.Z);

	UWorld* World = GetEditorWorld();
	if (!World)
	{
		return FCommandResult::Fail(TEXT("No editor world available"));
	}

	AActor* NewActor = nullptr;

	// --- Try BasicShapes first (by name match against /Engine/BasicShapes/) ---
	FString BasicShapePath = FString::Printf(TEXT("/Engine/BasicShapes/%s.%s"), *Type, *Type);
	// Capitalize first letter for matching: "cube" -> "Cube"
	FString ShapeName = Type;
	ShapeName[0] = FChar::ToUpper(ShapeName[0]);
	for (int32 i = 1; i < ShapeName.Len(); i++) ShapeName[i] = FChar::ToLower(ShapeName[i]);
	FString ShapePath = FString::Printf(TEXT("/Engine/BasicShapes/%s.%s"), *ShapeName, *ShapeName);

	UStaticMesh* BasicMesh = LoadObject<UStaticMesh>(nullptr, *ShapePath);
	if (BasicMesh)
	{
		AStaticMeshActor* MeshActor = World->SpawnActor<AStaticMeshActor>(
			AStaticMeshActor::StaticClass(), Location, Rotation);
		if (MeshActor)
		{
			if (MeshActor->GetStaticMeshComponent())
			{
				MeshActor->GetStaticMeshComponent()->SetStaticMesh(BasicMesh);
			}
			MeshActor->SetActorLabel(Name);
			MeshActor->SetFolderPath(TEXT("AI_Generated"));
			NewActor = MeshActor;
		}
	}

	// --- Try dynamic Actor class lookup via TObjectIterator ---
	if (!NewActor)
	{
		UClass* ActorClass = nullptr;

		// Search all AActor subclasses by name match (case-insensitive)
		for (TObjectIterator<UClass> It; It; ++It)
		{
			if (!It->IsChildOf(AActor::StaticClass())) continue;
			if (It->HasAnyClassFlags(CLASS_Abstract)) continue;

			FString ClassName = It->GetName();
			// Strip common prefixes for matching: APointLight -> PointLight
			FString ShortName = ClassName;
			if (ShortName.StartsWith(TEXT("A"))) ShortName.RemoveAt(0);

			if (ClassName.Equals(Type, ESearchCase::IgnoreCase) ||
				ShortName.Equals(Type, ESearchCase::IgnoreCase))
			{
				ActorClass = *It;
				break;
			}
		}

		if (ActorClass)
		{
			NewActor = World->SpawnActor(ActorClass, &Location, &Rotation);
		}
	}

	// --- Try StaticMesh from AssetRegistry (e.g. user passes a mesh name) ---
	if (!NewActor)
	{
		FAssetRegistryModule& ARModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
		TArray<FAssetData> MeshAssets;
		ARModule.Get().GetAssetsByClass(UStaticMesh::StaticClass()->GetClassPathName(), MeshAssets, true);

		UStaticMesh* FoundMesh = nullptr;
		for (const FAssetData& Asset : MeshAssets)
		{
			if (Asset.AssetName.ToString().Equals(Type, ESearchCase::IgnoreCase))
			{
				FoundMesh = Cast<UStaticMesh>(Asset.GetAsset());
				break;
			}
		}

		if (FoundMesh)
		{
			AStaticMeshActor* MeshActor = World->SpawnActor<AStaticMeshActor>(
				AStaticMeshActor::StaticClass(), Location, Rotation);
			if (MeshActor && MeshActor->GetStaticMeshComponent())
			{
				MeshActor->GetStaticMeshComponent()->SetStaticMesh(FoundMesh);
				MeshActor->SetActorLabel(Name);
				MeshActor->SetFolderPath(TEXT("AI_Generated"));
				NewActor = MeshActor;
			}
		}
	}

	if (!NewActor)
	{
		return FCommandResult::Fail(FString::Printf(
			TEXT("Actor type '%s' not found. Accepts: BasicShape name (Cube/Sphere/Cylinder/Cone), any AActor subclass name (PointLight/SpotLight/CameraActor), or a StaticMesh asset name."),
			*Type));
	}

	if (!NewActor)
	{
		return FCommandResult::Fail(TEXT("Failed to spawn actor"));
	}

	if (NewActor->GetActorLabel().IsEmpty())
	{
		NewActor->SetActorLabel(Name);
	}

	TSharedPtr<FJsonObject> Data = ActorToJson(NewActor);
	Data->SetStringField(TEXT("message"),
		FString::Printf(TEXT("Successfully spawned %s actor '%s'"), *Type, *Name));

	return FCommandResult::Success(Data);
}

// ============================================
// delete_actor
// Args: { "name": "MyCube" }
// ============================================
FCommandResult FActorCommandHandler::HandleDeleteActor(const TSharedPtr<FJsonObject>& Args)
{
	FString Name;
	if (!Args->TryGetStringField(TEXT("name"), Name))
	{
		return FCommandResult::Fail(TEXT("Missing required 'name' argument"));
	}

	AActor* Actor = FindActorByName(Name);
	if (!Actor)
	{
		return FCommandResult::Fail(FString::Printf(TEXT("Actor '%s' not found"), *Name));
	}

	FString ActorLabel = Actor->GetActorLabel();
	FString ActorClass = Actor->GetClass()->GetName();

	if (!GEditor || !GEditor->GetEditorWorldContext().World())
	{
		return FCommandResult::Fail(TEXT("No editor world available"));
	}

	bool bDestroyed = GEditor->GetEditorWorldContext().World()->DestroyActor(Actor);
	if (!bDestroyed)
	{
		return FCommandResult::Fail(FString::Printf(TEXT("Failed to destroy actor '%s'"), *Name));
	}

	return FCommandResult::SuccessMessage(
		FString::Printf(TEXT("Successfully deleted actor '%s' (%s)"), *ActorLabel, *ActorClass));
}

// ============================================
// set_actor_transform
// Args: { "name": "MyCube", "location": [x,y,z], "rotation": [p,y,r], "scale": [x,y,z] }
// ============================================
FCommandResult FActorCommandHandler::HandleSetActorTransform(const TSharedPtr<FJsonObject>& Args)
{
	FString Name;
	if (!Args->TryGetStringField(TEXT("name"), Name))
	{
		return FCommandResult::Fail(TEXT("Missing required 'name' argument"));
	}

	AActor* Actor = FindActorByName(Name);
	if (!Actor)
	{
		return FCommandResult::Fail(FString::Printf(TEXT("Actor '%s' not found"), *Name));
	}

	FVector Location;
	if (JsonArrayToVector(Args, TEXT("location"), Location))
	{
		Actor->SetActorLocation(Location);
	}

	FVector RotationVec;
	if (JsonArrayToVector(Args, TEXT("rotation"), RotationVec))
	{
		Actor->SetActorRotation(FRotator(RotationVec.X, RotationVec.Y, RotationVec.Z));
	}

	FVector Scale;
	if (JsonArrayToVector(Args, TEXT("scale"), Scale))
	{
		Actor->SetActorScale3D(Scale);
	}

	TSharedPtr<FJsonObject> Data = ActorToJson(Actor);
	Data->SetStringField(TEXT("message"),
		FString::Printf(TEXT("Successfully updated transform for '%s'"), *Name));

	return FCommandResult::Success(Data);
}

// ============================================
// get_actor_properties
// Args: { "name": "MyCube" }
// ============================================
FCommandResult FActorCommandHandler::HandleGetActorProperties(const TSharedPtr<FJsonObject>& Args)
{
	FString Name;
	if (!Args->TryGetStringField(TEXT("name"), Name))
	{
		return FCommandResult::Fail(TEXT("Missing required 'name' argument"));
	}

	AActor* Actor = FindActorByName(Name);
	if (!Actor)
	{
		return FCommandResult::Fail(FString::Printf(TEXT("Actor '%s' not found"), *Name));
	}

	TSharedPtr<FJsonObject> Data = ActorToJson(Actor);

	// Additional properties
	Data->SetBoolField(TEXT("isHidden"), Actor->IsHidden());
	Data->SetBoolField(TEXT("isEditable"), !Actor->IsLockLocation());

	// Mobility
	USceneComponent* RootComp = Actor->GetRootComponent();
	if (RootComp)
	{
		FString Mobility;
		switch (RootComp->Mobility)
		{
		case EComponentMobility::Static:    Mobility = TEXT("Static"); break;
		case EComponentMobility::Stationary: Mobility = TEXT("Stationary"); break;
		case EComponentMobility::Movable:   Mobility = TEXT("Movable"); break;
		}
		Data->SetStringField(TEXT("mobility"), Mobility);
	}

	// Tags
	TArray<TSharedPtr<FJsonValue>> TagsArray;
	for (const FName& Tag : Actor->Tags)
	{
		TagsArray.Add(MakeShared<FJsonValueString>(Tag.ToString()));
	}
	Data->SetArrayField(TEXT("tags"), TagsArray);

	// Static mesh info
	AStaticMeshActor* MeshActor = Cast<AStaticMeshActor>(Actor);
	if (MeshActor && MeshActor->GetStaticMeshComponent())
	{
		UStaticMesh* Mesh = MeshActor->GetStaticMeshComponent()->GetStaticMesh();
		if (Mesh)
		{
			Data->SetStringField(TEXT("staticMesh"), Mesh->GetPathName());
		}

		UMaterialInterface* Material = MeshActor->GetStaticMeshComponent()->GetMaterial(0);
		if (Material)
		{
			Data->SetStringField(TEXT("material"), Material->GetPathName());
		}
	}

	// Components list
	TArray<TSharedPtr<FJsonValue>> ComponentsArray;
	for (UActorComponent* Comp : Actor->GetComponents())
	{
		if (Comp)
		{
			TSharedPtr<FJsonObject> CompObj = MakeShared<FJsonObject>();
			CompObj->SetStringField(TEXT("name"), Comp->GetName());
			CompObj->SetStringField(TEXT("type"), Comp->GetClass()->GetName());
			ComponentsArray.Add(MakeShared<FJsonValueObject>(CompObj));
		}
	}
	Data->SetArrayField(TEXT("components"), ComponentsArray);

	return FCommandResult::Success(Data);
}

// ============================================
// set_actor_property
// Args: { "name": "MyCube", "property_name": "Mobility", "property_value": "Movable" }
// ============================================
FCommandResult FActorCommandHandler::HandleSetActorProperty(const TSharedPtr<FJsonObject>& Args)
{
	FString Name;
	if (!Args->TryGetStringField(TEXT("name"), Name))
	{
		return FCommandResult::Fail(TEXT("Missing required 'name' argument"));
	}

	FString PropertyName;
	if (!Args->TryGetStringField(TEXT("property_name"), PropertyName))
	{
		return FCommandResult::Fail(TEXT("Missing required 'property_name' argument"));
	}

	FString PropertyValue;
	if (!Args->TryGetStringField(TEXT("property_value"), PropertyValue))
	{
		return FCommandResult::Fail(TEXT("Missing required 'property_value' argument"));
	}

	AActor* Actor = FindActorByName(Name);
	if (!Actor)
	{
		return FCommandResult::Fail(FString::Printf(TEXT("Actor '%s' not found"), *Name));
	}

	// Handle some common properties specially
	if (PropertyName.Equals(TEXT("Mobility"), ESearchCase::IgnoreCase))
	{
		USceneComponent* RootComp = Actor->GetRootComponent();
		if (RootComp)
		{
			// Dynamic enum resolution via UEnum reflection
			UEnum* MobilityEnum = StaticEnum<EComponentMobility::Type>();
			if (MobilityEnum)
			{
				// Try name variants: "Movable", "EComponentMobility::Movable"
				int64 EnumValue = MobilityEnum->GetValueByNameString(PropertyValue);
				if (EnumValue == INDEX_NONE)
				{
					// Try with full prefix
					EnumValue = MobilityEnum->GetValueByNameString(
						FString::Printf(TEXT("EComponentMobility::%s"), *PropertyValue));
				}
				if (EnumValue != INDEX_NONE)
				{
					RootComp->SetMobility(static_cast<EComponentMobility::Type>(EnumValue));
				}
				else
				{
					// List valid values dynamically
					TArray<FString> ValidNames;
					for (int32 i = 0; i < MobilityEnum->NumEnums() - 1; i++)
					{
						ValidNames.Add(MobilityEnum->GetNameStringByIndex(i));
					}
					return FCommandResult::Fail(FString::Printf(TEXT("Invalid Mobility '%s'. Valid: %s"),
						*PropertyValue, *FString::Join(ValidNames, TEXT(", "))));
				}
			}
		}
	}
	else if (PropertyName.Equals(TEXT("Hidden"), ESearchCase::IgnoreCase) ||
			 PropertyName.Equals(TEXT("isHidden"), ESearchCase::IgnoreCase))
	{
		bool bHidden = PropertyValue.Equals(TEXT("true"), ESearchCase::IgnoreCase);
		Actor->SetIsTemporarilyHiddenInEditor(bHidden);
	}
	else if (PropertyName.Equals(TEXT("Label"), ESearchCase::IgnoreCase) ||
			 PropertyName.Equals(TEXT("ActorLabel"), ESearchCase::IgnoreCase))
	{
		Actor->SetActorLabel(PropertyValue);
	}
	else if (PropertyName.Equals(TEXT("FolderPath"), ESearchCase::IgnoreCase))
	{
		Actor->SetFolderPath(FName(*PropertyValue));
	}
	else
	{
		// Try to set property via UObject reflection
		FProperty* Prop = Actor->GetClass()->FindPropertyByName(FName(*PropertyName));
		if (!Prop)
		{
			return FCommandResult::Fail(FString::Printf(
				TEXT("Property '%s' not found on actor '%s' (%s). Common properties: Mobility, Hidden, Label, FolderPath"),
				*PropertyName, *Name, *Actor->GetClass()->GetName()));
		}

		// For now, try string-based property import
		void* PropAddr = Prop->ContainerPtrToValuePtr<void>(Actor);
		if (!Prop->ImportText_Direct(*PropertyValue, PropAddr, Actor, PPF_None))
		{
			return FCommandResult::Fail(FString::Printf(
				TEXT("Failed to set property '%s' to '%s'"), *PropertyName, *PropertyValue));
		}
	}

	Actor->PostEditChange();
	Actor->MarkPackageDirty();

	return FCommandResult::SuccessMessage(
		FString::Printf(TEXT("Successfully set '%s'.'%s' = '%s'"), *Name, *PropertyName, *PropertyValue));
}

// ============================================
// Helpers
// ============================================
UWorld* FActorCommandHandler::GetEditorWorld() const
{
	if (GEditor)
	{
		return GEditor->GetEditorWorldContext().World();
	}
	return nullptr;
}

AActor* FActorCommandHandler::FindActorByName(const FString& Name) const
{
	UWorld* World = GetEditorWorld();
	if (!World) return nullptr;

	// First try matching by actor label (display name in editor)
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (Actor && Actor->GetActorLabel().Equals(Name, ESearchCase::IgnoreCase))
		{
			return Actor;
		}
	}

	// Then try matching by internal name
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (Actor && Actor->GetName().Equals(Name, ESearchCase::IgnoreCase))
		{
			return Actor;
		}
	}

	return nullptr;
}

TSharedPtr<FJsonObject> FActorCommandHandler::ActorToJson(AActor* Actor) const
{
	TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
	if (!Actor) return Obj;

	Obj->SetStringField(TEXT("name"), Actor->GetActorLabel().IsEmpty() ? Actor->GetName() : Actor->GetActorLabel());
	Obj->SetStringField(TEXT("internalName"), Actor->GetName());
	Obj->SetStringField(TEXT("type"), Actor->GetClass()->GetName());

	FVector Loc = Actor->GetActorLocation();
	TArray<TSharedPtr<FJsonValue>> LocArray;
	LocArray.Add(MakeShared<FJsonValueNumber>(Loc.X));
	LocArray.Add(MakeShared<FJsonValueNumber>(Loc.Y));
	LocArray.Add(MakeShared<FJsonValueNumber>(Loc.Z));
	Obj->SetArrayField(TEXT("location"), LocArray);

	FRotator Rot = Actor->GetActorRotation();
	TArray<TSharedPtr<FJsonValue>> RotArray;
	RotArray.Add(MakeShared<FJsonValueNumber>(Rot.Pitch));
	RotArray.Add(MakeShared<FJsonValueNumber>(Rot.Yaw));
	RotArray.Add(MakeShared<FJsonValueNumber>(Rot.Roll));
	Obj->SetArrayField(TEXT("rotation"), RotArray);

	FVector Scale = Actor->GetActorScale3D();
	TArray<TSharedPtr<FJsonValue>> ScaleArray;
	ScaleArray.Add(MakeShared<FJsonValueNumber>(Scale.X));
	ScaleArray.Add(MakeShared<FJsonValueNumber>(Scale.Y));
	ScaleArray.Add(MakeShared<FJsonValueNumber>(Scale.Z));
	Obj->SetArrayField(TEXT("scale"), ScaleArray);

	return Obj;
}

bool FActorCommandHandler::JsonArrayToVector(const TSharedPtr<FJsonObject>& Args, const FString& FieldName, FVector& OutVector) const
{
	const TArray<TSharedPtr<FJsonValue>>* ArrayPtr;
	if (!Args->TryGetArrayField(FieldName, ArrayPtr) || ArrayPtr->Num() < 3)
	{
		return false;
	}

	OutVector.X = (*ArrayPtr)[0]->AsNumber();
	OutVector.Y = (*ArrayPtr)[1]->AsNumber();
	OutVector.Z = (*ArrayPtr)[2]->AsNumber();
	return true;
}

// ============================================
// attach_actor
// Args: { "child": "ChildActorName", "parent": "ParentActorName",
//         "socket_name": "" (optional), "rule": "KeepRelative" (optional) }
// ============================================
FCommandResult FActorCommandHandler::HandleAttachActor(const TSharedPtr<FJsonObject>& Args)
{
	FString ChildName, ParentName;
	if (!Args->TryGetStringField(TEXT("child"), ChildName))
	{
		return FCommandResult::Fail(TEXT("Missing required 'child' argument"));
	}
	if (!Args->TryGetStringField(TEXT("parent"), ParentName))
	{
		return FCommandResult::Fail(TEXT("Missing required 'parent' argument"));
	}

	AActor* ChildActor = FindActorByName(ChildName);
	if (!ChildActor)
	{
		return FCommandResult::Fail(FString::Printf(TEXT("Child actor '%s' not found"), *ChildName));
	}

	AActor* ParentActor = FindActorByName(ParentName);
	if (!ParentActor)
	{
		return FCommandResult::Fail(FString::Printf(TEXT("Parent actor '%s' not found"), *ParentName));
	}

	// Optional socket name
	FString SocketStr;
	Args->TryGetStringField(TEXT("socket_name"), SocketStr);
	FName SocketName = SocketStr.IsEmpty() ? NAME_None : FName(*SocketStr);

	// Attachment rule: KeepRelative (default), KeepWorld, SnapToTarget
	FString RuleStr;
	Args->TryGetStringField(TEXT("rule"), RuleStr);
	EAttachmentRule Rule = EAttachmentRule::KeepRelative;
	if (RuleStr.Equals(TEXT("KeepWorld"), ESearchCase::IgnoreCase))
	{
		Rule = EAttachmentRule::KeepWorld;
	}
	else if (RuleStr.Equals(TEXT("SnapToTarget"), ESearchCase::IgnoreCase))
	{
		Rule = EAttachmentRule::SnapToTarget;
	}

	FAttachmentTransformRules AttachRules(Rule, /*bWeldSimulatedBodies=*/ false);
	ChildActor->AttachToActor(ParentActor, AttachRules, SocketName);

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("child"), ChildName);
	Data->SetStringField(TEXT("parent"), ParentName);
	Data->SetStringField(TEXT("rule"), RuleStr.IsEmpty() ? TEXT("KeepRelative") : RuleStr);
	if (!SocketStr.IsEmpty())
	{
		Data->SetStringField(TEXT("socket"), SocketStr);
	}
	Data->SetStringField(TEXT("message"), FString::Printf(TEXT("Attached '%s' to '%s'"), *ChildName, *ParentName));
	return FCommandResult::Success(Data);
}

// ============================================
// detach_actor
// Args: { "name": "ActorName", "rule": "KeepRelative" (optional) }
// ============================================
FCommandResult FActorCommandHandler::HandleDetachActor(const TSharedPtr<FJsonObject>& Args)
{
	FString Name;
	if (!Args->TryGetStringField(TEXT("name"), Name))
	{
		return FCommandResult::Fail(TEXT("Missing required 'name' argument"));
	}

	AActor* Actor = FindActorByName(Name);
	if (!Actor)
	{
		return FCommandResult::Fail(FString::Printf(TEXT("Actor '%s' not found"), *Name));
	}

	AActor* OldParent = Actor->GetAttachParentActor();
	if (!OldParent)
	{
		return FCommandResult::Fail(FString::Printf(TEXT("Actor '%s' is not attached to any parent"), *Name));
	}

	FString RuleStr;
	Args->TryGetStringField(TEXT("rule"), RuleStr);
	EDetachmentRule Rule = EDetachmentRule::KeepRelative;
	if (RuleStr.Equals(TEXT("KeepWorld"), ESearchCase::IgnoreCase))
	{
		Rule = EDetachmentRule::KeepWorld;
	}

	FDetachmentTransformRules DetachRules(Rule, /*bCallModify=*/ true);
	Actor->DetachFromActor(DetachRules);

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("name"), Name);
	Data->SetStringField(TEXT("previousParent"), OldParent->GetActorLabel());
	Data->SetStringField(TEXT("message"), FString::Printf(TEXT("Detached '%s' from '%s'"), *Name, *OldParent->GetActorLabel()));
	return FCommandResult::Success(Data);
}
