#pragma once

#include "CoreMinimal.h"
#include "HAL/Runnable.h"
#include "HAL/RunnableThread.h"
#include <atomic>

// Runs a Windows Named Pipe server on a dedicated background thread.
// One connection is served at a time (request → response, then disconnect).
// RequestHandler is invoked on the IPC thread — callers must marshal to the
// game thread themselves if they need UObject access.
class FIpcServer : public FRunnable
{
public:
	using FRequestHandler = TFunction<FString(const FString& RequestJson)>;

	FIpcServer(const FString& InPipeName, FRequestHandler InHandler);
	~FIpcServer();

	bool Start();
	void Stop();

	// FRunnable
	uint32 Run() override;
	void Exit() override;

private:
	void ServeOneClient(void* ClientPipe);

	FString PipeName;
	FRequestHandler Handler;
	FRunnableThread* Thread = nullptr;
	void* StopEvent = nullptr;
	std::atomic<bool> bShouldStop{false};
};
