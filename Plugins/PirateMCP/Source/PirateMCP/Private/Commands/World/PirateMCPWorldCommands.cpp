#include "PirateMCPWorldCommands.h"
#include "PirateMCPCommonUtils.h"
#include "PirateMCPModule.h"
#include "Editor.h"
#include "EditorViewportClient.h"
#include "LevelEditorViewport.h"
#include "FileHelpers.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/Actor.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "EditorAssetLibrary.h"
#include "Engine/LevelStreaming.h"
#include "Engine/World.h"

TSharedPtr<FJsonObject> FPirateMCPWorldCommands::HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params)
{
	if (CommandType == TEXT("world_get_info"))               return HandleGetInfo(Params);
	if (CommandType == TEXT("world_open_level"))             return HandleOpenLevel(Params);
	if (CommandType == TEXT("world_list_levels"))            return HandleListLevels(Params);
	if (CommandType == TEXT("world_set_editor_camera"))      return HandleSetEditorCamera(Params);
	if (CommandType == TEXT("world_list_streaming_levels"))  return HandleListStreamingLevels(Params);
	if (CommandType == TEXT("world_load_streaming_level"))   return HandleLoadStreamingLevel(Params);
	if (CommandType == TEXT("world_unload_streaming_level")) return HandleUnloadStreamingLevel(Params);
	return FPirateMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Unknown world command: %s"), *CommandType));
}

TSharedPtr<FJsonObject> FPirateMCPWorldCommands::HandleGetInfo(const TSharedPtr<FJsonObject>& Params)
{
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World) return FPirateMCPCommonUtils::CreateErrorResponse(TEXT("No editor world"));

	TArray<AActor*> AllActors;
	UGameplayStatics::GetAllActorsOfClass(World, AActor::StaticClass(), AllActors);

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("level_name"), World->GetMapName());
	Result->SetStringField(TEXT("package_name"), World->GetPackage()->GetName());
	Result->SetNumberField(TEXT("actor_count"), AllActors.Num());
	Result->SetBoolField(TEXT("world_partition"), World->IsPartitionedWorld());
	Result->SetBoolField(TEXT("is_play_in_editor"), World->IsPlayInEditor());
	return Result;
}

TSharedPtr<FJsonObject> FPirateMCPWorldCommands::HandleOpenLevel(const TSharedPtr<FJsonObject>& Params)
{
	FString LevelPath;
	if (!Params->TryGetStringField(TEXT("level_path"), LevelPath))
		return FPirateMCPCommonUtils::CreateErrorResponse(TEXT("Missing level_path"));

	if (!UEditorAssetLibrary::DoesAssetExist(LevelPath))
		return FPirateMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Level not found: %s"), *LevelPath));

	bool bOpened = FEditorFileUtils::LoadMap(LevelPath, false, true);
	if (!bOpened)
		return FPirateMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Failed to open level: %s"), *LevelPath));

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("level_path"), LevelPath);
	return Result;
}

TSharedPtr<FJsonObject> FPirateMCPWorldCommands::HandleListLevels(const TSharedPtr<FJsonObject>& Params)
{
	FAssetRegistryModule& AR = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	FARFilter Filter;
	Filter.ClassPaths.Add(UWorld::StaticClass()->GetClassPathName());
	Filter.bRecursivePaths = true;
	Filter.PackagePaths.Add(TEXT("/Game"));

	TArray<FAssetData> Assets;
	AR.Get().GetAssets(Filter, Assets);

	TArray<TSharedPtr<FJsonValue>> LevelList;
	for (const FAssetData& Asset : Assets)
	{
		TSharedPtr<FJsonObject> Entry = MakeShared<FJsonObject>();
		Entry->SetStringField(TEXT("name"), Asset.AssetName.ToString());
		Entry->SetStringField(TEXT("path"), Asset.GetObjectPathString());
		LevelList.Add(MakeShared<FJsonValueObject>(Entry));
	}

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetArrayField(TEXT("levels"), LevelList);
	return Result;
}

