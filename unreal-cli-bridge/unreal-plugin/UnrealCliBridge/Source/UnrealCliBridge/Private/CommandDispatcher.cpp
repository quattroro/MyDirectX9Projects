#include "CommandDispatcher.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/JsonSerializer.h"
#include "Async/Async.h"
#include "Misc/EngineVersion.h"

DEFINE_LOG_CATEGORY_STATIC(LogUnrealCliBridgeDispatcher, Log, All);

static const FString ProtocolVersion = TEXT("1.0.0");

FCommandDispatcher& FCommandDispatcher::Get()
{
	static FCommandDispatcher Instance;
	return Instance;
}

void FCommandDispatcher::Register(const FString& Command, FCommandHandlerFn Handler)
{
	Handlers.Add(Command, MoveTemp(Handler));
}

FString FCommandDispatcher::Dispatch(const FString& RequestJson)
{
	double StartTime = FPlatformTime::Seconds();

	// Parse request
	TSharedPtr<FJsonObject> RequestObj;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(RequestJson);
	if (!FJsonSerializer::Deserialize(Reader, RequestObj) || !RequestObj.IsValid())
	{
		return BuildErrorResponse(TEXT(""), TEXT("PARSE_ERROR"), TEXT("Invalid JSON request"), TEXT(""), 0.0);
	}

	FString RequestId = RequestObj->GetStringField(TEXT("requestId"));
	FString Command = RequestObj->GetStringField(TEXT("command"));
	bool bForce = RequestObj->GetBoolField(TEXT("force"));

	// Protocol version check
	FString IncomingVersion = RequestObj->GetStringField(TEXT("protocolVersion"));
	if (!IncomingVersion.IsEmpty() && IncomingVersion != ProtocolVersion)
	{
		double DurationMs = (FPlatformTime::Seconds() - StartTime) * 1000.0;
		return BuildErrorResponse(RequestId, TEXT("PROTOCOL_MISMATCH"),
			FString::Printf(TEXT("Protocol version mismatch: expected %s, got %s. Upgrade both CLI and plugin."),
				*ProtocolVersion, *IncomingVersion),
			TEXT(""), DurationMs);
	}

	// ping shortcut (no game thread needed)
	if (Command == TEXT("ping"))
	{
		TSharedPtr<FJsonObject> PongData = MakeShared<FJsonObject>();
		PongData->SetStringField(TEXT("pong"), TEXT("ok"));
		double DurationMs = (FPlatformTime::Seconds() - StartTime) * 1000.0;
		return BuildOkResponse(RequestId, PongData, DurationMs);
	}

	FCommandHandlerFn* HandlerPtr = Handlers.Find(Command);
	if (!HandlerPtr)
	{
		double DurationMs = (FPlatformTime::Seconds() - StartTime) * 1000.0;
		return BuildErrorResponse(RequestId, TEXT("CLI_USAGE"),
			FString::Printf(TEXT("Unknown command: %s"), *Command), TEXT(""), DurationMs);
	}

	const TSharedPtr<FJsonObject>* ArgsPtr = nullptr;
	RequestObj->TryGetObjectField(TEXT("arguments"), ArgsPtr);
	TSharedPtr<FJsonObject> Args = (ArgsPtr && ArgsPtr->IsValid()) ? *ArgsPtr : MakeShared<FJsonObject>();

	TSharedPtr<FJsonObject> ResultData;
	FString ErrorCode;
	FString ErrorMessage;
	FString ErrorDetail;

	try
	{
		ResultData = RunOnGameThread(Command, Args, bForce);
	}
	catch (const FCommandFailedException& Ex)
	{
		ErrorCode = Ex.Code;
		ErrorMessage = Ex.Message;
		ErrorDetail = Ex.Detail;
	}
	catch (const std::exception& Ex)
	{
		ErrorCode = TEXT("INTERNAL_ERROR");
		ErrorMessage = UTF8_TO_TCHAR(Ex.what());
	}
	catch (...)
	{
		ErrorCode = TEXT("INTERNAL_ERROR");
		ErrorMessage = TEXT("An unknown error occurred.");
	}

	double DurationMs = (FPlatformTime::Seconds() - StartTime) * 1000.0;

	if (!ErrorCode.IsEmpty())
	{
		return BuildErrorResponse(RequestId, ErrorCode, ErrorMessage, ErrorDetail, DurationMs);
	}

	return BuildOkResponse(RequestId, ResultData, DurationMs);
}

