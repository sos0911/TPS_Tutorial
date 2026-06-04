// Copyright AI Assistant. All Rights Reserved.

#include "Commands/AssetCommandHandler.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/Blueprint.h"
#include "Engine/StaticMesh.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/Texture2D.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Sound/SoundWave.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimBlueprint.h"
#include "UObject/UObjectIterator.h"

TArray<FString> FAssetCommandHandler::GetSupportedCommands() const
{
	return {
		TEXT("search_assets"),
		TEXT("get_assets_by_class"),
		TEXT("get_asset_details"),
	};
}

FCommandResult FAssetCommandHandler::Execute(const FString& Command, const TSharedPtr<FJsonObject>& Args)
{
	if (Command == TEXT("search_assets"))       return HandleSearchAssets(Args);
	if (Command == TEXT("get_assets_by_class")) return HandleGetAssetsByClass(Args);
	if (Command == TEXT("get_asset_details"))   return HandleGetAssetDetails(Args);

	return FCommandResult::Fail(FString::Printf(TEXT("AssetHandler: Unknown command '%s'"), *Command));
}

// ============================================
// search_assets
// Args: { "query": "Chair", "path": "/Game/", "max_results": 50 }
// ============================================
FCommandResult FAssetCommandHandler::HandleSearchAssets(const TSharedPtr<FJsonObject>& Args)
{
	FString Query;
	if (!Args->TryGetStringField(TEXT("query"), Query))
	{
		return FCommandResult::Fail(TEXT("Missing required 'query' argument"));
	}

	FString SearchPath;
	if (!Args->TryGetStringField(TEXT("path"), SearchPath))
	{
		SearchPath = TEXT("/Game/");
	}

	int32 MaxResults = 50;
	Args->TryGetNumberField(TEXT("max_results"), MaxResults);

	FAssetRegistryModule& ARModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	IAssetRegistry& AR = ARModule.Get();

	FARFilter Filter;
	Filter.PackagePaths.Add(FName(*SearchPath));
	Filter.bRecursivePaths = true;

	TArray<FAssetData> AllAssets;
	AR.GetAssets(Filter, AllAssets);

	TArray<TSharedPtr<FJsonValue>> ResultArray;
	for (const FAssetData& Asset : AllAssets)
	{
		FString AssetName = Asset.AssetName.ToString();
		FString AssetPath = Asset.GetObjectPathString();

		if (AssetName.Contains(Query, ESearchCase::IgnoreCase) ||
			AssetPath.Contains(Query, ESearchCase::IgnoreCase))
		{
			TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
			Obj->SetStringField(TEXT("name"), AssetName);
			Obj->SetStringField(TEXT("path"), AssetPath);
			Obj->SetStringField(TEXT("class"), Asset.AssetClassPath.GetAssetName().ToString());

			ResultArray.Add(MakeShared<FJsonValueObject>(Obj));

			if (ResultArray.Num() >= MaxResults) break;
		}
	}

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetArrayField(TEXT("assets"), ResultArray);
	Data->SetNumberField(TEXT("count"), ResultArray.Num());

	return FCommandResult::Success(Data);
}

