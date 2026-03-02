#pragma once

#include "CoreMinimal.h"
#include "HAL/Runnable.h"
#include "Sockets.h"

class UPirateMCPBridge;

class FPirateMCPServerRunnable : public FRunnable
{
public:
	FPirateMCPServerRunnable(UPirateMCPBridge* InBridge, TSharedPtr<FSocket> InListenerSocket);
	virtual ~FPirateMCPServerRunnable();

	virtual bool Init() override;
	virtual uint32 Run() override;
	virtual void Stop() override;
	virtual void Exit() override;

private:
	void HandleClientConnection(TSharedPtr<FSocket> ClientSocket);
	void ProcessMessage(TSharedPtr<FSocket> Client, const FString& Message);
	bool SendResponse(TSharedPtr<FSocket> Client, const FString& Response);

	UPirateMCPBridge* Bridge;
	TSharedPtr<FSocket> ListenerSocket;
	bool bRunning;
};