TSharedPtr<FJsonObject> FCommandDispatcher::RunOnGameThread(
	const FString& Command,
	const TSharedPtr<FJsonObject>& Args,
	bool bForce)
{
	// If already on the game thread (shouldn't normally happen), run directly.
	if (IsInGameThread())
	{
		FCommandHandlerFn& Handler = Handlers[Command];
		return Handler(Args, bForce);
	}

	// Marshal to game thread via promise/future pattern.
	TPromise<TSharedPtr<FJsonObject>> ResultPromise;
	TFuture<TSharedPtr<FJsonObject>> ResultFuture = ResultPromise.GetFuture();

	// We need to capture the exception across threads.
	TSharedPtr<TOptional<FCommandFailedException>> CaughtException = MakeShared<TOptional<FCommandFailedException>>();

	FCommandHandlerFn* HandlerPtr = Handlers.Find(Command);
	if (!HandlerPtr)
	{
		throw FCommandFailedException(TEXT("CLI_USAGE"), FString::Printf(TEXT("Unknown command: %s"), *Command));
	}
	FCommandHandlerFn HandlerCopy = *HandlerPtr;

	AsyncTask(ENamedThreads::GameThread, [
		HandlerCopy = MoveTemp(HandlerCopy),
		Args,
		bForce,
		Promise = MoveTemp(ResultPromise),
		CaughtException
	]() mutable
	{
		TSharedPtr<FJsonObject> Data;
		try
		{
			Data = HandlerCopy(Args, bForce);
		}
		catch (const FCommandFailedException& Ex)
		{
			*CaughtException = Ex;
		}
		catch (...)
		{
			*CaughtException = FCommandFailedException(TEXT("INTERNAL_ERROR"), TEXT("Unhandled exception on game thread."));
		}
		Promise.SetValue(MoveTemp(Data));
	});

	// Block the IPC thread waiting for the game thread result.
	TSharedPtr<FJsonObject> Data = ResultFuture.Get();

	if (CaughtException->IsSet())
	{
		throw CaughtException->GetValue();
	}

	return Data;
}

FString FCommandDispatcher::BuildOkResponse(const FString& RequestId, TSharedPtr<FJsonObject> Data, double DurationMs) const
{
	TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("protocolVersion"), ProtocolVersion);
	Root->SetStringField(TEXT("requestId"), RequestId);
	Root->SetStringField(TEXT("status"), TEXT("ok"));
	Root->SetNumberField(TEXT("durationMs"), DurationMs);
	Root->SetStringField(TEXT("transport"), TEXT("live"));
	if (Data.IsValid())
	{
		Root->SetObjectField(TEXT("data"), Data);
	}

	FString Output;
	TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
		TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Output);
	FJsonSerializer::Serialize(Root.ToSharedRef(), Writer);
	return Output;
}

FString FCommandDispatcher::BuildErrorResponse(const FString& RequestId, const FString& Code,
	const FString& Message, const FString& Detail, double DurationMs) const
{
	TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("protocolVersion"), ProtocolVersion);
	Root->SetStringField(TEXT("requestId"), RequestId);
	Root->SetStringField(TEXT("status"), TEXT("error"));
	Root->SetStringField(TEXT("errorCode"), Code);
	Root->SetStringField(TEXT("errorMessage"), Message);
	Root->SetNumberField(TEXT("durationMs"), DurationMs);
	Root->SetStringField(TEXT("transport"), TEXT("live"));
	if (!Detail.IsEmpty())
	{
		Root->SetStringField(TEXT("errorDetail"), Detail);
	}

	FString Output;
	TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
		TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Output);
	FJsonSerializer::Serialize(Root.ToSharedRef(), Writer);
	return Output;
}
