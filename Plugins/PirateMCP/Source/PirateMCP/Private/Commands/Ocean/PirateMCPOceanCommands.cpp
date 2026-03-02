#include "PirateMCPOceanCommands.h"
#include "PirateMCPCommonUtils.h"
#include "PirateMCPModule.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "EditorAssetLibrary.h"
#include "GameFramework/Actor.h"
#include "Components/PrimitiveComponent.h"

TSharedPtr<FJsonObject> FPirateMCPOceanCommands::HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params)
{
	if (CommandType == TEXT("ocean_set_material_scalar"))  return HandleSetMaterialScalar(Params);
	if (CommandType == TEXT("ocean_set_material_vector"))  return HandleSetMaterialVector(Params);
	if (CommandType == TEXT("ocean_spawn_niagara"))        return HandleSpawnNiagara(Params);
	if (CommandType == TEXT("ocean_set_niagara_float"))    return HandleSetNiagaraFloat(Params);
	if (CommandType == TEXT("ocean_set_niagara_vector"))   return HandleSetNiagaraVector(Params);
	if (CommandType == TEXT("ocean_list_water_bodies"))    return HandleListWaterBodies(Params);
	return FPirateMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Unknown ocean command: %s"), *CommandType));
}

TSharedPtr<FJsonObject> FPirateMCPOceanCommands::HandleSetMaterialScalar(const TSharedPtr<FJsonObject>& Params)
{
	FString ActorName, ParamName;
	double Value = 0.0;
	if (!Params->TryGetStringField(TEXT("actor_name"), ActorName))
		return FPirateMCPCommonUtils::CreateErrorResponse(TEXT("Missing actor_name"));
	if (!Params->TryGetStringField(TEXT("param_name"), ParamName))
		return FPirateMCPCommonUtils::CreateErrorResponse(TEXT("Missing param_name"));
	if (!Params->TryGetNumberField(TEXT("value"), Value))
		return FPirateMCPCommonUtils::CreateErrorResponse(TEXT("Missing value"));

	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World) return FPirateMCPCommonUtils::CreateErrorResponse(TEXT("No editor world"));

	TArray<AActor*> Actors;
	UGameplayStatics::GetAllActorsOfClass(World, AActor::StaticClass(), Actors);
	for (AActor* Actor : Actors)
	{
		if (!Actor || Actor->GetName() != ActorName) continue;
		TArray<UPrimitiveComponent*> Comps;
		Actor->GetComponents<UPrimitiveComponent>(Comps);
		for (UPrimitiveComponent* Comp : Comps)
		{
			for (int32 i = 0; i < Comp->GetNumMaterials(); i++)
			{
				UMaterialInstanceDynamic* MID = Cast<UMaterialInstanceDynamic>(Comp->GetMaterial(i));
				if (!MID) MID = Comp->CreateAndSetMaterialInstanceDynamic(i);
				if (MID) MID->SetScalarParameterValue(FName(*ParamName), (float)Value);
			}
		}
		TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
		Result->SetBoolField(TEXT("success"), true);
		Result->SetStringField(TEXT("actor"), ActorName);
		Result->SetStringField(TEXT("param"), ParamName);
		Result->SetNumberField(TEXT("value"), Value);
		return Result;
	}
	return FPirateMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Actor not found: %s"), *ActorName));
}