TSharedPtr<FJsonObject> FPirateMCPWorldCommands::HandleSetEditorCamera(const TSharedPtr<FJsonObject>& Params)
{
	if (!Params->HasField(TEXT("location")))
		return FPirateMCPCommonUtils::CreateErrorResponse(TEXT("Missing location parameter"));

	FVector Location = FPirateMCPCommonUtils::GetVectorFromJson(Params, TEXT("location"));
	FRotator Rotation = Params->HasField(TEXT("rotation"))
		? FPirateMCPCommonUtils::GetRotatorFromJson(Params, TEXT("rotation"))
		: FRotator::ZeroRotator;

	// Find the active level editor viewport
	for (FLevelEditorViewportClient* ViewportClient : GEditor->GetLevelViewportClients())
	{
		if (ViewportClient && ViewportClient->IsPerspective())
		{
			ViewportClient->SetViewLocation(Location);
			ViewportClient->SetViewRotation(Rotation);
			ViewportClient->Invalidate();
			break;
		}
	}

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	return Result;
}

TSharedPtr<FJsonObject> FPirateMCPWorldCommands::HandleListStreamingLevels(const TSharedPtr<FJsonObject>& Params)
{
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World) return FPirateMCPCommonUtils::CreateErrorResponse(TEXT("No editor world"));

	TArray<TSharedPtr<FJsonValue>> Levels;
	for (ULevelStreaming* StreamingLevel : World->GetStreamingLevels())
	{
		if (!StreamingLevel) continue;
		TSharedPtr<FJsonObject> Entry = MakeShared<FJsonObject>();
		Entry->SetStringField(TEXT("package_name"), StreamingLevel->GetWorldAssetPackageName());
		Entry->SetBoolField(TEXT("is_loaded"), StreamingLevel->IsLevelLoaded());
		Entry->SetBoolField(TEXT("is_visible"), StreamingLevel->IsLevelVisible());
		Levels.Add(MakeShared<FJsonValueObject>(Entry));
	}

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetArrayField(TEXT("streaming_levels"), Levels);
	return Result;
}

TSharedPtr<FJsonObject> FPirateMCPWorldCommands::HandleLoadStreamingLevel(const TSharedPtr<FJsonObject>& Params)
{
	FString PackageName;
	if (!Params->TryGetStringField(TEXT("package_name"), PackageName))
		return FPirateMCPCommonUtils::CreateErrorResponse(TEXT("Missing package_name"));

	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World) return FPirateMCPCommonUtils::CreateErrorResponse(TEXT("No editor world"));

	for (ULevelStreaming* StreamingLevel : World->GetStreamingLevels())
	{
		if (!StreamingLevel) continue;
		if (StreamingLevel->GetWorldAssetPackageName() == PackageName)
		{
			StreamingLevel->SetShouldBeLoaded(true);
			StreamingLevel->SetShouldBeVisible(true);
			World->FlushLevelStreaming();
			TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
			Result->SetBoolField(TEXT("success"), true);
			Result->SetStringField(TEXT("package_name"), PackageName);
			return Result;
		}
	}
	return FPirateMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Streaming level not found: %s"), *PackageName));
}

TSharedPtr<FJsonObject> FPirateMCPWorldCommands::HandleUnloadStreamingLevel(const TSharedPtr<FJsonObject>& Params)
{
	FString PackageName;
	if (!Params->TryGetStringField(TEXT("package_name"), PackageName))
		return FPirateMCPCommonUtils::CreateErrorResponse(TEXT("Missing package_name"));

	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World) return FPirateMCPCommonUtils::CreateErrorResponse(TEXT("No editor world"));

	for (ULevelStreaming* StreamingLevel : World->GetStreamingLevels())
	{
		if (!StreamingLevel) continue;
		if (StreamingLevel->GetWorldAssetPackageName() == PackageName)
		{
			StreamingLevel->SetShouldBeLoaded(false);
			StreamingLevel->SetShouldBeVisible(false);
			World->FlushLevelStreaming();
			TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
			Result->SetBoolField(TEXT("success"), true);
			Result->SetStringField(TEXT("package_name"), PackageName);
			return Result;
		}
	}
	return FPirateMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Streaming level not found: %s"), *PackageName));
}
