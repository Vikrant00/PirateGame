#include "PirateMCPEditorUtilityCommands.h"
#include "PirateMCPCommonUtils.h"
#include "PirateMCPModule.h"
#include "Editor.h"
#include "EditorViewportClient.h"
#include "LevelEditorViewport.h"
#include "Engine/Selection.h"
#include "GameFramework/Actor.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "FileHelpers.h"
#include "ImageUtils.h"
#include "HighResScreenshot.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "EditorAssetLibrary.h"
#include "ScopedTransaction.h"
#include "Subsystems/EditorActorSubsystem.h"
#include "UnrealEdGlobals.h"
#include "Editor/UnrealEdEngine.h"

TSharedPtr<FJsonObject> FPirateMCPEditorUtilityCommands::HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params)
{
	if (CommandType == TEXT("editor_undo"))
	{
		return HandleUndo(Params);
	}
	else if (CommandType == TEXT("editor_redo"))
	{
		return HandleRedo(Params);
	}
	else if (CommandType == TEXT("editor_save_all"))
	{
		return HandleSaveAll(Params);
	}
	else if (CommandType == TEXT("editor_play"))
	{
		return HandlePlay(Params);
	}
	else if (CommandType == TEXT("editor_stop"))
	{
		return HandleStop(Params);
	}
	else if (CommandType == TEXT("editor_get_selected"))
	{
		return HandleGetSelected(Params);
	}
	else if (CommandType == TEXT("editor_select_actors"))
	{
		return HandleSelectActors(Params);
	}
	else if (CommandType == TEXT("editor_take_screenshot"))
	{
		return HandleTakeScreenshot(Params);
	}
	else if (CommandType == TEXT("editor_get_output_log"))
	{
		return HandleGetOutputLog(Params);
	}

	return FPirateMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Unknown editor utility command: %s"), *CommandType));
}

TSharedPtr<FJsonObject> FPirateMCPEditorUtilityCommands::HandleUndo(const TSharedPtr<FJsonObject>& Params)
{
	if (!GEditor)
	{
		return FPirateMCPCommonUtils::CreateErrorResponse(TEXT("No editor available"));
	}

	bool bSuccess = GEditor->UndoTransaction(true);

	TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
	Result->SetBoolField(TEXT("success"), bSuccess);
	Result->SetStringField(TEXT("message"), bSuccess ? TEXT("Undo performed") : TEXT("Nothing to undo"));
	return Result;
}

TSharedPtr<FJsonObject> FPirateMCPEditorUtilityCommands::HandleRedo(const TSharedPtr<FJsonObject>& Params)
{
	if (!GEditor)
	{
		return FPirateMCPCommonUtils::CreateErrorResponse(TEXT("No editor available"));
	}

	bool bSuccess = GEditor->RedoTransaction();

	TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
	Result->SetBoolField(TEXT("success"), bSuccess);
	Result->SetStringField(TEXT("message"), bSuccess ? TEXT("Redo performed") : TEXT("Nothing to redo"));
	return Result;
}

TSharedPtr<FJsonObject> FPirateMCPEditorUtilityCommands::HandleSaveAll(const TSharedPtr<FJsonObject>& Params)
{
	FEditorFileUtils::SaveDirtyPackages(
		/*bPromptUserToSave=*/false,
		/*bSaveMapPackages=*/true,
		/*bSaveContentPackages=*/true
	);

	TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("message"), TEXT("All dirty packages saved"));
	return Result;
}

TSharedPtr<FJsonObject> FPirateMCPEditorUtilityCommands::HandlePlay(const TSharedPtr<FJsonObject>& Params)
{
	if (!GEditor)
	{
		return FPirateMCPCommonUtils::CreateErrorResponse(TEXT("No editor available"));
	}

	if (GEditor->PlayWorld != nullptr)
	{
		TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
		Result->SetBoolField(TEXT("success"), true);
		Result->SetStringField(TEXT("message"), TEXT("PIE is already running"));
		return Result;
	}

	FRequestPlaySessionParams PIEParams;
	GEditor->RequestPlaySession(PIEParams);

	TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("message"), TEXT("Play In Editor started"));
	return Result;
}