TSharedPtr<FJsonObject> FPirateMCPOceanCommands::HandleSetMaterialVector(const TSharedPtr<FJsonObject>& Params)
{
	FString ActorName, ParamName;
	if (!Params->TryGetStringField(TEXT("actor_name"), ActorName))
		return FPirateMCPCommonUtils::CreateErrorResponse(TEXT("Missing actor_name"));
	if (!Params->TryGetStringField(TEXT("param_name"), ParamName))
		return FPirateMCPCommonUtils::CreateErrorResponse(TEXT("Missing param_name"));
	if (!Params->HasField(TEXT("value")))
		return FPirateMCPCommonUtils::CreateErrorResponse(TEXT("Missing value (array [r,g,b,a])"));

	const TArray<TSharedPtr<FJsonValue>>* ValArray;
	if (!Params->TryGetArrayField(TEXT("value"), ValArray) || ValArray->Num() < 3)
		return FPirateMCPCommonUtils::CreateErrorResponse(TEXT("value must be [r,g,b] or [r,g,b,a]"));

	FLinearColor Color;
	Color.R = (float)(*ValArray)[0]->AsNumber();
	Color.G = (float)(*ValArray)[1]->AsNumber();
	Color.B = (float)(*ValArray)[2]->AsNumber();
	Color.A = ValArray->Num() >= 4 ? (float)(*ValArray)[3]->AsNumber() : 1.0f;

	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World) return FPirateMCPCommonUtils::CreateErrorResponse(TEXT("No editor world"));

	TArray<AActor*> Actors;
	UGameplayStatics::GetAllActorsOfClass(World, AActor::StaticClass(), Actors);
	for (AActor* Actor : Actors)
	{
		if (!Actor || Actor->GetName() != ActorName) continue;
		TArray<UPrimitiveComponent*> Comps;
		Actor->GetComponents<UPrimitiveComponent>(Comps);
		for (UPrimitiveComponent* Comp : Comps)
		{
			for (int32 i = 0; i < Comp->GetNumMaterials(); i++)
			{
				UMaterialInstanceDynamic* MID = Cast<UMaterialInstanceDynamic>(Comp->GetMaterial(i));
				if (!MID) MID = Comp->CreateAndSetMaterialInstanceDynamic(i);
				if (MID) MID->SetVectorParameterValue(FName(*ParamName), Color);
			}
		}
		TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
		Result->SetBoolField(TEXT("success"), true);
		Result->SetStringField(TEXT("actor"), ActorName);
		Result->SetStringField(TEXT("param"), ParamName);
		return Result;
	}
	return FPirateMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Actor not found: %s"), *ActorName));
}

TSharedPtr<FJsonObject> FPirateMCPOceanCommands::HandleSpawnNiagara(const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath;
	if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath))
		return FPirateMCPCommonUtils::CreateErrorResponse(TEXT("Missing asset_path"));

	FVector Location = Params->HasField(TEXT("location"))
		? FPirateMCPCommonUtils::GetVectorFromJson(Params, TEXT("location")) : FVector::ZeroVector;
	FRotator Rotation = Params->HasField(TEXT("rotation"))
		? FPirateMCPCommonUtils::GetRotatorFromJson(Params, TEXT("rotation")) : FRotator::ZeroRotator;

	UNiagaraSystem* NS = Cast<UNiagaraSystem>(UEditorAssetLibrary::LoadAsset(AssetPath));
	if (!NS)
		return FPirateMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Could not load Niagara system: %s"), *AssetPath));

	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World) return FPirateMCPCommonUtils::CreateErrorResponse(TEXT("No editor world"));

	UNiagaraComponent* Comp = UNiagaraFunctionLibrary::SpawnSystemAtLocation(World, NS, Location, Rotation);
	if (!Comp)
		return FPirateMCPCommonUtils::CreateErrorResponse(TEXT("Failed to spawn Niagara system"));

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("asset_path"), AssetPath);
	return Result;
}

