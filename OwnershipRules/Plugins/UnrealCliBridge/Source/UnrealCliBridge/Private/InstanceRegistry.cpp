#include "InstanceRegistry.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/PlatformProcess.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/EngineVersion.h"

DEFINE_LOG_CATEGORY_STATIC(LogUnrealCliBridgeRegistry, Log, All);

FInstanceRegistry& FInstanceRegistry::Get()
{
	static FInstanceRegistry Instance;
	return Instance;
}

FString FInstanceRegistry::GetRegistryPath() const
{
	FString AppData = FPlatformMisc::GetEnvironmentVariable(TEXT("APPDATA"));
	if (AppData.IsEmpty())
	{
		AppData = FPlatformProcess::UserSettingsDir();
	}
	return FPaths::Combine(AppData, TEXT("UnrealCliBridge"), TEXT("instances.json"));
}

void FInstanceRegistry::RegisterSelf(const FString& ProjectRoot, const FString& ProjectHash, const FString& PipeName)
{
	UpdateRegistry([&](TArray<TSharedPtr<FJsonObject>>& Instances)
	{
		// Remove any stale entry for this hash
		Instances.RemoveAll([&](const TSharedPtr<FJsonObject>& Obj)
		{
			return Obj->GetStringField(TEXT("projectHash")) == ProjectHash;
		});

		TSharedPtr<FJsonObject> Entry = MakeShared<FJsonObject>();
		Entry->SetStringField(TEXT("projectRoot"), ProjectRoot);
		Entry->SetStringField(TEXT("projectHash"), ProjectHash);
		Entry->SetStringField(TEXT("pipeName"), PipeName);
		Entry->SetNumberField(TEXT("pid"), FPlatformProcess::GetCurrentProcessId());

		FString VersionString = FEngineVersion::Current().ToString();
		Entry->SetStringField(TEXT("engineVersion"), VersionString);

		FString ProjectName = FPaths::GetBaseFilename(FPaths::GetProjectFilePath());
		Entry->SetStringField(TEXT("projectName"), ProjectName);

		FDateTime Now = FDateTime::UtcNow();
		Entry->SetStringField(TEXT("startedAt"), Now.ToIso8601());

		Instances.Add(Entry);
	});

	UE_LOG(LogUnrealCliBridgeRegistry, Log, TEXT("Registered instance: %s (pipe: %s)"), *ProjectRoot, *PipeName);
}

void FInstanceRegistry::UnregisterSelf(const FString& ProjectHash)
{
	UpdateRegistry([&](TArray<TSharedPtr<FJsonObject>>& Instances)
	{
		Instances.RemoveAll([&](const TSharedPtr<FJsonObject>& Obj)
		{
			return Obj->GetStringField(TEXT("projectHash")) == ProjectHash;
		});
	});

	UE_LOG(LogUnrealCliBridgeRegistry, Log, TEXT("Unregistered instance: %s"), *ProjectHash);
}

void FInstanceRegistry::UpdateRegistry(TFunction<void(TArray<TSharedPtr<FJsonObject>>&)> Mutator)
{
	FString RegistryPath = GetRegistryPath();
	FString RegistryDir = FPaths::GetPath(RegistryPath);

	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	if (!PlatformFile.DirectoryExists(*RegistryDir))
	{
		PlatformFile.CreateDirectoryTree(*RegistryDir);
	}

	// Load existing registry
	TArray<TSharedPtr<FJsonObject>> Instances;
	FString ExistingJson;
	if (FFileHelper::LoadFileToString(ExistingJson, *RegistryPath))
	{
		TSharedPtr<FJsonObject> Root;
		TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ExistingJson);
		if (FJsonSerializer::Deserialize(Reader, Root) && Root.IsValid())
		{
			const TArray<TSharedPtr<FJsonValue>>* InstancesArray;
			if (Root->TryGetArrayField(TEXT("instances"), InstancesArray))
			{
				for (const TSharedPtr<FJsonValue>& Val : *InstancesArray)
				{
					if (Val->Type == EJson::Object)
					{
						Instances.Add(Val->AsObject());
					}
				}
			}
		}
	}

	// Apply mutation
	Mutator(Instances);

	// Build output JSON
	TSharedPtr<FJsonObject> NewRoot = MakeShared<FJsonObject>();
	TArray<TSharedPtr<FJsonValue>> InstanceValues;
	for (const TSharedPtr<FJsonObject>& Obj : Instances)
	{
		InstanceValues.Add(MakeShared<FJsonValueObject>(Obj));
	}
	NewRoot->SetArrayField(TEXT("instances"), InstanceValues);

	FString OutputJson;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputJson);
	FJsonSerializer::Serialize(NewRoot.ToSharedRef(), Writer);

	if (!FFileHelper::SaveStringToFile(OutputJson, *RegistryPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		UE_LOG(LogUnrealCliBridgeRegistry, Warning, TEXT("Failed to write instance registry: %s"), *RegistryPath);
	}
}
