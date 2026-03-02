#pragma once
#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

class PIRATEMCP_API FPirateMCPAssetCommands
{
public:
	TSharedPtr<FJsonObject> HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params);

private:
	TSharedPtr<FJsonObject> HandleAssetList(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleAssetGetInfo(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleAssetDuplicate(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleAssetRename(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleAssetDelete(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleAssetImport(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleAssetSave(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleAssetFindReferences(const TSharedPtr<FJsonObject>& Params);
};
