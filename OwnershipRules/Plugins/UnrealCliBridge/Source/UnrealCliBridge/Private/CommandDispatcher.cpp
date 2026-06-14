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

	FCommandHandlerFn* HandlerPtr = Handlers.Find(Command);
	if (!HandlerPtr)
	{
		throw FCommandFailedException(TEXT("CLI_USAGE"), FString::Printf(TEXT("Unknown command: %s"), *Command));
	}
	FCommandHandlerFn HandlerCopy = *HandlerPtr;

	// Use FEvent + shared result struct instead of TPromise/TFuture.
	// TPromise abandoned without SetValue() (e.g. during editor shutdown) puts the future into a
	// non-complete state; TFuture::Wait() returns immediately but TFuture::Get() then asserts
	// IsComplete() and crashes. FEvent avoids that entire class of issue.
	struct FGameThreadResult
	{
		TSharedPtr<FJsonObject> Data;
		TOptional<FCommandFailedException> Exception;
	};
	TSharedPtr<FGameThreadResult> Result = MakeShared<FGameThreadResult>();
	FEvent* DoneEvent = FPlatformProcess::GetSynchEventFromPool(false);

	AsyncTask(ENamedThreads::GameThread, [
		HandlerCopy = MoveTemp(HandlerCopy),
		Args,
		bForce,
		Result,
		DoneEvent
	]() mutable
	{
		try
		{
			Result->Data = HandlerCopy(Args, bForce);
		}
		catch (const FCommandFailedException& Ex)
		{
			Result->Exception = Ex;
		}
		catch (...)
		{
			Result->Exception = FCommandFailedException(TEXT("INTERNAL_ERROR"), TEXT("Unhandled exception on game thread."));
		}
		DoneEvent->Trigger();
	});

	static constexpr uint32 TimeoutMs = 30000;
	const bool bCompleted = DoneEvent->Wait(TimeoutMs);
	if (bCompleted)
	{
		FPlatformProcess::ReturnSynchEventToPool(DoneEvent);
	}
	// On timeout, intentionally leak DoneEvent: the game thread may still call Trigger() later
	// and returning it to the pool would corrupt a recycled handle.

	if (!bCompleted)
	{
		throw FCommandFailedException(TEXT("TIMEOUT"), TEXT("Game thread did not respond within 30 seconds."));
	}

	if (Result->Exception.IsSet())
	{
		throw Result->Exception.GetValue();
	}

	return Result->Data;
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
