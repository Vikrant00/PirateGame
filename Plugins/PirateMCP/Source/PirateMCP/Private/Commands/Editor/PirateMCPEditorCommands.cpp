#include "PirateMCPEditorCommands.h"
#include "PirateMCPCommonUtils.h"
#include "PirateMCPModule.h"
#include "Editor.h"
#include "EditorViewportClient.h"
#include "LevelEditorViewport.h"
#include "ImageUtils.h"
#include "HighResScreenshot.h"
#include "Engine/GameViewportClient.h"
#include "Misc/FileHelper.h"
#include "GameFramework/Actor.h"
#include "EngineUtils.h"
#include "Engine/Selection.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/DirectionalLight.h"
#include "Engine/PointLight.h"
#include "Engine/SpotLight.h"
#include "Camera/CameraActor.h"
#include "Components/StaticMeshComponent.h"
#include "EditorSubsystem.h"
#include "Subsystems/EditorActorSubsystem.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "EditorAssetLibrary.h"
#include "PirateMCPBlueprintCommands.h"
#include "ScopedTransaction.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "NiagaraFunctionLibrary.h"
#include "Components/PrimitiveComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/UObjectIterator.h"

FPirateMCPEditorCommands::FPirateMCPEditorCommands()
{
}

TSharedPtr<FJsonObject> FPirateMCPEditorCommands::HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params)
{
    // Actor manipulation commands
    if (CommandType == TEXT("get_actors_in_level"))
    {
        return HandleGetActorsInLevel(Params);
    }
    else if (CommandType == TEXT("find_actors_by_name"))
    {
        return HandleFindActorsByName(Params);
    }
    else if (CommandType == TEXT("spawn_actor"))
    {
        return HandleSpawnActor(Params);
    }
    else if (CommandType == TEXT("delete_actor"))
    {
        return HandleDeleteActor(Params);
    }
    else if (CommandType == TEXT("set_actor_transform"))
    {
        return HandleSetActorTransform(Params);
    }
    // Blueprint actor spawning
    else if (CommandType == TEXT("spawn_blueprint_actor"))
    {
        return HandleSpawnBlueprintActor(Params);
    }
    // Ship/Actor extensions
    else if (CommandType == TEXT("actor_get_by_class"))
    {
        return HandleActorGetByClass(Params);
    }
    else if (CommandType == TEXT("actor_set_collision_profile"))
    {
        return HandleActorSetCollisionProfile(Params);
    }
    else if (CommandType == TEXT("actor_set_material_scalar_param"))
    {
        return HandleActorSetMaterialScalarParam(Params);
    }
    else if (CommandType == TEXT("actor_add_niagara_component"))
    {
        return HandleActorAddNiagaraComponent(Params);
    }
    // Property access
    else if (CommandType == TEXT("actor_get_properties"))
    {
        return HandleActorGetProperties(Params);
    }
    else if (CommandType == TEXT("actor_set_property"))
    {
        return HandleActorSetProperty(Params);
    }
    else if (CommandType == TEXT("actor_get_components"))
    {
        return HandleActorGetComponents(Params);
    }
    else if (CommandType == TEXT("actor_add_component"))
    {
        return HandleActorAddComponent(Params);
    }

    return FPirateMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Unknown editor command: %s"), *CommandType));
}

TSharedPtr<FJsonObject> FPirateMCPEditorCommands::HandleGetActorsInLevel(const TSharedPtr<FJsonObject>& Params)
{
    TArray<AActor*> AllActors;
    UGameplayStatics::GetAllActorsOfClass(GWorld, AActor::StaticClass(), AllActors);
    
    TArray<TSharedPtr<FJsonValue>> ActorArray;
    for (AActor* Actor : AllActors)
    {
        if (Actor)
        {
            ActorArray.Add(FPirateMCPCommonUtils::ActorToJson(Actor));
        }
    }
    
    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetArrayField(TEXT("actors"), ActorArray);
    
    return ResultObj;
}

