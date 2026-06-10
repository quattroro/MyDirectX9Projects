#include "ExecuteCommandHandler.h"

// PythonScriptPlugin is conditionally compiled; wrap in a feature flag.
#if WITH_EDITOR && WITH_PYTHON_SCRIPT_PLUGIN
#include "IPythonScriptPlugin.h"
#endif

DEFINE_LOG_CATEGORY_STATIC(LogUnrealCliBridgeExecute, Log, All);

void FExecuteCommandHandler::RegisterAll(FCommandDispatcher& Dispatcher)
{
	Dispatcher.Register(TEXT("execute"), &HandleExecute);
}

TSharedPtr<FJsonObject> FExecuteCommandHandler::HandleExecute(const TSharedPtr<FJsonObject>& Args, bool bForce)
{
	if (!bForce)
		throw FCommandFailedException(TEXT("FORCE_REQUIRED"), TEXT("execute always requires --force."));

	FString Code;
	if (!Args.IsValid() || !Args->TryGetStringField(TEXT("code"), Code) || Code.IsEmpty())
		throw FCommandFailedException(TEXT("CLI_USAGE"), TEXT("--code is required."));

	FString ArgsJson;
	Args->TryGetStringField(TEXT("argsJson"), ArgsJson);

#if WITH_EDITOR && WITH_PYTHON_SCRIPT_PLUGIN
	IPythonScriptPlugin* PythonPlugin = FModuleManager::Get().GetModulePtr<IPythonScriptPlugin>("PythonScriptPlugin");
	if (!PythonPlugin || !PythonPlugin->IsPythonAvailable())
	{
		throw FCommandFailedException(TEXT("PYTHON_UNAVAILABLE"),
			TEXT("Python Script Plugin is not enabled. Enable it in Edit > Plugins > Scripting > Python Script Plugin."));
	}

	// Inject args as a variable
	FString Preamble;
	if (!ArgsJson.IsEmpty())
	{
		// Escape single quotes in the JSON for Python string literal
		FString Escaped = ArgsJson.Replace(TEXT("'"), TEXT("\\'"));
		Preamble = FString::Printf(TEXT("import json\nargs = json.loads('%s')\n"), *Escaped);
	}

	FString FullCode = Preamble + Code;

	FPythonCommandEx PythonCommand;
	PythonCommand.Command = FullCode;
	PythonCommand.ExecutionMode = EPythonCommandExecutionMode::ExecuteFile;
	PythonCommand.FileExecutionScope = EPythonFileExecutionScope::Private;

	bool bSuccess = PythonPlugin->ExecPythonCommandEx(PythonCommand);

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetBoolField(TEXT("success"), bSuccess);
	if (!PythonCommand.CommandResult.IsEmpty())
	{
		Data->SetStringField(TEXT("output"), PythonCommand.CommandResult);
	}
	if (!bSuccess && !PythonCommand.CommandResult.IsEmpty())
	{
		Data->SetStringField(TEXT("error"), PythonCommand.CommandResult);
	}
	return Data;
#else
	throw FCommandFailedException(TEXT("PYTHON_UNAVAILABLE"), TEXT("Python Script Plugin is not available in this engine build. Enable it in Edit > Plugins > Scripting > Python Script Plugin."));
#endif
}
