#pragma once
#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

class FPirateMCPEditorCommands
{
public:
	FPirateMCPEditorCommands();
	TSharedPtr<FJsonObject> HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params);
private:
	TSharedPtr<FJsonObject> HandleGetActorsInLevel(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleFindActorsByName(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleSpawnActor(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleDeleteActor(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleSetActorTransform(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleSpawnBlueprintActor(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleActorGetByClass(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleActorSetCollisionProfile(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleActorSetMaterialScalarParam(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleActorAddNiagaraComponent(const TSharedPtr<FJsonObject>& Params);
};
