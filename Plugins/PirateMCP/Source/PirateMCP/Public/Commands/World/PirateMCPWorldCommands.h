#pragma once
#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

class FPirateMCPWorldCommands
{
public:
	TSharedPtr<FJsonObject> HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params);
private:
	TSharedPtr<FJsonObject> HandleGetInfo(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleOpenLevel(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleListLevels(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleSetEditorCamera(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleListStreamingLevels(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleLoadStreamingLevel(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleUnloadStreamingLevel(const TSharedPtr<FJsonObject>& Params);
};