TSharedPtr<FJsonObject> FPirateMCPEditorCommands::HandleFindActorsByName(const TSharedPtr<FJsonObject>& Params)
{
    FString Pattern;
    if (!Params->TryGetStringField(TEXT("pattern"), Pattern))
    {
        return FPirateMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'pattern' parameter"));
    }
    
    TArray<AActor*> AllActors;
    UGameplayStatics::GetAllActorsOfClass(GWorld, AActor::StaticClass(), AllActors);
    
    TArray<TSharedPtr<FJsonValue>> MatchingActors;
    for (AActor* Actor : AllActors)
    {
        if (Actor && Actor->GetName().Contains(Pattern))
        {
            MatchingActors.Add(FPirateMCPCommonUtils::ActorToJson(Actor));
        }
    }
    
    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetArrayField(TEXT("actors"), MatchingActors);
    
    return ResultObj;
}

TSharedPtr<FJsonObject> FPirateMCPEditorCommands::HandleSpawnActor(const TSharedPtr<FJsonObject>& Params)
{
    // Get required parameters
    FString ActorType;
    if (!Params->TryGetStringField(TEXT("type"), ActorType))
    {
        return FPirateMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'type' parameter"));
    }

    // Get actor name (required parameter)
    FString ActorName;
    if (!Params->TryGetStringField(TEXT("name"), ActorName))
    {
        return FPirateMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'name' parameter"));
    }

    // Get optional transform parameters
    FVector Location(0.0f, 0.0f, 0.0f);
    FRotator Rotation(0.0f, 0.0f, 0.0f);
    FVector Scale(1.0f, 1.0f, 1.0f);

    if (Params->HasField(TEXT("location")))
    {
        Location = FPirateMCPCommonUtils::GetVectorFromJson(Params, TEXT("location"));
    }
    if (Params->HasField(TEXT("rotation")))
    {
        Rotation = FPirateMCPCommonUtils::GetRotatorFromJson(Params, TEXT("rotation"));
    }
    if (Params->HasField(TEXT("scale")))
    {
        Scale = FPirateMCPCommonUtils::GetVectorFromJson(Params, TEXT("scale"));
    }

    // Create the actor based on type
    FScopedTransaction Transaction(NSLOCTEXT("PirateMCP", "SpawnActor", "PirateMCP: Spawn Actor"));
    AActor* NewActor = nullptr;
    UWorld* World = GEditor->GetEditorWorldContext().World();

    if (!World)
    {
        return FPirateMCPCommonUtils::CreateErrorResponse(TEXT("Failed to get editor world"));
    }

    // Check if an actor with this name already exists
    TArray<AActor*> AllActors;
    UGameplayStatics::GetAllActorsOfClass(World, AActor::StaticClass(), AllActors);
    for (AActor* Actor : AllActors)
    {
        if (Actor && Actor->GetName() == ActorName)
        {
            return FPirateMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Actor with name '%s' already exists"), *ActorName));
        }
    }

    FActorSpawnParameters SpawnParams;
    SpawnParams.Name = *ActorName;

    if (ActorType == TEXT("StaticMeshActor"))
    {
        AStaticMeshActor* NewMeshActor = World->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(), Location, Rotation, SpawnParams);
        if (NewMeshActor)
        {
            // Check for an optional static_mesh parameter to assign a mesh
            FString MeshPath;
            if (Params->TryGetStringField(TEXT("static_mesh"), MeshPath))
            {
                UStaticMesh* Mesh = Cast<UStaticMesh>(UEditorAssetLibrary::LoadAsset(MeshPath));
                if (Mesh)
                {
                    NewMeshActor->GetStaticMeshComponent()->SetStaticMesh(Mesh);
                }
                else
                {
                    UE_LOG(LogPirateMCP, Warning, TEXT("Could not find static mesh at path: %s"), *MeshPath);
                }
            }
        }
        NewActor = NewMeshActor;
    }
    else if (ActorType == TEXT("PointLight"))
    {
        NewActor = World->SpawnActor<APointLight>(APointLight::StaticClass(), Location, Rotation, SpawnParams);
    }
    else if (ActorType == TEXT("SpotLight"))
    {
        NewActor = World->SpawnActor<ASpotLight>(ASpotLight::StaticClass(), Location, Rotation, SpawnParams);
    }
    else if (ActorType == TEXT("DirectionalLight"))
    {
        NewActor = World->SpawnActor<ADirectionalLight>(ADirectionalLight::StaticClass(), Location, Rotation, SpawnParams);
    }
    else if (ActorType == TEXT("CameraActor"))
    {
        NewActor = World->SpawnActor<ACameraActor>(ACameraActor::StaticClass(), Location, Rotation, SpawnParams);
    }
    else
    {
        return FPirateMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Unknown actor type: %s"), *ActorType));
    }

    if (NewActor)
    {
        // Set scale (since SpawnActor only takes location and rotation)
        FTransform Transform = NewActor->GetTransform();
        Transform.SetScale3D(Scale);
        NewActor->SetActorTransform(Transform);

        // Return the created actor's details
        return FPirateMCPCommonUtils::ActorToJsonObject(NewActor, true);
    }

    return FPirateMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create actor"));
}

