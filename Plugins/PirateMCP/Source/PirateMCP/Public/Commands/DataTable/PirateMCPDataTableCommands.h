#pragma once
#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

class FPirateMCPDataTableCommands
{
public:
	TSharedPtr<FJsonObject> HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params);
private:
	TSharedPtr<FJsonObject> HandleListTables(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleGetRowNames(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleGetRow(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleGetAllRows(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleSetRow(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleDeleteRow(const TSharedPtr<FJsonObject>& Params);
};
