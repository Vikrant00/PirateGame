#pragma once

#include "CoreMinimal.h"
#include "EditorSubsystem.h"
#include "Sockets.h"
#include "Json.h"
#include "Commands/Editor/PirateMCPEditorCommands.h"
#include "Commands/Blueprint/PirateMCPBlueprintCommands.h"
#include "Commands/Blueprint/PirateMCPBlueprintGraphCommands.h"
#include "Commands/Ocean/PirateMCPOceanCommands.h"
#include "Commands/DataTable/PirateMCPDataTableCommands.h"
#include "Commands/World/PirateMCPWorldCommands.h"
#include "PirateMCPBridge.generated.h"

class FPirateMCPServerRunnable;

UCLASS()
class PIRATEMCP_API UPirateMCPBridge : public UEditorSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	void StartServer();
	void StopServer();
	bool IsRunning() const { return bIsRunning; }

	FString ExecuteCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params);

private:
	bool bIsRunning = false;
	uint16 Port = 55558;
	FString Host = TEXT("127.0.0.1");

	TSharedPtr<FSocket> ListenerSocket;
	FRunnableThread* ServerThread = nullptr;

	TSharedPtr<FPirateMCPEditorCommands> EditorCommands;
	TSharedPtr<FPirateMCPBlueprintCommands> BlueprintCommands;
	TSharedPtr<FPirateMCPBlueprintGraphCommands> BlueprintGraphCommands;
	TSharedPtr<FPirateMCPOceanCommands> OceanCommands;
	TSharedPtr<FPirateMCPDataTableCommands> DataTableCommands;
	TSharedPtr<FPirateMCPWorldCommands> WorldCommands;

	void LoadConfig();
};
