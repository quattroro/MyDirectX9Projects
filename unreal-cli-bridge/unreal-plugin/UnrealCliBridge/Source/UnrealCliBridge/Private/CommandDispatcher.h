#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

// Handler signature: receives parsed arguments JSON and force flag,
// returns a data JSON object (null on void commands) or throws FCommandFailedException.
using FCommandHandlerFn = TFunction<TSharedPtr<FJsonObject>(
	const TSharedPtr<FJsonObject>& Args, bool bForce)>;

class UNREALCLIBRIDGE_API FCommandFailedException
{
public:
	FCommandFailedException(FString InCode, FString InMessage, FString InDetail = TEXT(""))
		: Code(MoveTemp(InCode)), Message(MoveTemp(InMessage)), Detail(MoveTemp(InDetail)) {}
	FString Code;
	FString Message;
	FString Detail;
};

class FCommandDispatcher
{
public:
	static FCommandDispatcher& Get();

	void Register(const FString& Command, FCommandHandlerFn Handler);

	// Dispatch a raw JSON request line → returns a raw JSON response line (no trailing newline).
	FString Dispatch(const FString& RequestJson);

private:
	TMap<FString, FCommandHandlerFn> Handlers;

	FString BuildOkResponse(const FString& RequestId, TSharedPtr<FJsonObject> Data, double DurationMs) const;
	FString BuildErrorResponse(const FString& RequestId, const FString& Code, const FString& Message,
		const FString& Detail, double DurationMs) const;

	// Runs a handler on the game thread and waits for the result (blocks the calling IPC thread).
	TSharedPtr<FJsonObject> RunOnGameThread(
		const FString& Command,
		const TSharedPtr<FJsonObject>& Args,
		bool bForce);
};