// ============================================
// get_assets_by_class
// Args: { "class_name": "StaticMesh", "path": "/Game/", "max_results": 50 }
// ============================================
FCommandResult FAssetCommandHandler::HandleGetAssetsByClass(const TSharedPtr<FJsonObject>& Args)
{
	FString ClassName;
	if (!Args->TryGetStringField(TEXT("class_name"), ClassName))
	{
		return FCommandResult::Fail(TEXT("Missing required 'class_name' argument"));
	}

	FString SearchPath;
	if (!Args->TryGetStringField(TEXT("path"), SearchPath))
	{
		SearchPath = TEXT("/Game/");
	}

	int32 MaxResults = 50;
	Args->TryGetNumberField(TEXT("max_results"), MaxResults);

	// Dynamic UClass lookup — find any UClass by name
	UClass* AssetClass = nullptr;

	for (TObjectIterator<UClass> It; It; ++It)
	{
		FString CName = It->GetName();
		// Strip 'U'/'A' prefix for matching: UStaticMesh -> StaticMesh
		FString ShortName = CName;
		if (ShortName.Len() > 1 && (ShortName[0] == 'U' || ShortName[0] == 'A'))
		{
			ShortName.RemoveAt(0);
		}

		if (CName.Equals(ClassName, ESearchCase::IgnoreCase) ||
			ShortName.Equals(ClassName, ESearchCase::IgnoreCase))
		{
			AssetClass = *It;
			break;
		}
	}

	if (!AssetClass)
	{
		return FCommandResult::Fail(FString::Printf(
			TEXT("Class '%s' not found. Pass any UClass name (e.g. StaticMesh, Material, Blueprint, AnimSequence, Texture2D, SoundWave, etc.)"),
			*ClassName));
	}

	FAssetRegistryModule& ARModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	TArray<FAssetData> Assets;
	ARModule.Get().GetAssetsByClass(AssetClass->GetClassPathName(), Assets, true);

	TArray<TSharedPtr<FJsonValue>> ResultArray;
	for (const FAssetData& Asset : Assets)
	{
		if (!Asset.GetObjectPathString().StartsWith(SearchPath))
			continue;

		TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
		Obj->SetStringField(TEXT("name"), Asset.AssetName.ToString());
		Obj->SetStringField(TEXT("path"), Asset.GetObjectPathString());
		ResultArray.Add(MakeShared<FJsonValueObject>(Obj));

		if (ResultArray.Num() >= MaxResults) break;
	}

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetArrayField(TEXT("assets"), ResultArray);
	Data->SetNumberField(TEXT("count"), ResultArray.Num());
	Data->SetStringField(TEXT("className"), ClassName);

	return FCommandResult::Success(Data);
}

// ============================================
// get_asset_details
// Args: { "asset_path": "/Game/Blueprints/BP_MyActor.BP_MyActor" } or { "asset_name": "BP_MyActor" }
// ============================================
FCommandResult FAssetCommandHandler::HandleGetAssetDetails(const TSharedPtr<FJsonObject>& Args)
{
	FString AssetPath;
	if (!Args->TryGetStringField(TEXT("asset_path"), AssetPath))
	{
		FString AssetName;
		if (!Args->TryGetStringField(TEXT("asset_name"), AssetName))
		{
			return FCommandResult::Fail(TEXT("Missing 'asset_path' or 'asset_name' argument"));
		}

		// Search by name
		FAssetRegistryModule& ARModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
		TArray<FAssetData> AllAssets;
		ARModule.Get().GetAllAssets(AllAssets, true);

		for (const FAssetData& Asset : AllAssets)
		{
			if (Asset.AssetName.ToString().Equals(AssetName, ESearchCase::IgnoreCase))
			{
				AssetPath = Asset.GetObjectPathString();
				break;
			}
		}

		if (AssetPath.IsEmpty())
		{
			return FCommandResult::Fail(FString::Printf(TEXT("Asset '%s' not found"), *AssetName));
		}
	}

	FAssetRegistryModule& ARModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	FAssetData AssetData = ARModule.Get().GetAssetByObjectPath(FSoftObjectPath(AssetPath));

	if (!AssetData.IsValid())
	{
		return FCommandResult::Fail(FString::Printf(TEXT("Asset not found at path '%s'"), *AssetPath));
	}

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("name"), AssetData.AssetName.ToString());
	Data->SetStringField(TEXT("path"), AssetPath);
	Data->SetStringField(TEXT("class"), AssetData.AssetClassPath.GetAssetName().ToString());
	Data->SetStringField(TEXT("packageName"), AssetData.PackageName.ToString());

	// Get file size from package path
	FString PackagePath = AssetData.PackagePath.ToString();
	Data->SetStringField(TEXT("packagePath"), PackagePath);

	// Add tag values
	TSharedPtr<FJsonObject> TagsObj = MakeShared<FJsonObject>();
	for (const auto& Tag : AssetData.TagsAndValues)
	{
		TagsObj->SetStringField(Tag.Key.ToString(), Tag.Value.GetValue());
	}
	Data->SetObjectField(TEXT("tags"), TagsObj);

	return FCommandResult::Success(Data);
}
