#include "BridgeHost.h"
#include "IpcServer.h"
#include "InstanceRegistry.h"
#include "CommandDispatcher.h"
#include "Handlers/EditorCommandHandler.h"
#include "Handlers/AssetCommandHandler.h"
#include "Handlers/LevelCommandHandler.h"
#include "Handlers/BlueprintCommandHandler.h"
#include "Handlers/ExecuteCommandHandler.h"
#include "Handlers/PluginCommandHandler.h"
#include "Misc/Paths.h"
#include "Misc/SecureHash.h"

DEFINE_LOG_CATEGORY_STATIC(LogUnrealCliBridge, Log, All);

FBridgeHost& FBridgeHost::Get()
{
	static FBridgeHost Instance;
	return Instance;
}

void FBridgeHost::Initialize()
{
	if (bRunning) return;

	// Determine project root
	ProjectRoot = FPaths::ConvertRelativePathToFull(FPaths::GetPath(FPaths::GetProjectFilePath()));
	FPaths::NormalizeDirectoryName(ProjectRoot);

	// Compute 12-char SHA256 hash of the lowercase project root for pipe name uniqueness
	ProjectHash = ComputeProjectHash(ProjectRoot);
	PipeName = FString::Printf(TEXT("unreal-cli-%s"), *ProjectHash);

	UE_LOG(LogUnrealCliBridge, Log, TEXT("UnrealCliBridge: Starting. ProjectRoot=%s PipeName=%s"), *ProjectRoot, *PipeName);

	// Register command handlers
	RegisterHandlers();

	// Start Named Pipe server
	FCommandDispatcher& Dispatcher = FCommandDispatcher::Get();
	IpcServer = MakeUnique<FIpcServer>(PipeName, [&Dispatcher](const FString& Request)
	{
		return Dispatcher.Dispatch(Request);
	});

	if (!IpcServer->Start())
	{
		UE_LOG(LogUnrealCliBridge, Error, TEXT("UnrealCliBridge: Failed to start IPC server."));
		return;
	}

	// Write to instance registry
	FInstanceRegistry::Get().RegisterSelf(ProjectRoot, ProjectHash, PipeName);

	bRunning = true;
	UE_LOG(LogUnrealCliBridge, Log, TEXT("UnrealCliBridge: Ready on pipe: %s"), *PipeName);
}

void FBridgeHost::Shutdown()
{
	if (!bRunning) return;

	UE_LOG(LogUnrealCliBridge, Log, TEXT("UnrealCliBridge: Shutting down."));

	if (IpcServer)
	{
		IpcServer->Stop();
		IpcServer.Reset();
	}

	FInstanceRegistry::Get().UnregisterSelf(ProjectHash);
	bRunning = false;
}

void FBridgeHost::RegisterHandlers()
{
	FCommandDispatcher& D = FCommandDispatcher::Get();

	// Editor control
	FEditorCommandHandler::RegisterAll(D);

	// Asset workflows
	FAssetCommandHandler::RegisterAll(D);

	// Level workflows
	FLevelCommandHandler::RegisterAll(D);

	// Blueprint workflows
	FBlueprintCommandHandler::RegisterAll(D);

	// Execute (Python)
	FExecuteCommandHandler::RegisterAll(D);

	// Plugin management
	FPluginCommandHandler::RegisterAll(D);
}

FString FBridgeHost::ComputeProjectHash(const FString& InProjectRoot) const
{
	FString Normalized = InProjectRoot.ToLower().Replace(TEXT("\\"), TEXT("/"));
	FSHAHash Hash;
	FSHA1::HashBuffer(TCHAR_TO_UTF8(*Normalized), FCStringAnsi::Strlen(TCHAR_TO_UTF8(*Normalized)), Hash.Hash);
	FString HexHash = Hash.ToString().ToLower();
	return HexHash.Left(12);
}
