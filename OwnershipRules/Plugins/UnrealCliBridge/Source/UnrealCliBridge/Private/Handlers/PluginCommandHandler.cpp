#include "PluginCommandHandler.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonWriter.h"

DEFINE_LOG_CATEGORY_STATIC(LogUnrealCliBridgePlugin, Log, All);

void FPluginCommandHandler::RegisterAll(FCommandDispatcher& Dispatcher)
{
	Dispatcher.Register(TEXT("plugin.list"), &HandleList);
	Dispatcher.Register(TEXT("plugin.enable"), &HandleEnable);
	Dispatcher.Register(TEXT("plugin.disable"), &HandleDisable);
}

TSharedPtr<FJsonObject> FPluginCommandHandler::HandleList(const TSharedPtr<FJsonObject>& Args, bool bForce)
{
	TArray<TSharedPtr<FJsonValue>> PluginList;
	IPluginManager& PluginManager = IPluginManager::Get();

	for (const TSharedRef<IPlugin>& Plugin : PluginManager.GetDiscoveredPlugins())
	{
		TSharedPtr<FJsonObject> PluginObj = MakeShared<FJsonObject>();
		PluginObj->SetStringField(TEXT("name"), Plugin->GetName());
		PluginObj->SetStringField(TEXT("version"), Plugin->GetDescriptor().VersionName);
		PluginObj->SetStringField(TEXT("description"), Plugin->GetDescriptor().Description);
		PluginObj->SetBoolField(TEXT("enabled"), Plugin->IsEnabled());
		PluginObj->SetBoolField(TEXT("isProject"), Plugin->GetType() == EPluginType::Project);
		PluginObj->SetBoolField(TEXT("isEngine"), Plugin->GetType() == EPluginType::Engine);
		PluginObj->SetStringField(TEXT("location"), Plugin->GetBaseDir());
		PluginList.Add(MakeShared<FJsonValueObject>(PluginObj));
	}

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetArrayField(TEXT("plugins"), PluginList);
	Data->SetNumberField(TEXT("count"), PluginList.Num());
	return Data;
}

static bool SetPluginEnabledInDescriptor(const FString& PluginName, bool bEnable)
{
	// Modify the .uproject file to enable/disable the plugin
	FString ProjectFilePath = FPaths::GetProjectFilePath();
	FString ProjectJson;
	if (!FFileHelper::LoadFileToString(ProjectJson, *ProjectFilePath))
		return false;

	TSharedPtr<FJsonObject> ProjectRoot;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ProjectJson);
	if (!FJsonSerializer::Deserialize(Reader, ProjectRoot) || !ProjectRoot.IsValid())
		return false;

	const TArray<TSharedPtr<FJsonValue>>* PluginsArray;
	bool bFound = false;

	if (ProjectRoot->TryGetArrayField(TEXT("Plugins"), PluginsArray))
	{
		TArray<TSharedPtr<FJsonValue>>& Plugins = const_cast<TArray<TSharedPtr<FJsonValue>>&>(*PluginsArray);
		for (TSharedPtr<FJsonValue>& PluginVal : Plugins)
		{
			TSharedPtr<FJsonObject> PluginObj = PluginVal->AsObject();
			FString Name;
			if (PluginObj && PluginObj->TryGetStringField(TEXT("Name"), Name) && Name == PluginName)
			{
				PluginObj->SetBoolField(TEXT("Enabled"), bEnable);
				bFound = true;
				break;
			}
		}

		if (!bFound)
		{
			TSharedPtr<FJsonObject> NewEntry = MakeShared<FJsonObject>();
			NewEntry->SetStringField(TEXT("Name"), PluginName);
			NewEntry->SetBoolField(TEXT("Enabled"), bEnable);
			Plugins.Add(MakeShared<FJsonValueObject>(NewEntry));
		}
	}
	else
	{
		// No Plugins array yet — create it
		TArray<TSharedPtr<FJsonValue>> NewPlugins;
		TSharedPtr<FJsonObject> NewEntry = MakeShared<FJsonObject>();
		NewEntry->SetStringField(TEXT("Name"), PluginName);
		NewEntry->SetBoolField(TEXT("Enabled"), bEnable);
		NewPlugins.Add(MakeShared<FJsonValueObject>(NewEntry));
		ProjectRoot->SetArrayField(TEXT("Plugins"), NewPlugins);
	}

	FString OutputJson;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputJson);
	FJsonSerializer::Serialize(ProjectRoot.ToSharedRef(), Writer);

	return FFileHelper::SaveStringToFile(OutputJson, *ProjectFilePath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
}

TSharedPtr<FJsonObject> FPluginCommandHandler::HandleEnable(const TSharedPtr<FJsonObject>& Args, bool bForce)
{
	FString PluginName;
	if (!Args.IsValid() || !Args->TryGetStringField(TEXT("name"), PluginName) || PluginName.IsEmpty())
		throw FCommandFailedException(TEXT("CLI_USAGE"), TEXT("--name is required."));

	TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(PluginName);
	if (!Plugin.IsValid())
		throw FCommandFailedException(TEXT("NOT_FOUND"), FString::Printf(TEXT("Plugin not found: %s"), *PluginName));

	if (Plugin->IsEnabled())
	{
		TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
		Data->SetStringField(TEXT("name"), PluginName);
		Data->SetStringField(TEXT("message"), TEXT("Plugin is already enabled."));
		return Data;
	}

	if (!SetPluginEnabledInDescriptor(PluginName, true))
		throw FCommandFailedException(TEXT("OPERATION_FAILED"), TEXT("Failed to update .uproject file."));

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("name"), PluginName);
	Data->SetStringField(TEXT("message"), TEXT("Plugin enabled in .uproject. Restart the editor to apply."));
	return Data;
}

TSharedPtr<FJsonObject> FPluginCommandHandler::HandleDisable(const TSharedPtr<FJsonObject>& Args, bool bForce)
{
	if (!bForce) throw FCommandFailedException(TEXT("FORCE_REQUIRED"), TEXT("plugin disable always requires --force."));

	FString PluginName;
	if (!Args.IsValid() || !Args->TryGetStringField(TEXT("name"), PluginName) || PluginName.IsEmpty())
		throw FCommandFailedException(TEXT("CLI_USAGE"), TEXT("--name is required."));

	TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(PluginName);
	if (!Plugin.IsValid())
		throw FCommandFailedException(TEXT("NOT_FOUND"), FString::Printf(TEXT("Plugin not found: %s"), *PluginName));

	if (!SetPluginEnabledInDescriptor(PluginName, false))
		throw FCommandFailedException(TEXT("OPERATION_FAILED"), TEXT("Failed to update .uproject file."));

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("name"), PluginName);
	Data->SetStringField(TEXT("message"), TEXT("Plugin disabled in .uproject. Restart the editor to apply."));
	return Data;
}