TSharedPtr<FJsonObject> FPirateMCPEditorUtilityCommands::HandleStop(const TSharedPtr<FJsonObject>& Params)
{
	if (!GEditor)
	{
		return FPirateMCPCommonUtils::CreateErrorResponse(TEXT("No editor available"));
	}

	if (GEditor->PlayWorld == nullptr)
	{
		TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
		Result->SetBoolField(TEXT("success"), true);
		Result->SetStringField(TEXT("message"), TEXT("PIE is not running"));
		return Result;
	}

	GEditor->RequestEndPlayMap();

	TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("message"), TEXT("Play In Editor stopped"));
	return Result;
}

TSharedPtr<FJsonObject> FPirateMCPEditorUtilityCommands::HandleGetSelected(const TSharedPtr<FJsonObject>& Params)
{
	if (!GEditor)
	{
		return FPirateMCPCommonUtils::CreateErrorResponse(TEXT("No editor available"));
	}

	USelection* Selection = GEditor->GetSelectedActors();
	TArray<TSharedPtr<FJsonValue>> ActorArray;

	for (int32 i = 0; i < Selection->Num(); ++i)
	{
		AActor* Actor = Cast<AActor>(Selection->GetSelectedObject(i));
		if (Actor)
		{
			TSharedPtr<FJsonObject> ActorObj = MakeShareable(new FJsonObject);
			ActorObj->SetStringField(TEXT("name"), Actor->GetName());
			ActorObj->SetStringField(TEXT("label"), Actor->GetActorLabel());
			ActorObj->SetStringField(TEXT("class"), Actor->GetClass()->GetName());

			FVector Loc = Actor->GetActorLocation();
			FRotator Rot = Actor->GetActorRotation();
			FVector Scale = Actor->GetActorScale3D();

			TArray<TSharedPtr<FJsonValue>> LocArr;
			LocArr.Add(MakeShareable(new FJsonValueNumber(Loc.X)));
			LocArr.Add(MakeShareable(new FJsonValueNumber(Loc.Y)));
			LocArr.Add(MakeShareable(new FJsonValueNumber(Loc.Z)));
			ActorObj->SetArrayField(TEXT("location"), LocArr);

			TArray<TSharedPtr<FJsonValue>> RotArr;
			RotArr.Add(MakeShareable(new FJsonValueNumber(Rot.Pitch)));
			RotArr.Add(MakeShareable(new FJsonValueNumber(Rot.Yaw)));
			RotArr.Add(MakeShareable(new FJsonValueNumber(Rot.Roll)));
			ActorObj->SetArrayField(TEXT("rotation"), RotArr);

			TArray<TSharedPtr<FJsonValue>> ScaleArr;
			ScaleArr.Add(MakeShareable(new FJsonValueNumber(Scale.X)));
			ScaleArr.Add(MakeShareable(new FJsonValueNumber(Scale.Y)));
			ScaleArr.Add(MakeShareable(new FJsonValueNumber(Scale.Z)));
			ActorObj->SetArrayField(TEXT("scale"), ScaleArr);

			ActorArray.Add(MakeShareable(new FJsonValueObject(ActorObj)));
		}
	}

	TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
	Result->SetBoolField(TEXT("success"), true);
	Result->SetNumberField(TEXT("count"), ActorArray.Num());
	Result->SetArrayField(TEXT("selected_actors"), ActorArray);
	return Result;
}

