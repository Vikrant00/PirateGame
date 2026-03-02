#include "PirateMCPBridge.h"
#include "PirateMCPServerRunnable.h"
#include "PirateMCPModule.h"
#include "Sockets.h"
#include "SocketSubsystem.h"
#include "HAL/RunnableThread.h"
#include "Interfaces/IPv4/IPv4Address.h"
#include "Interfaces/IPv4/IPv4Endpoint.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Async/Async.h"
#include "Misc/ConfigCacheIni.h"

void UPirateMCPBridge::LoadConfig()
{
	// Read port and host from Config/DefaultPirateMCP.ini
	int32 ConfigPort = 55558;
	FString ConfigHost = TEXT("127.0.0.1");

	GConfig->GetInt(
		TEXT("/Script/PirateMCP.PirateMCPSettings"),
		TEXT("Port"),
		ConfigPort,
		GGameIni
	);
	GConfig->GetString(
		TEXT("/Script/PirateMCP.PirateMCPSettings"),
		TEXT("Host"),
		ConfigHost,
		GGameIni
	);

	Port = (uint16)FMath::Clamp(ConfigPort, 1024, 65535);
	Host = ConfigHost;

	UE_LOG(LogPirateMCP, Display, TEXT("PirateMCP config loaded — Host: %s  Port: %d"), *Host, Port);
}

void UPirateMCPBridge::Initialize(FSubsystemCollectionBase& Collection)
{
	EditorCommands       = MakeShared<FPirateMCPEditorCommands>();
	BlueprintCommands    = MakeShared<FPirateMCPBlueprintCommands>();
	BlueprintGraphCommands = MakeShared<FPirateMCPBlueprintGraphCommands>();
	OceanCommands        = MakeShared<FPirateMCPOceanCommands>();
	DataTableCommands    = MakeShared<FPirateMCPDataTableCommands>();
	WorldCommands        = MakeShared<FPirateMCPWorldCommands>();

	LoadConfig();
	StartServer();
}

void UPirateMCPBridge::Deinitialize()
{
	StopServer();
}

void UPirateMCPBridge::StartServer()
{
	if (bIsRunning) return;

	ISocketSubsystem* SS = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
	if (!SS)
	{
		UE_LOG(LogPirateMCP, Error, TEXT("Failed to get socket subsystem"));
		return;
	}

	TSharedPtr<FSocket> NewSocket = MakeShareable(SS->CreateSocket(NAME_Stream, TEXT("PirateMCPListener"), false));
	if (!NewSocket.IsValid())
	{
		UE_LOG(LogPirateMCP, Error, TEXT("Failed to create listener socket"));
		return;
	}

	NewSocket->SetReuseAddr(true);
	NewSocket->SetNonBlocking(true);

	FIPv4Address Addr;
	FIPv4Address::Parse(Host, Addr);
	FIPv4Endpoint Endpoint(Addr, Port);

	if (!NewSocket->Bind(*Endpoint.ToInternetAddr()))
	{
		UE_LOG(LogPirateMCP, Error, TEXT("Failed to bind to %s:%d — is port already in use?"), *Host, Port);
		SS->DestroySocket(NewSocket.Get());
		return;
	}

	if (!NewSocket->Listen(5))
	{
		UE_LOG(LogPirateMCP, Error, TEXT("Failed to start listening"));
		SS->DestroySocket(NewSocket.Get());
		return;
	}

	ListenerSocket = NewSocket;
	bIsRunning = true;

	UE_LOG(LogPirateMCP, Display, TEXT("PirateMCP server listening on %s:%d"), *Host, Port);

	ServerThread = FRunnableThread::Create(
		new FPirateMCPServerRunnable(this, ListenerSocket),
		TEXT("PirateMCPServerThread"),
		0, TPri_Normal
	);

	if (!ServerThread)
	{
		UE_LOG(LogPirateMCP, Error, TEXT("Failed to create server thread"));
		StopServer();
	}
}

void UPirateMCPBridge::StopServer()
{
	if (!bIsRunning) return;
	bIsRunning = false;

	if (ServerThread)
	{
		ServerThread->Kill(true);
		delete ServerThread;
		ServerThread = nullptr;
	}

	if (ListenerSocket.IsValid())
	{
		ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(ListenerSocket.Get());
		ListenerSocket.Reset();
	}

	UE_LOG(LogPirateMCP, Display, TEXT("PirateMCP server stopped"));
}