TSharedPtr<FJsonObject> FPirateMCPEditorCommands::HandleDeleteActor(const TSharedPtr<FJsonObject>& Params)
{
    FString ActorName;
    if (!Params->TryGetStringField(TEXT("name"), ActorName))
    {
        return FPirateMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'name' parameter"));
    }

    TArray<AActor*> AllActors;
    UGameplayStatics::GetAllActorsOfClass(GWorld, AActor::StaticClass(), AllActors);
    
    for (AActor* Actor : AllActors)
    {
        if (Actor && Actor->GetName() == ActorName)
        {
            // Store actor info before deletion for the response
            TSharedPtr<FJsonObject> ActorInfo = FPirateMCPCommonUtils::ActorToJsonObject(Actor);
            
            // Delete the actor
            FScopedTransaction Transaction(NSLOCTEXT("PirateMCP", "DeleteActor", "PirateMCP: Delete Actor"));
            Actor->Modify();
            Actor->Destroy();
            
            TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
            ResultObj->SetObjectField(TEXT("deleted_actor"), ActorInfo);
            return ResultObj;
        }
    }
    
    return FPirateMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Actor not found: %s"), *ActorName));
}

TSharedPtr<FJsonObject> FPirateMCPEditorCommands::HandleSetActorTransform(const TSharedPtr<FJsonObject>& Params)
{
    // Get actor name
    FString ActorName;
    if (!Params->TryGetStringField(TEXT("name"), ActorName))
    {
        return FPirateMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'name' parameter"));
    }

    // Find the actor
    AActor* TargetActor = nullptr;
    TArray<AActor*> AllActors;
    UGameplayStatics::GetAllActorsOfClass(GWorld, AActor::StaticClass(), AllActors);
    
    for (AActor* Actor : AllActors)
    {
        if (Actor && Actor->GetName() == ActorName)
        {
            TargetActor = Actor;
            break;
        }
    }

    if (!TargetActor)
    {
        return FPirateMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Actor not found: %s"), *ActorName));
    }

    // Get transform parameters
    FTransform NewTransform = TargetActor->GetTransform();

    if (Params->HasField(TEXT("location")))
    {
        NewTransform.SetLocation(FPirateMCPCommonUtils::GetVectorFromJson(Params, TEXT("location")));
    }
    if (Params->HasField(TEXT("rotation")))
    {
        NewTransform.SetRotation(FQuat(FPirateMCPCommonUtils::GetRotatorFromJson(Params, TEXT("rotation"))));
    }
    if (Params->HasField(TEXT("scale")))
    {
        NewTransform.SetScale3D(FPirateMCPCommonUtils::GetVectorFromJson(Params, TEXT("scale")));
    }

    // Set the new transform (undo-able)
    FScopedTransaction Transaction(NSLOCTEXT("PirateMCP", "SetActorTransform", "PirateMCP: Set Actor Transform"));
    TargetActor->Modify();
    TargetActor->SetActorTransform(NewTransform);

    // Return updated actor info
    return FPirateMCPCommonUtils::ActorToJsonObject(TargetActor, true);
}

TSharedPtr<FJsonObject> FPirateMCPEditorCommands::HandleSpawnBlueprintActor(const TSharedPtr<FJsonObject>& Params)
{
    // This function will now correctly call the implementation in BlueprintCommands
    FPirateMCPBlueprintCommands BlueprintCommands;
    return BlueprintCommands.HandleCommand(TEXT("spawn_blueprint_actor"), Params);
}

