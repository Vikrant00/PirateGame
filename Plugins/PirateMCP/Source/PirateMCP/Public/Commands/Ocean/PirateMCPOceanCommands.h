#pragma once
#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

class FPirateMCPOceanCommands
{
public:
	TSharedPtr<FJsonObject> HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params);
private:
	TSharedPtr<FJsonObject> HandleSetMaterialScalar(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleSetMaterialVector(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleSpawnNiagara(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleSetNiagaraFloat(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleSetNiagaraVector(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleListWaterBodies(const TSharedPtr<FJsonObject>& Params);
};