FString UPirateMCPBridge::ExecuteCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params)
{
	TPromise<FString> Promise;
	TFuture<FString> Future = Promise.GetFuture();

	AsyncTask(ENamedThreads::GameThread, [this, CommandType, Params, Promise = MoveTemp(Promise)]() mutable
	{
		TSharedPtr<FJsonObject> ResponseJson = MakeShareable(new FJsonObject);

		try
		{
			TSharedPtr<FJsonObject> ResultJson;

			// ── Ping ────────────────────────────────────────────────
			if (CommandType == TEXT("ping"))
			{
				ResultJson = MakeShareable(new FJsonObject);
				ResultJson->SetStringField(TEXT("message"), TEXT("pong"));
				ResultJson->SetStringField(TEXT("api_version"), TEXT("1.0"));
				// List available command categories
				TArray<TSharedPtr<FJsonValue>> Categories;
				for (const FString& Cat : TArray<FString>{
					TEXT("editor"), TEXT("blueprint"), TEXT("blueprint_graph"),
					TEXT("ocean"), TEXT("datatable"), TEXT("world") })
				{
					Categories.Add(MakeShareable(new FJsonValueString(Cat)));
				}
				ResultJson->SetArrayField(TEXT("command_categories"), Categories);
			}
			// ── Editor ──────────────────────────────────────────────
			else if (CommandType.StartsWith(TEXT("actor_")) ||
			         CommandType == TEXT("get_actors_in_level") ||
			         CommandType == TEXT("find_actors_by_name") ||
			         CommandType == TEXT("spawn_actor") ||
			         CommandType == TEXT("delete_actor") ||
			         CommandType == TEXT("set_actor_transform") ||
			         CommandType == TEXT("spawn_blueprint_actor"))
			{
				ResultJson = EditorCommands->HandleCommand(CommandType, Params);
			}
			// ── Blueprint ────────────────────────────────────────────
			else if (CommandType == TEXT("create_blueprint") ||
			         CommandType == TEXT("add_component_to_blueprint") ||
			         CommandType == TEXT("set_physics_properties") ||
			         CommandType == TEXT("compile_blueprint") ||
			         CommandType == TEXT("set_static_mesh_properties") ||
			         CommandType == TEXT("set_mesh_material_color") ||
			         CommandType == TEXT("get_available_materials") ||
			         CommandType == TEXT("apply_material_to_actor") ||
			         CommandType == TEXT("apply_material_to_blueprint") ||
			         CommandType == TEXT("get_actor_material_info") ||
			         CommandType == TEXT("get_blueprint_material_info") ||
			         CommandType == TEXT("read_blueprint_content") ||
			         CommandType == TEXT("analyze_blueprint_graph") ||
			         CommandType == TEXT("get_blueprint_variable_details") ||
			         CommandType == TEXT("get_blueprint_function_details"))
			{
				ResultJson = BlueprintCommands->HandleCommand(CommandType, Params);
			}
			// ── Blueprint Graph ──────────────────────────────────────
			else if (CommandType == TEXT("add_blueprint_node") ||
			         CommandType == TEXT("connect_nodes") ||
			         CommandType == TEXT("create_variable") ||
			         CommandType == TEXT("set_blueprint_variable_properties") ||
			         CommandType == TEXT("add_event_node") ||
			         CommandType == TEXT("delete_node") ||
			         CommandType == TEXT("set_node_property") ||
			         CommandType == TEXT("create_function") ||
			         CommandType == TEXT("add_function_input") ||
			         CommandType == TEXT("add_function_output") ||
			         CommandType == TEXT("delete_function") ||
			         CommandType == TEXT("rename_function"))
			{
				ResultJson = BlueprintGraphCommands->HandleCommand(CommandType, Params);
			}
			// ── Ocean ────────────────────────────────────────────────
			else if (CommandType.StartsWith(TEXT("ocean_")))
			{
				ResultJson = OceanCommands->HandleCommand(CommandType, Params);
			}
			// ── DataTable ────────────────────────────────────────────
			else if (CommandType.StartsWith(TEXT("datatable_")))
			{
				ResultJson = DataTableCommands->HandleCommand(CommandType, Params);
			}
			// ── World ────────────────────────────────────────────────
			else if (CommandType.StartsWith(TEXT("world_")))
			{
				ResultJson = WorldCommands->HandleCommand(CommandType, Params);
			}
			else
			{
				ResultJson = MakeShareable(new FJsonObject);
				ResultJson->SetBoolField(TEXT("success"), false);
				ResultJson->SetStringField(TEXT("error"), FString::Printf(TEXT("Unknown command: %s"), *CommandType));
			}

			// Wrap result
			bool bSuccess = true;
			FString ErrorMsg;
			if (ResultJson->HasField(TEXT("success")))
			{
				bSuccess = ResultJson->GetBoolField(TEXT("success"));
				if (!bSuccess) ResultJson->TryGetStringField(TEXT("error"), ErrorMsg);
			}

			if (bSuccess)
			{
				ResponseJson->SetStringField(TEXT("status"), TEXT("success"));
				ResponseJson->SetObjectField(TEXT("result"), ResultJson);
			}
			else
			{
				ResponseJson->SetStringField(TEXT("status"), TEXT("error"));
				ResponseJson->SetStringField(TEXT("error"), ErrorMsg);
			}
		}
		catch (const std::exception& e)
		{
			ResponseJson->SetStringField(TEXT("status"), TEXT("error"));
			ResponseJson->SetStringField(TEXT("error"), UTF8_TO_TCHAR(e.what()));
		}

		FString Out;
		TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Out);
		FJsonSerializer::Serialize(ResponseJson.ToSharedRef(), Writer);
		Promise.SetValue(Out);
	});

	return Future.Get();
}