TSharedPtr<FJsonObject> FPirateMCPOceanCommands::HandleSetNiagaraFloat(const TSharedPtr<FJsonObject>& Params)
{
	FString ActorName, VarName;
	double Value = 0.0;
	if (!Params->TryGetStringField(TEXT("actor_name"), ActorName))
		return FPirateMCPCommonUtils::CreateErrorResponse(TEXT("Missing actor_name"));
	if (!Params->TryGetStringField(TEXT("variable_name"), VarName))
		return FPirateMCPCommonUtils::CreateErrorResponse(TEXT("Missing variable_name"));
	if (!Params->TryGetNumberField(TEXT("value"), Value))
		return FPirateMCPCommonUtils::CreateErrorResponse(TEXT("Missing value"));

	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World) return FPirateMCPCommonUtils::CreateErrorResponse(TEXT("No editor world"));

	TArray<AActor*> Actors;
	UGameplayStatics::GetAllActorsOfClass(World, AActor::StaticClass(), Actors);
	for (AActor* Actor : Actors)
	{
		if (!Actor || Actor->GetName() != ActorName) continue;
		UNiagaraComponent* Comp = Actor->FindComponentByClass<UNiagaraComponent>();
		if (!Comp)
			return FPirateMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Actor has no Niagara component: %s"), *ActorName));
		Comp->SetVariableFloat(FName(*VarName), (float)Value);
		TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
		Result->SetBoolField(TEXT("success"), true);
		Result->SetStringField(TEXT("actor"), ActorName);
		Result->SetStringField(TEXT("variable"), VarName);
		Result->SetNumberField(TEXT("value"), Value);
		return Result;
	}
	return FPirateMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Actor not found: %s"), *ActorName));
}

TSharedPtr<FJsonObject> FPirateMCPOceanCommands::HandleSetNiagaraVector(const TSharedPtr<FJsonObject>& Params)
{
	FString ActorName, VarName;
	if (!Params->TryGetStringField(TEXT("actor_name"), ActorName))
		return FPirateMCPCommonUtils::CreateErrorResponse(TEXT("Missing actor_name"));
	if (!Params->TryGetStringField(TEXT("variable_name"), VarName))
		return FPirateMCPCommonUtils::CreateErrorResponse(TEXT("Missing variable_name"));
	if (!Params->HasField(TEXT("value")))
		return FPirateMCPCommonUtils::CreateErrorResponse(TEXT("Missing value (array [x,y,z])"));

	FVector Vec = FPirateMCPCommonUtils::GetVectorFromJson(Params, TEXT("value"));

	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World) return FPirateMCPCommonUtils::CreateErrorResponse(TEXT("No editor world"));

	TArray<AActor*> Actors;
	UGameplayStatics::GetAllActorsOfClass(World, AActor::StaticClass(), Actors);
	for (AActor* Actor : Actors)
	{
		if (!Actor || Actor->GetName() != ActorName) continue;
		UNiagaraComponent* Comp = Actor->FindComponentByClass<UNiagaraComponent>();
		if (!Comp)
			return FPirateMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Actor has no Niagara component: %s"), *ActorName));
		Comp->SetVariableVec3(FName(*VarName), Vec);
		TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
		Result->SetBoolField(TEXT("success"), true);
		Result->SetStringField(TEXT("actor"), ActorName);
		Result->SetStringField(TEXT("variable"), VarName);
		return Result;
	}
	return FPirateMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Actor not found: %s"), *ActorName));
}

TSharedPtr<FJsonObject> FPirateMCPOceanCommands::HandleListWaterBodies(const TSharedPtr<FJsonObject>& Params)
{
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World) return FPirateMCPCommonUtils::CreateErrorResponse(TEXT("No editor world"));

	TArray<AActor*> AllActors;
	UGameplayStatics::GetAllActorsOfClass(World, AActor::StaticClass(), AllActors);

	TArray<TSharedPtr<FJsonValue>> WaterBodies;
	for (AActor* Actor : AllActors)
	{
		if (!Actor) continue;
		FString ClassName = Actor->GetClass()->GetName();
		if (ClassName.Contains(TEXT("Water")) || ClassName.Contains(TEXT("Ocean")))
			WaterBodies.Add(FPirateMCPCommonUtils::ActorToJson(Actor));
	}

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetArrayField(TEXT("water_bodies"), WaterBodies);
	return Result;
}
