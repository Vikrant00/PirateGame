#include "PirateMCPModule.h"
#include "Modules/ModuleManager.h"

#define LOCTEXT_NAMESPACE "FPirateMCPModule"

DEFINE_LOG_CATEGORY(LogPirateMCP);

void FPirateMCPModule::StartupModule()
{
	UE_LOG(LogPirateMCP, Display, TEXT("PirateMCP Module started"));
}

void FPirateMCPModule::ShutdownModule()
{
	UE_LOG(LogPirateMCP, Display, TEXT("PirateMCP Module shut down"));
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FPirateMCPModule, PirateMCP)
