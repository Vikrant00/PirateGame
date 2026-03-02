#pragma once
#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

class PIRATEMCP_API FPirateMCPPythonCommands
{
public:
	TSharedPtr<FJsonObject> HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params);

private:
	TSharedPtr<FJsonObject> HandleExecPython(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleExecConsoleCommand(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleGetPythonHelp(const TSharedPtr<FJsonObject>& Params);
};
