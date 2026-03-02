#include "PirateMCPAssetCommands.h"
#include "PirateMCPCommonUtils.h"
#include "PirateMCPModule.h"
#include "Editor.h"
#include "EditorAssetLibrary.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "FileHelpers.h"
#include "Misc/Paths.h"
#include "Misc/PackageName.h"
#include "UObject/SavePackage.h"
#include "ScopedTransaction.h"
#include "Factories/Factory.h"
#include "EditorFramework/AssetImportData.h"
#include "ObjectTools.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"

TSharedPtr<FJsonObject> FPirateMCPAssetCommands::HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params)
{
	if (CommandType == TEXT("asset_list"))
	{
		return HandleAssetList(Params);
	}
	else if (CommandType == TEXT("asset_get_info"))
	{
		return HandleAssetGetInfo(Params);
	}
	else if (CommandType == TEXT("asset_duplicate"))
	{
		return HandleAssetDuplicate(Params);
	}
	else if (CommandType == TEXT("asset_rename"))
	{
		return HandleAssetRename(Params);
	}
	else if (CommandType == TEXT("asset_delete"))
	{
		return HandleAssetDelete(Params);
	}
	else if (CommandType == TEXT("asset_import"))
	{
		return HandleAssetImport(Params);
	}
	else if (CommandType == TEXT("asset_save"))
	{
		return HandleAssetSave(Params);
	}
	else if (CommandType == TEXT("asset_find_references"))
	{
		return HandleAssetFindReferences(Params);
	}

	return FPirateMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Unknown asset command: %s"), *CommandType));
}

TSharedPtr<FJsonObject> FPirateMCPAssetCommands::HandleAssetList(const TSharedPtr<FJsonObject>& Params)
{
	FString Path = TEXT("/Game");
	Params->TryGetStringField(TEXT("path"), Path);

	FString ClassFilter;
	Params->TryGetStringField(TEXT("class_filter"), ClassFilter);

	bool bRecursive = true;
	Params->TryGetBoolField(TEXT("recursive"), bRecursive);

	TArray<FString> AssetPaths;
	if (bRecursive)
	{
		UEditorAssetLibrary::ListAssets(Path, true, false);
		AssetPaths = UEditorAssetLibrary::ListAssets(Path, true, false);
	}
	else
	{
		AssetPaths = UEditorAssetLibrary::ListAssets(Path, false, false);
	}

	IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();

	TArray<TSharedPtr<FJsonValue>> AssetArray;
	for (const FString& AssetPath : AssetPaths)
	{
		FAssetData AssetData = AssetRegistry.GetAssetByObjectPath(FSoftObjectPath(AssetPath));
		if (!AssetData.IsValid()) continue;

		FString ClassName = AssetData.AssetClassPath.GetAssetName().ToString();

		// Apply class filter if specified
		if (!ClassFilter.IsEmpty() && !ClassName.Contains(ClassFilter))
		{
			continue;
		}

		TSharedPtr<FJsonObject> AssetObj = MakeShareable(new FJsonObject);
		AssetObj->SetStringField(TEXT("path"), AssetPath);
		AssetObj->SetStringField(TEXT("name"), AssetData.AssetName.ToString());
		AssetObj->SetStringField(TEXT("class"), ClassName);
		AssetArray.Add(MakeShareable(new FJsonValueObject(AssetObj)));
	}

	TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
	Result->SetBoolField(TEXT("success"), true);
	Result->SetNumberField(TEXT("count"), AssetArray.Num());
	Result->SetArrayField(TEXT("assets"), AssetArray);
	return Result;
}

