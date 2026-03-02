#include "PirateMCPServerRunnable.h"
#include "PirateMCPBridge.h"
#include "PirateMCPModule.h"
#include "Sockets.h"
#include "SocketSubsystem.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonReader.h"
#include "HAL/PlatformProcess.h"

FPirateMCPServerRunnable::FPirateMCPServerRunnable(UPirateMCPBridge* InBridge, TSharedPtr<FSocket> InListenerSocket)
	: Bridge(InBridge)
	, ListenerSocket(InListenerSocket)
	, bRunning(true)
{
}

FPirateMCPServerRunnable::~FPirateMCPServerRunnable() {}

bool FPirateMCPServerRunnable::Init() { return true; }

uint32 FPirateMCPServerRunnable::Run()
{
	UE_LOG(LogPirateMCP, Display, TEXT("Server thread started"));

	while (bRunning)
	{
		bool bPending = false;
		if (ListenerSocket->HasPendingConnection(bPending) && bPending)
		{
			TSharedPtr<FSocket> ClientSocket = MakeShareable(ListenerSocket->Accept(TEXT("PirateMCPClient")));
			if (ClientSocket.IsValid())
			{
				UE_LOG(LogPirateMCP, Display, TEXT("Client connected"));
				ClientSocket->SetNoDelay(true);
				int32 ActualSendSize = 0;
				int32 ActualRecvSize = 0;
				ClientSocket->SetSendBufferSize(65536, ActualSendSize);
				ClientSocket->SetReceiveBufferSize(65536, ActualRecvSize);
				// Handle session — returns when client disconnects
				HandleClientConnection(ClientSocket);
				// FIX vs original: close the client socket cleanly, then loop back
				// to listen for the next client instead of killing the thread
				ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(ClientSocket.Get());
				UE_LOG(LogPirateMCP, Display, TEXT("Client disconnected — waiting for next connection"));
			}
		}
		FPlatformProcess::Sleep(0.05f);
	}

	UE_LOG(LogPirateMCP, Display, TEXT("Server thread stopped"));
	return 0;
}

void FPirateMCPServerRunnable::Stop()  { bRunning = false; }
void FPirateMCPServerRunnable::Exit()  {}

void FPirateMCPServerRunnable::HandleClientConnection(TSharedPtr<FSocket> ClientSocket)
{
	FString MessageBuffer;
	uint8 Buffer[8192];

	while (bRunning && ClientSocket.IsValid())
	{
		int32 BytesRead = 0;
		if (ClientSocket->Recv(Buffer, sizeof(Buffer) - 1, BytesRead))
		{
			if (BytesRead == 0) break; // graceful close

			Buffer[BytesRead] = '\0';
			MessageBuffer += UTF8_TO_TCHAR(Buffer);

			// Process all complete newline-delimited messages
			int32 NewlineIdx;
			while (MessageBuffer.FindChar('\n', NewlineIdx))
			{
				FString Message = MessageBuffer.Left(NewlineIdx).TrimStartAndEnd();
				MessageBuffer = MessageBuffer.Mid(NewlineIdx + 1);
				if (!Message.IsEmpty())
				{
					ProcessMessage(ClientSocket, Message);
				}
			}
		}
		else
		{
			int32 Err = (int32)ISocketSubsystem::Get()->GetLastErrorCode();
			if (Err == SE_EWOULDBLOCK)
			{
				FPlatformProcess::Sleep(0.01f);
				continue;
			}
			if (Err == SE_EINTR) continue;
			break; // real error or disconnect
		}
	}
}

void FPirateMCPServerRunnable::ProcessMessage(TSharedPtr<FSocket> Client, const FString& Message)
{
	TSharedPtr<FJsonObject> JsonMsg;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Message);
	if (!FJsonSerializer::Deserialize(Reader, JsonMsg) || !JsonMsg.IsValid())
	{
		UE_LOG(LogPirateMCP, Warning, TEXT("Failed to parse JSON: %s"), *Message.Left(200));
		return;
	}

	FString CommandType;
	if (!JsonMsg->TryGetStringField(TEXT("command"), CommandType))
	{
		// Also support "type" field for backwards compatibility with UnrealMCP clients
		if (!JsonMsg->TryGetStringField(TEXT("type"), CommandType))
		{
			UE_LOG(LogPirateMCP, Warning, TEXT("Message missing 'command' field"));
			return;
		}
	}

	TSharedPtr<FJsonObject> Params = MakeShareable(new FJsonObject());
	if (JsonMsg->HasField(TEXT("params")))
	{
		TSharedPtr<FJsonValue> ParamsValue = JsonMsg->TryGetField(TEXT("params"));
		if (ParamsValue.IsValid() && ParamsValue->Type == EJson::Object)
		{
			Params = ParamsValue->AsObject();
		}
	}

	FString Response = Bridge->ExecuteCommand(CommandType, Params);
	Response += TEXT("\n");
	SendResponse(Client, Response);
}

bool FPirateMCPServerRunnable::SendResponse(TSharedPtr<FSocket> Client, const FString& Response)
{
	FTCHARToUTF8 UTF8(*Response);
	const uint8* Data = (const uint8*)UTF8.Get();
	int32 Total = UTF8.Length();
	int32 Sent = 0;

	while (Sent < Total)
	{
		int32 BytesSent = 0;
		if (!Client->Send(Data + Sent, Total - Sent, BytesSent))
		{
			UE_LOG(LogPirateMCP, Error, TEXT("Failed to send response after %d/%d bytes"), Sent, Total);
			return false;
		}
		Sent += BytesSent;
	}
	return true;
}
