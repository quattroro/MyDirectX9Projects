#pragma once

#include "CoreMinimal.h"

// Manages the shared instance registry file at %APPDATA%\UnrealCliBridge\instances.json.
// Each Editor process writes its entry on startup and removes it on shutdown.
class FInstanceRegistry
{
public:
	static FInstanceRegistry& Get();

	void RegisterSelf(const FString& ProjectRoot, const FString& ProjectHash, const FString& PipeName);
	void UnregisterSelf(const FString& ProjectHash);

private:
	FString GetRegistryPath() const;
	void UpdateRegistry(TFunction<void(TArray<TSharedPtr<FJsonObject>>&)> Mutator);
};
