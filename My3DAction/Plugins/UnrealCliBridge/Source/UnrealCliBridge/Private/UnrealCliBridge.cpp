#include "UnrealCliBridge.h"
#include "BridgeHost.h"

#define LOCTEXT_NAMESPACE "FUnrealCliBridgeModule"

void FUnrealCliBridgeModule::StartupModule()
{
	FBridgeHost::Get().Initialize();
}

void FUnrealCliBridgeModule::ShutdownModule()
{
	FBridgeHost::Get().Shutdown();
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FUnrealCliBridgeModule, UnrealCliBridge)
