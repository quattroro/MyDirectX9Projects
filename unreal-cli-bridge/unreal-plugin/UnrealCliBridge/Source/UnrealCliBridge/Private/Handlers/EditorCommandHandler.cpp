#include "EditorCommandHandler.h"
#include "BridgeHost.h"
#include "Editor.h"
#include "Editor/EditorEngine.h"
#include "LevelEditor.h"
#include "Subsystems/UnrealEditorSubsystem.h"
#include "EditorDirectories.h"
#include "Misc/EngineVersion.h"
#include "Misc/Paths.h"
#include "Engine/World.h"
#include "IHotReload.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "ToolMenus.h"
#include "Framework/Application/SlateApplication.h"
#include "Slate/SceneViewport.h"
#include "Widgets/SViewport.h"
#include "Engine/GameViewportClient.h"

DEFINE_LOG_CATEGORY_STATIC(LogUnrealCliBridgeEditor, Log, All);

// Simple ring buffer for log messages captured via output device
class FLogCapture : public FOutputDevice
{
public:
	static FLogCapture& Get()
	{
		static FLogCapture Instance;
		return Instance;
	}

	struct FEntry
	{
		FString Message;
		ELogVerbosity::Type Verbosity;
		FDateTime Time;
	};

	void Serialize(const TCHAR* V, ELogVerbosity::Type Verbosity, const FName& Category) override
	{
		FScopeLock Lock(&CritSection);
		if (Entries.Num() >= MaxEntries)
		{
			Entries.RemoveAt(0);
		}
		Entries.Add({ FString(V), Verbosity, FDateTime::UtcNow() });
	}

	TArray<FEntry> GetEntries(int32 Limit, ELogVerbosity::Type FilterVerbosity)
	{
		FScopeLock Lock(&CritSection);
		TArray<FEntry> Result;
		int32 Start = FMath::Max(0, Entries.Num() - Limit);
		for (int32 i = Start; i < Entries.Num(); ++i)
		{
			if (Entries[i].Verbosity <= FilterVerbosity)
			{
				Result.Add(Entries[i]);
			}
		}
		return Result;
	}

private:
	TArray<FEntry> Entries;
	FCriticalSection CritSection;
	static constexpr int32 MaxEntries = 500;
};

static bool bLogCapureRegistered = false;

void FEditorCommandHandler::RegisterAll(FCommandDispatcher& Dispatcher)
{
	if (!bLogCapureRegistered)
	{
		GLog->AddOutputDevice(&FLogCapture::Get());
		bLogCapureRegistered = true;
	}

	Dispatcher.Register(TEXT("status"), &HandleStatus);
	Dispatcher.Register(TEXT("play"), &HandlePlay);
	Dispatcher.Register(TEXT("pause"), &HandlePause);
	Dispatcher.Register(TEXT("stop"), &HandleStop);
	Dispatcher.Register(TEXT("compile"), &HandleCompile);
	Dispatcher.Register(TEXT("refresh"), &HandleRefresh);
	Dispatcher.Register(TEXT("read-log"), &HandleReadLog);
	Dispatcher.Register(TEXT("screenshot"), &HandleScreenshot);
	Dispatcher.Register(TEXT("execute-menu"), &HandleExecuteMenu);
}

TSharedPtr<FJsonObject> FEditorCommandHandler::HandleStatus(const TSharedPtr<FJsonObject>& Args, bool bForce)
{
	UEditorEngine* EditorEngine = GEditor;
	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();

	Data->SetStringField(TEXT("status"), TEXT("online"));
	Data->SetStringField(TEXT("engineVersion"), FEngineVersion::Current().ToString());
	Data->SetStringField(TEXT("projectName"), FPaths::GetBaseFilename(FPaths::GetProjectFilePath()));
	Data->SetStringField(TEXT("projectRoot"), FBridgeHost::Get().GetProjectRoot());
	Data->SetStringField(TEXT("pipeName"), FBridgeHost::Get().GetPipeName());

	// Current world / level
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (World)
	{
		Data->SetStringField(TEXT("currentLevel"), World->GetMapName());
		Data->SetBoolField(TEXT("isPIEActive"), World->IsPlayInEditor());
		Data->SetBoolField(TEXT("isPIEPaused"), GEditor->PlayWorld != nullptr && GEditor->bRequestEndPlayMapQueued == false && GEditor->PlayWorld->IsPaused());
	}

	// Busy state — IsCompiling/IsReloading removed in UE 5.3; use IHotReloadInterface
	bool bIsBusy = false;
	if (IHotReloadInterface* HotReload = FModuleManager::GetModulePtr<IHotReloadInterface>("HotReload"))
	{
		bIsBusy = HotReload->IsCurrentlyCompiling();
	}
	Data->SetBoolField(TEXT("isBusy"), bIsBusy);

	return Data;
}