TSharedPtr<FJsonObject> FPirateMCPEditorCommands::HandleActorGetByClass(const TSharedPtr<FJsonObject>& Params)
{
    FString ClassName;
    if (!Params->TryGetStringField(TEXT("class_name"), ClassName))
        return FPirateMCPCommonUtils::CreateErrorResponse(TEXT("Missing class_name"));

    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!World) return FPirateMCPCommonUtils::CreateErrorResponse(TEXT("No editor world"));

    // Find the class by name
    UClass* ActorClass = nullptr;
    for (TObjectIterator<UClass> It; It; ++It)
    {
        if (It->GetName() == ClassName || It->GetName() == ClassName + TEXT("_C"))
        {
            if (It->IsChildOf(AActor::StaticClass()))
            {
                ActorClass = *It;
                break;
            }
        }
    }
    if (!ActorClass) ActorClass = AActor::StaticClass();

    TArray<AActor*> AllActors;
    UGameplayStatics::GetAllActorsOfClass(World, ActorClass, AllActors);

    TArray<TSharedPtr<FJsonValue>> ActorArray;
    for (AActor* Actor : AllActors)
    {
        if (Actor) ActorArray.Add(FPirateMCPCommonUtils::ActorToJson(Actor));
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetArrayField(TEXT("actors"), ActorArray);
    return Result;
}

TSharedPtr<FJsonObject> FPirateMCPEditorCommands::HandleActorSetCollisionProfile(const TSharedPtr<FJsonObject>& Params)
{
    FString ActorName, ProfileName;
    if (!Params->TryGetStringField(TEXT("name"), ActorName))
        return FPirateMCPCommonUtils::CreateErrorResponse(TEXT("Missing name"));
    if (!Params->TryGetStringField(TEXT("profile_name"), ProfileName))
        return FPirateMCPCommonUtils::CreateErrorResponse(TEXT("Missing profile_name"));

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
            Comp->SetCollisionProfileName(FName(*ProfileName));

        TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
        Result->SetBoolField(TEXT("success"), true);
        Result->SetStringField(TEXT("actor"), ActorName);
        Result->SetStringField(TEXT("profile"), ProfileName);
        return Result;
    }
    return FPirateMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Actor not found: %s"), *ActorName));
}

TSharedPtr<FJsonObject> FPirateMCPEditorCommands::HandleActorSetMaterialScalarParam(const TSharedPtr<FJsonObject>& Params)
{
    FString ActorName, ParamName;
    double Value = 0.0;
    if (!Params->TryGetStringField(TEXT("name"), ActorName))
        return FPirateMCPCommonUtils::CreateErrorResponse(TEXT("Missing name"));
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

TSharedPtr<FJsonObject> FPirateMCPEditorCommands::HandleActorAddNiagaraComponent(const TSharedPtr<FJsonObject>& Params)
{
    FString ActorName, AssetPath;
    if (!Params->TryGetStringField(TEXT("name"), ActorName))
        return FPirateMCPCommonUtils::CreateErrorResponse(TEXT("Missing name"));
    if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath))
        return FPirateMCPCommonUtils::CreateErrorResponse(TEXT("Missing asset_path"));

    UNiagaraSystem* NS = Cast<UNiagaraSystem>(UEditorAssetLibrary::LoadAsset(AssetPath));
    if (!NS)
        return FPirateMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Could not load Niagara system: %s"), *AssetPath));

    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!World) return FPirateMCPCommonUtils::CreateErrorResponse(TEXT("No editor world"));

    TArray<AActor*> Actors;
    UGameplayStatics::GetAllActorsOfClass(World, AActor::StaticClass(), Actors);
    for (AActor* Actor : Actors)
    {
        if (!Actor || Actor->GetName() != ActorName) continue;
        UNiagaraComponent* Comp = NewObject<UNiagaraComponent>(Actor);
        Comp->SetAsset(NS);
        Comp->RegisterComponent();
        Comp->AttachToComponent(Actor->GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);

        TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
        Result->SetBoolField(TEXT("success"), true);
        Result->SetStringField(TEXT("actor"), ActorName);
        Result->SetStringField(TEXT("asset_path"), AssetPath);
        return Result;
    }
    return FPirateMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Actor not found: %s"), *ActorName));
}

// ── Property Access ───────────────────────────────────────────────────────────