TSharedPtr<FJsonObject> FPirateMCPEditorUtilityCommands::HandleSelectActors(const TSharedPtr<FJsonObject>& Params)
{
	if (!GEditor)
	{
		return FPirateMCPCommonUtils::CreateErrorResponse(TEXT("No editor available"));
	}

	const TArray<TSharedPtr<FJsonValue>>* NamesArray;
	if (!Params->TryGetArrayField(TEXT("names"), NamesArray))
	{
		return FPirateMCPCommonUtils::CreateErrorResponse(TEXT("Missing required 'names' array parameter"));
	}

	UWorld* World = GEditor->GetEditorWorldContext().World();
	if (!World) return FPirateMCPCommonUtils::CreateErrorResponse(TEXT("No world available"));

	GEditor->SelectNone(true, true, false);

	int32 SelectedCount = 0;
	for (const TSharedPtr<FJsonValue>& NameVal : *NamesArray)
	{
		FString ActorName = NameVal->AsString();
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			if (It->GetName() == ActorName || It->GetActorLabel() == ActorName)
			{
				GEditor->SelectActor(*It, true, true, false);
				SelectedCount++;
				break;
			}
		}
	}

	TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
	Result->SetBoolField(TEXT("success"), true);
	Result->SetNumberField(TEXT("selected_count"), SelectedCount);
	return Result;
}

TSharedPtr<FJsonObject> FPirateMCPEditorUtilityCommands::HandleTakeScreenshot(const TSharedPtr<FJsonObject>& Params)
{
	if (!GEditor)
	{
		return FPirateMCPCommonUtils::CreateErrorResponse(TEXT("No editor available"));
	}

	// Get optional filename
	FString Filename;
	if (!Params->TryGetStringField(TEXT("filename"), Filename))
	{
		Filename = FString::Printf(TEXT("PirateMCP_Screenshot_%s.png"),
			*FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S")));
	}

	FString FullPath = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Screenshots"), Filename);
	FPaths::MakePathRelativeTo(FullPath, *FPaths::RootDir());
	FullPath = FPaths::ConvertRelativePathToFull(FullPath);

	// Ensure directory exists
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(FullPath), true);

	// Find an active viewport and take screenshot
	for (FLevelEditorViewportClient* ViewportClient : GEditor->GetLevelViewportClients())
	{
		if (ViewportClient && ViewportClient->IsPerspective())
		{
			FViewport* Viewport = ViewportClient->Viewport;
			if (Viewport)
			{
				TArray<FColor> Bitmap;
				int32 Width = Viewport->GetSizeXY().X;
				int32 Height = Viewport->GetSizeXY().Y;

				if (Width > 0 && Height > 0 && Viewport->ReadPixels(Bitmap))
				{
					TArray64<uint8> CompressedBitmap;
					FImageUtils::PNGCompressImageArray(Width, Height, Bitmap, CompressedBitmap);
					FFileHelper::SaveArrayToFile(CompressedBitmap, *FullPath);

					TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
					Result->SetBoolField(TEXT("success"), true);
					Result->SetStringField(TEXT("path"), FullPath);
					Result->SetNumberField(TEXT("width"), Width);
					Result->SetNumberField(TEXT("height"), Height);
					return Result;
				}
			}
		}
	}

	return FPirateMCPCommonUtils::CreateErrorResponse(TEXT("No active perspective viewport found for screenshot"));
}

TSharedPtr<FJsonObject> FPirateMCPEditorUtilityCommands::HandleGetOutputLog(const TSharedPtr<FJsonObject>& Params)
{
	// Get the last N log entries from PirateMCP category
	int32 MaxEntries = 50;
	Params->TryGetNumberField(TEXT("max_entries"), MaxEntries);
	MaxEntries = FMath::Clamp(MaxEntries, 1, 500);

	// We use exec_python to read the output log via Python since there's no clean
	// C++ API to read back log history. Return a message suggesting alternatives.
	TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("message"), TEXT("Use exec_python with 'import unreal; unreal.log(\"test\")' to interact with the output log. For reading logs, check the Saved/Logs/ directory."));
	Result->SetStringField(TEXT("log_dir"), FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Logs"))));
	return Result;
}