TSharedPtr<FJsonObject> FEditorCommandHandler::HandlePlay(const TSharedPtr<FJsonObject>& Args, bool bForce)
{
	if (!GEditor) throw FCommandFailedException(TEXT("INTERNAL_ERROR"), TEXT("GEditor is null."));
	if (GEditor->PlayWorld) throw FCommandFailedException(TEXT("PLAYMODE_ACTIVE"), TEXT("PIE is already running. Stop it first."));

	FRequestPlaySessionParams Params;
	GEditor->RequestPlaySession(Params);

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("message"), TEXT("PIE session requested."));
	return Data;
}

TSharedPtr<FJsonObject> FEditorCommandHandler::HandlePause(const TSharedPtr<FJsonObject>& Args, bool bForce)
{
	if (!GEditor || !GEditor->PlayWorld)
		throw FCommandFailedException(TEXT("PLAYMODE_NOT_ACTIVE"), TEXT("PIE is not running."));

	GEditor->SetPIEWorldsPaused(true);

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("message"), TEXT("PIE paused."));
	return Data;
}

TSharedPtr<FJsonObject> FEditorCommandHandler::HandleStop(const TSharedPtr<FJsonObject>& Args, bool bForce)
{
	if (!GEditor || !GEditor->PlayWorld)
		throw FCommandFailedException(TEXT("PLAYMODE_NOT_ACTIVE"), TEXT("PIE is not running."));

	GEditor->RequestEndPlayMap();

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("message"), TEXT("PIE stop requested."));
	return Data;
}

TSharedPtr<FJsonObject> FEditorCommandHandler::HandleCompile(const TSharedPtr<FJsonObject>& Args, bool bForce)
{
	if (!GEditor) throw FCommandFailedException(TEXT("INTERNAL_ERROR"), TEXT("GEditor is null."));
	if (GEditor->PlayWorld) throw FCommandFailedException(TEXT("PLAYMODE_ACTIVE"), TEXT("Cannot compile during PIE."));

	IHotReloadInterface& HotReloadInterface = FModuleManager::LoadModuleChecked<IHotReloadInterface>("HotReload");
	if (HotReloadInterface.IsCurrentlyCompiling()) throw FCommandFailedException(TEXT("EDITOR_BUSY"), TEXT("Already compiling."));
	HotReloadInterface.DoHotReloadFromEditor(EHotReloadFlags::None);

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("message"), TEXT("Compile triggered. Use --wait to poll for completion."));
	return Data;
}

TSharedPtr<FJsonObject> FEditorCommandHandler::HandleRefresh(const TSharedPtr<FJsonObject>& Args, bool bForce)
{
	if (!GEditor) throw FCommandFailedException(TEXT("INTERNAL_ERROR"), TEXT("GEditor is null."));

	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	AssetRegistryModule.Get().SearchAllAssets(false);

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("message"), TEXT("Asset refresh triggered."));
	return Data;
}

TSharedPtr<FJsonObject> FEditorCommandHandler::HandleReadLog(const TSharedPtr<FJsonObject>& Args, bool bForce)
{
	int32 Limit = 50;
	FString TypeFilter;
	if (Args.IsValid())
	{
		Args->TryGetNumberField(TEXT("limit"), Limit);
		Args->TryGetStringField(TEXT("type"), TypeFilter);
	}

	ELogVerbosity::Type FilterVerbosity = ELogVerbosity::All;
	if (TypeFilter == TEXT("error")) FilterVerbosity = ELogVerbosity::Error;
	else if (TypeFilter == TEXT("warning")) FilterVerbosity = ELogVerbosity::Warning;
	else if (TypeFilter == TEXT("log")) FilterVerbosity = ELogVerbosity::Log;

	auto Entries = FLogCapture::Get().GetEntries(Limit, FilterVerbosity);

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	TArray<TSharedPtr<FJsonValue>> Logs;
	for (const auto& Entry : Entries)
	{
		TSharedPtr<FJsonObject> LogEntry = MakeShared<FJsonObject>();
		LogEntry->SetStringField(TEXT("message"), Entry.Message);
		FString VerbStr;
		switch (Entry.Verbosity)
		{
			case ELogVerbosity::Error:   VerbStr = TEXT("error"); break;
			case ELogVerbosity::Warning: VerbStr = TEXT("warning"); break;
			default: VerbStr = TEXT("log"); break;
		}
		LogEntry->SetStringField(TEXT("type"), VerbStr);
		LogEntry->SetStringField(TEXT("time"), Entry.Time.ToIso8601());
		Logs.Add(MakeShared<FJsonValueObject>(LogEntry));
	}
	Data->SetArrayField(TEXT("entries"), Logs);
	Data->SetNumberField(TEXT("count"), Logs.Num());
	return Data;
}