TSharedPtr<FJsonObject> FPirateMCPEditorCommands::HandleActorGetProperties(const TSharedPtr<FJsonObject>& Params)
{
    FString ActorName;
    if (!Params->TryGetStringField(TEXT("name"), ActorName))
    {
        return FPirateMCPCommonUtils::CreateErrorResponse(TEXT("Missing required 'name' parameter"));
    }

    UWorld* World = GEditor->GetEditorWorldContext().World();
    if (!World) return FPirateMCPCommonUtils::CreateErrorResponse(TEXT("No world"));

    for (TActorIterator<AActor> It(World); It; ++It)
    {
        if (It->GetName() == ActorName || It->GetActorLabel() == ActorName)
        {
            AActor* Actor = *It;
            TSharedPtr<FJsonObject> PropsObj = MakeShareable(new FJsonObject);

            for (TFieldIterator<FProperty> PropIt(Actor->GetClass()); PropIt; ++PropIt)
            {
                FProperty* Prop = *PropIt;
                if (!Prop->HasAnyPropertyFlags(CPF_Edit | CPF_BlueprintVisible))
                {
                    continue;
                }

                FString ValueStr;
                const void* ValuePtr = Prop->ContainerPtrToValuePtr<void>(Actor);
                Prop->ExportTextItem_Direct(ValueStr, ValuePtr, nullptr, Actor, PPF_None);

                TSharedPtr<FJsonObject> PropInfo = MakeShareable(new FJsonObject);
                PropInfo->SetStringField(TEXT("type"), Prop->GetCPPType());
                PropInfo->SetStringField(TEXT("value"), ValueStr);
                PropInfo->SetStringField(TEXT("category"), Prop->GetMetaData(TEXT("Category")));
                PropsObj->SetObjectField(Prop->GetName(), PropInfo);
            }

            TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
            Result->SetBoolField(TEXT("success"), true);
            Result->SetStringField(TEXT("actor"), ActorName);
            Result->SetStringField(TEXT("class"), Actor->GetClass()->GetName());
            Result->SetObjectField(TEXT("properties"), PropsObj);
            return Result;
        }
    }
    return FPirateMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Actor not found: %s"), *ActorName));
}

TSharedPtr<FJsonObject> FPirateMCPEditorCommands::HandleActorSetProperty(const TSharedPtr<FJsonObject>& Params)
{
    FString ActorName, PropName, ValueStr;
    if (!Params->TryGetStringField(TEXT("name"), ActorName))
        return FPirateMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'name'"));
    if (!Params->TryGetStringField(TEXT("property_name"), PropName))
        return FPirateMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'property_name'"));
    if (!Params->TryGetStringField(TEXT("value"), ValueStr))
        return FPirateMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'value'"));

    UWorld* World = GEditor->GetEditorWorldContext().World();
    if (!World) return FPirateMCPCommonUtils::CreateErrorResponse(TEXT("No world"));

    for (TActorIterator<AActor> It(World); It; ++It)
    {
        if (It->GetName() == ActorName || It->GetActorLabel() == ActorName)
        {
            AActor* Actor = *It;
            FProperty* Prop = Actor->GetClass()->FindPropertyByName(FName(*PropName));
            if (!Prop)
            {
                return FPirateMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Property '%s' not found on %s"), *PropName, *ActorName));
            }

            FScopedTransaction Transaction(NSLOCTEXT("PirateMCP", "SetProperty", "PirateMCP: Set Property"));
            Actor->Modify();

            void* ValuePtr = Prop->ContainerPtrToValuePtr<void>(Actor);
            const TCHAR* ImportResult = Prop->ImportText_Direct(*ValueStr, ValuePtr, Actor, PPF_None);

            if (!ImportResult)
            {
                return FPirateMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Failed to set property '%s' to '%s'"), *PropName, *ValueStr));
            }

            Actor->PostEditChange();

            TSharedPtr<FJsonObject> ResultObj = MakeShareable(new FJsonObject);
            ResultObj->SetBoolField(TEXT("success"), true);
            ResultObj->SetStringField(TEXT("actor"), ActorName);
            ResultObj->SetStringField(TEXT("property"), PropName);
            ResultObj->SetStringField(TEXT("value"), ValueStr);
            return ResultObj;
        }
    }
    return FPirateMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Actor not found: %s"), *ActorName));
}

