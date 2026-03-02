#pragma once
#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

class PIRATEMCP_API FPirateMCPEditorUtilityCommands
{
public:
	TSharedPtr<FJsonObject> HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params);

private:
	TSharedPtr<FJsonObject> HandleUndo(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleRedo(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleSaveAll(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandlePlay(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleStop(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleGetSelected(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleSelectActors(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleTakeScreenshot(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleGetOutputLog(const TSharedPtr<FJsonObject>& Params);
};