TSharedPtr<FJsonObject> FEditorCommandHandler::HandleScreenshot(const TSharedPtr<FJsonObject>& Args, bool bForce)
{
	FString Viewport = TEXT("game");
	FString OutputPath;
	int32 Width = 0, Height = 0;

	if (Args.IsValid())
	{
		Args->TryGetStringField(TEXT("viewport"), Viewport);
		Args->TryGetStringField(TEXT("path"), OutputPath);
		Args->TryGetNumberField(TEXT("width"), Width);
		Args->TryGetNumberField(TEXT("height"), Height);
	}

	if (OutputPath.IsEmpty())
	{
		OutputPath = FPaths::Combine(
			FPaths::ProjectSavedDir(),
			TEXT("Screenshots"),
			FString::Printf(TEXT("unreal-cli-%lld.png"), FDateTime::UtcNow().GetTicks())
		);
	}

	// Request screenshot via Unreal's screenshot system
	FScreenshotRequest::RequestScreenshot(OutputPath, false, false);

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("path"), OutputPath);
	Data->SetStringField(TEXT("viewport"), Viewport);
	Data->SetStringField(TEXT("message"), TEXT("Screenshot requested. File may not be written yet — wait a frame."));
	return Data;
}

TSharedPtr<FJsonObject> FEditorCommandHandler::HandleExecuteMenu(const TSharedPtr<FJsonObject>& Args, bool bForce)
{
	if (!Args.IsValid()) throw FCommandFailedException(TEXT("CLI_USAGE"), TEXT("Arguments required."));

	bool bList = false;
	Args->TryGetBoolField(TEXT("list"), bList);

	if (bList)
	{
		FString Prefix;
		Args->TryGetStringField(TEXT("prefix"), Prefix);

		// UToolMenus::GetAllMenuNames() does not exist in UE 5.3 — return known common menus
		TArray<TSharedPtr<FJsonValue>> Items;
		TArray<FString> KnownMenus = {
			TEXT("LevelEditor.MainMenu"),
			TEXT("LevelEditor.MainMenu.File"),
			TEXT("LevelEditor.MainMenu.Edit"),
			TEXT("LevelEditor.MainMenu.Window"),
			TEXT("LevelEditor.MainMenu.Help"),
			TEXT("LevelEditor.MainMenu.Build"),
			TEXT("LevelEditor.MainMenu.Tools"),
			TEXT("LevelEditor.LevelEditorToolBar.ModesToolBar"),
			TEXT("LevelEditor.LevelEditorToolBar.AssetsToolBar"),
		};
		for (const FString& MenuStr : KnownMenus)
		{
			if (Prefix.IsEmpty() || MenuStr.StartsWith(Prefix))
			{
				Items.Add(MakeShared<FJsonValueString>(MenuStr));
			}
		}

		TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
		Data->SetArrayField(TEXT("items"), Items);
		return Data;
	}

	FString MenuPath;
	if (!Args->TryGetStringField(TEXT("path"), MenuPath) || MenuPath.IsEmpty())
		throw FCommandFailedException(TEXT("CLI_USAGE"), TEXT("--path is required for execute-menu."));

	// Execute via GEditor console command
	if (GEditor)
	{
		GEditor->Exec(GEditor->GetEditorWorldContext().World(), *FString::Printf(TEXT("ExecuteMenuItem %s"), *MenuPath));
	}

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("path"), MenuPath);
	Data->SetStringField(TEXT("message"), TEXT("Menu item executed."));
	return Data;
}
