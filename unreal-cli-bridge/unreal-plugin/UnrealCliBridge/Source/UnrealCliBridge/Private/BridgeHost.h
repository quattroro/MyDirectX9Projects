#pragma once

#include "CoreMinimal.h"

class FIpcServer;

class FBridgeHost
{
public:
	static FBridgeHost& Get();

	void Initialize();
	void Shutdown();

	bool IsRunning() const { return bRunning; }
	const FString& GetProjectRoot() const { return ProjectRoot; }
	const FString& GetProjectHash() const { return ProjectHash; }
	const FString& GetPipeName() const { return PipeName; }

private:
	FBridgeHost() = default;
	~FBridgeHost() = default;

	FString ComputeProjectHash(const FString& InProjectRoot) const;
	void RegisterHandlers();

	bool bRunning = false;
	FString ProjectRoot;
	FString ProjectHash;
	FString PipeName;

	TUniquePtr<FIpcServer> IpcServer;
};