TSharedPtr<FJsonObject> FPirateMCPEditorCommands::HandleActorGetComponents(const TSharedPtr<FJsonObject>& Params)
{
    FString ActorName;
    if (!Params->TryGetStringField(TEXT("name"), ActorName))
    {
        return FPirateMCPCommonUtils::CreateErrorResponse(TEXT("Missing required 'name' parameter"));
    }

    UWorld* World = GEditor->GetEditorWorldContext().World();
    if (!World) return FPirateMCPCommonUtils::CreateErrorResponse(TEXT("No world"));

    for (TActorIterator<AActor> It(World); It; ++It)
    {
        if (It->GetName() == ActorName || It->GetActorLabel() == ActorName)
        {
            AActor* Actor = *It;
            TArray<TSharedPtr<FJsonValue>> CompArray;

            TArray<UActorComponent*> Components;
            Actor->GetComponents(Components);

            for (UActorComponent* Comp : Components)
            {
                TSharedPtr<FJsonObject> CompObj = MakeShareable(new FJsonObject);
                CompObj->SetStringField(TEXT("name"), Comp->GetName());
                CompObj->SetStringField(TEXT("class"), Comp->GetClass()->GetName());

                USceneComponent* SceneComp = Cast<USceneComponent>(Comp);
                if (SceneComp)
                {
                    FVector RelLoc = SceneComp->GetRelativeLocation();
                    TArray<TSharedPtr<FJsonValue>> LocArr;
                    LocArr.Add(MakeShareable(new FJsonValueNumber(RelLoc.X)));
                    LocArr.Add(MakeShareable(new FJsonValueNumber(RelLoc.Y)));
                    LocArr.Add(MakeShareable(new FJsonValueNumber(RelLoc.Z)));
                    CompObj->SetArrayField(TEXT("relative_location"), LocArr);

                    if (SceneComp->GetAttachParent())
                    {
                        CompObj->SetStringField(TEXT("attached_to"), SceneComp->GetAttachParent()->GetName());
                    }
                }

                CompArray.Add(MakeShareable(new FJsonValueObject(CompObj)));
            }

            TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
            Result->SetBoolField(TEXT("success"), true);
            Result->SetStringField(TEXT("actor"), ActorName);
            Result->SetNumberField(TEXT("count"), CompArray.Num());
            Result->SetArrayField(TEXT("components"), CompArray);
            return Result;
        }
    }
    return FPirateMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Actor not found: %s"), *ActorName));
}

TSharedPtr<FJsonObject> FPirateMCPEditorCommands::HandleActorAddComponent(const TSharedPtr<FJsonObject>& Params)
{
    FString ActorName, ClassName;
    if (!Params->TryGetStringField(TEXT("name"), ActorName))
        return FPirateMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'name'"));
    if (!Params->TryGetStringField(TEXT("component_class"), ClassName))
        return FPirateMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'component_class' (e.g. StaticMeshComponent, PointLightComponent)"));

    UWorld* World = GEditor->GetEditorWorldContext().World();
    if (!World) return FPirateMCPCommonUtils::CreateErrorResponse(TEXT("No world"));

    // Find the component class
    UClass* CompClass = nullptr;
    for (TObjectIterator<UClass> ClassIt; ClassIt; ++ClassIt)
    {
        if (ClassIt->GetName() == ClassName && ClassIt->IsChildOf(UActorComponent::StaticClass()))
        {
            CompClass = *ClassIt;
            break;
        }
    }

    if (!CompClass)
    {
        return FPirateMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Component class not found: %s"), *ClassName));
    }

    for (TActorIterator<AActor> It(World); It; ++It)
    {
        if (It->GetName() == ActorName || It->GetActorLabel() == ActorName)
        {
            AActor* Actor = *It;

            FScopedTransaction Transaction(NSLOCTEXT("PirateMCP", "AddComponent", "PirateMCP: Add Component"));
            Actor->Modify();

            UActorComponent* NewComp = NewObject<UActorComponent>(Actor, CompClass);
            if (!NewComp)
            {
                return FPirateMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create component"));
            }

            NewComp->RegisterComponent();

            USceneComponent* SceneComp = Cast<USceneComponent>(NewComp);
            if (SceneComp && Actor->GetRootComponent())
            {
                SceneComp->AttachToComponent(Actor->GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);
            }

            TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
            Result->SetBoolField(TEXT("success"), true);
            Result->SetStringField(TEXT("actor"), ActorName);
            Result->SetStringField(TEXT("component_name"), NewComp->GetName());
            Result->SetStringField(TEXT("component_class"), CompClass->GetName());
            return Result;
        }
    }
    return FPirateMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Actor not found: %s"), *ActorName));
}