TSharedPtr<FJsonObject> FPirateMCPAssetCommands::HandleAssetGetInfo(const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath;
	if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath))
	{
		return FPirateMCPCommonUtils::CreateErrorResponse(TEXT("Missing required 'asset_path' parameter"));
	}

	if (!UEditorAssetLibrary::DoesAssetExist(AssetPath))
	{
		return FPirateMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Asset not found: %s"), *AssetPath));
	}

	IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
	FAssetData AssetData = AssetRegistry.GetAssetByObjectPath(FSoftObjectPath(AssetPath));

	TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("path"), AssetPath);
	Result->SetStringField(TEXT("name"), AssetData.AssetName.ToString());
	Result->SetStringField(TEXT("class"), AssetData.AssetClassPath.GetAssetName().ToString());
	Result->SetStringField(TEXT("package_name"), AssetData.PackageName.ToString());

	// Get disk file size
	FString DiskPath;
	if (FPackageName::DoesPackageExist(AssetData.PackageName.ToString(), &DiskPath))
	{
		Result->SetStringField(TEXT("disk_path"), DiskPath);
		int64 FileSize = IFileManager::Get().FileSize(*DiskPath);
		Result->SetNumberField(TEXT("file_size_bytes"), (double)FileSize);
	}

	// Check if dirty
	UObject* Asset = AssetData.GetAsset();
	if (Asset && Asset->GetOutermost())
	{
		Result->SetBoolField(TEXT("is_dirty"), Asset->GetOutermost()->IsDirty());
	}

	return Result;
}

TSharedPtr<FJsonObject> FPirateMCPAssetCommands::HandleAssetDuplicate(const TSharedPtr<FJsonObject>& Params)
{
	FString SourcePath, DestPath;
	if (!Params->TryGetStringField(TEXT("source_path"), SourcePath))
	{
		return FPirateMCPCommonUtils::CreateErrorResponse(TEXT("Missing required 'source_path' parameter"));
	}
	if (!Params->TryGetStringField(TEXT("dest_path"), DestPath))
	{
		return FPirateMCPCommonUtils::CreateErrorResponse(TEXT("Missing required 'dest_path' parameter"));
	}

	FScopedTransaction Transaction(NSLOCTEXT("PirateMCP", "DuplicateAsset", "PirateMCP: Duplicate Asset"));

	UObject* DupAsset = UEditorAssetLibrary::DuplicateAsset(SourcePath, DestPath);
	if (!DupAsset)
	{
		return FPirateMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Failed to duplicate %s to %s"), *SourcePath, *DestPath));
	}

	TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("source_path"), SourcePath);
	Result->SetStringField(TEXT("dest_path"), DestPath);
	Result->SetStringField(TEXT("message"), TEXT("Asset duplicated successfully"));
	return Result;
}

TSharedPtr<FJsonObject> FPirateMCPAssetCommands::HandleAssetRename(const TSharedPtr<FJsonObject>& Params)
{
	FString SourcePath, DestPath;
	if (!Params->TryGetStringField(TEXT("source_path"), SourcePath))
	{
		return FPirateMCPCommonUtils::CreateErrorResponse(TEXT("Missing required 'source_path' parameter"));
	}
	if (!Params->TryGetStringField(TEXT("dest_path"), DestPath))
	{
		return FPirateMCPCommonUtils::CreateErrorResponse(TEXT("Missing required 'dest_path' parameter"));
	}

	FScopedTransaction Transaction(NSLOCTEXT("PirateMCP", "RenameAsset", "PirateMCP: Rename Asset"));

	bool bResult = UEditorAssetLibrary::RenameAsset(SourcePath, DestPath);
	if (!bResult)
	{
		return FPirateMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Failed to rename %s to %s"), *SourcePath, *DestPath));
	}

	TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("source_path"), SourcePath);
	Result->SetStringField(TEXT("dest_path"), DestPath);
	Result->SetStringField(TEXT("message"), TEXT("Asset renamed/moved successfully"));
	return Result;
}

TSharedPtr<FJsonObject> FPirateMCPAssetCommands::HandleAssetDelete(const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath;
	if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath))
	{
		return FPirateMCPCommonUtils::CreateErrorResponse(TEXT("Missing required 'asset_path' parameter"));
	}

	if (!UEditorAssetLibrary::DoesAssetExist(AssetPath))
	{
		return FPirateMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Asset not found: %s"), *AssetPath));
	}

	FScopedTransaction Transaction(NSLOCTEXT("PirateMCP", "DeleteAsset", "PirateMCP: Delete Asset"));

	bool bResult = UEditorAssetLibrary::DeleteAsset(AssetPath);
	if (!bResult)
	{
		return FPirateMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Failed to delete %s"), *AssetPath));
	}

	TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("asset_path"), AssetPath);
	Result->SetStringField(TEXT("message"), TEXT("Asset deleted successfully"));
	return Result;
}

TSharedPtr<FJsonObject> FPirateMCPAssetCommands::HandleAssetImport(const TSharedPtr<FJsonObject>& Params)
{
	FString FilePath, DestPath;
	if (!Params->TryGetStringField(TEXT("file_path"), FilePath))
	{
		return FPirateMCPCommonUtils::CreateErrorResponse(TEXT("Missing required 'file_path' parameter"));
	}
	if (!Params->TryGetStringField(TEXT("dest_path"), DestPath))
	{
		return FPirateMCPCommonUtils::CreateErrorResponse(TEXT("Missing required 'dest_path' parameter (e.g. /Game/Pirate/Meshes)"));
	}

	if (!FPaths::FileExists(FilePath))
	{
		return FPirateMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("File not found on disk: %s"), *FilePath));
	}

	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();

	TArray<FString> FilesToImport;
	FilesToImport.Add(FilePath);

	TArray<UObject*> ImportedAssets = AssetTools.ImportAssets(FilesToImport, DestPath);

	if (ImportedAssets.Num() == 0)
	{
		return FPirateMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Import failed for %s"), *FilePath));
	}

	TArray<TSharedPtr<FJsonValue>> ImportedArray;
	for (UObject* Asset : ImportedAssets)
	{
		TSharedPtr<FJsonObject> AssetObj = MakeShareable(new FJsonObject);
		AssetObj->SetStringField(TEXT("name"), Asset->GetName());
		AssetObj->SetStringField(TEXT("class"), Asset->GetClass()->GetName());
		AssetObj->SetStringField(TEXT("path"), Asset->GetPathName());
		ImportedArray.Add(MakeShareable(new FJsonValueObject(AssetObj)));
	}

	TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
	Result->SetBoolField(TEXT("success"), true);
	Result->SetArrayField(TEXT("imported_assets"), ImportedArray);
	Result->SetStringField(TEXT("message"), FString::Printf(TEXT("Imported %d asset(s)"), ImportedAssets.Num()));
	return Result;
}

TSharedPtr<FJsonObject> FPirateMCPAssetCommands::HandleAssetSave(const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath;
	if (Params->TryGetStringField(TEXT("asset_path"), AssetPath))
	{
		// Save specific asset
		if (!UEditorAssetLibrary::DoesAssetExist(AssetPath))
		{
			return FPirateMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Asset not found: %s"), *AssetPath));
		}

		bool bResult = UEditorAssetLibrary::SaveAsset(AssetPath, false);

		TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
		Result->SetBoolField(TEXT("success"), bResult);
		Result->SetStringField(TEXT("asset_path"), AssetPath);
		Result->SetStringField(TEXT("message"), bResult ? TEXT("Asset saved") : TEXT("Failed to save asset"));
		return Result;
	}

	// Save all dirty
	FEditorFileUtils::SaveDirtyPackages(false, true, true);

	TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("message"), TEXT("All dirty packages saved"));
	return Result;
}

TSharedPtr<FJsonObject> FPirateMCPAssetCommands::HandleAssetFindReferences(const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath;
	if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath))
	{
		return FPirateMCPCommonUtils::CreateErrorResponse(TEXT("Missing required 'asset_path' parameter"));
	}

	IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();

	// Convert asset path to package name
	FString PackageName = AssetPath;
	if (PackageName.Contains(TEXT(".")))
	{
		PackageName = FPackageName::ObjectPathToPackageName(PackageName);
	}

	TArray<FName> Referencers;
	AssetRegistry.GetReferencers(FName(*PackageName), Referencers);

	TArray<TSharedPtr<FJsonValue>> RefArray;
	for (const FName& Ref : Referencers)
	{
		RefArray.Add(MakeShareable(new FJsonValueString(Ref.ToString())));
	}

	TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("asset_path"), AssetPath);
	Result->SetNumberField(TEXT("reference_count"), RefArray.Num());
	Result->SetArrayField(TEXT("referencers"), RefArray);
	return Result;
}
