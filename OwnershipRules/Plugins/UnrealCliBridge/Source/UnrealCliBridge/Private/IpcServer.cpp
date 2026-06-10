#include "IpcServer.h"

#if PLATFORM_WINDOWS
#include "Windows/AllowWindowsPlatformTypes.h"
#include <windows.h>
// HideWindowsPlatformTypes is at the bottom of this file so Windows macros
// (TRUE, FALSE, HANDLE, etc.) remain visible throughout the implementation.
#endif

DEFINE_LOG_CATEGORY_STATIC(LogUnrealCliBridgeIpc, Log, All);

static constexpr uint32 PipeBufferSize = 65536;

FIpcServer::FIpcServer(const FString& InPipeName, FRequestHandler InHandler)
	: PipeName(InPipeName), Handler(MoveTemp(InHandler))
{
#if PLATFORM_WINDOWS
	// Store a raw HANDLE so we can signal it from Stop() without FEvent::GetNativeRef
	StopEvent = CreateEvent(nullptr, TRUE, FALSE, nullptr);
#endif
}

FIpcServer::~FIpcServer()
{
	Stop();
#if PLATFORM_WINDOWS
	if (StopEvent)
	{
		CloseHandle((HANDLE)StopEvent);
		StopEvent = nullptr;
	}
#endif
}

bool FIpcServer::Start()
{
	bShouldStop = false;
	Thread = FRunnableThread::Create(this, TEXT("UnrealCliBridge.IpcServer"), 0, TPri_BelowNormal);
	return Thread != nullptr;
}

void FIpcServer::Stop()
{
	bShouldStop = true;
#if PLATFORM_WINDOWS
	if (StopEvent)
	{
		SetEvent((HANDLE)StopEvent);
	}
#endif
	if (Thread)
	{
		Thread->WaitForCompletion();
		delete Thread;
		Thread = nullptr;
	}
}

uint32 FIpcServer::Run()
{
#if PLATFORM_WINDOWS
	const FString FullPipeName = FString::Printf(TEXT("\\\\.\\pipe\\%s"), *PipeName);

	while (!bShouldStop)
	{
		HANDLE PipeHandle = CreateNamedPipeW(
			*FullPipeName,
			PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED,
			PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT,
			1,
			PipeBufferSize,
			PipeBufferSize,
			5000,
			nullptr
		);

		if (PipeHandle == INVALID_HANDLE_VALUE)
		{
			UE_LOG(LogUnrealCliBridgeIpc, Error, TEXT("CreateNamedPipe failed: %d"), GetLastError());
			FPlatformProcess::Sleep(1.0f);
			continue;
		}

		OVERLAPPED Overlapped = {};
		Overlapped.hEvent = CreateEvent(nullptr, TRUE, FALSE, nullptr);

		BOOL ConnectResult = ConnectNamedPipe(PipeHandle, &Overlapped);
		DWORD LastError = GetLastError();

		bool bClientConnected = false;

		if (ConnectResult || LastError == ERROR_PIPE_CONNECTED)
		{
			bClientConnected = true;
		}
		else if (LastError == ERROR_IO_PENDING)
		{
			HANDLE Events[2] = { Overlapped.hEvent, (HANDLE)StopEvent };
			DWORD WaitResult = WaitForMultipleObjects(2, Events, FALSE, INFINITE);
			if (WaitResult == WAIT_OBJECT_0)
			{
				DWORD BytesTransferred = 0;
				bClientConnected = GetOverlappedResult(PipeHandle, &Overlapped, &BytesTransferred, FALSE) ||
					GetLastError() == ERROR_PIPE_CONNECTED;
			}
		}

		CloseHandle(Overlapped.hEvent);

		if (bClientConnected && !bShouldStop)
		{
			ServeOneClient(PipeHandle);
		}

		DisconnectNamedPipe(PipeHandle);
		CloseHandle(PipeHandle);
	}
#else
	UE_LOG(LogUnrealCliBridgeIpc, Error, TEXT("IpcServer: Named Pipe is only supported on Windows."));
#endif
	return 0;
}

void FIpcServer::ServeOneClient(void* ClientPipeRaw)
{
#if PLATFORM_WINDOWS
	HANDLE Pipe = (HANDLE)ClientPipeRaw;

	TArray<uint8> Buffer;
	Buffer.Reserve(4096);
	uint8 Byte = 0;
	DWORD BytesRead = 0;

	while (true)
	{
		if (!ReadFile(Pipe, &Byte, 1, &BytesRead, nullptr) || BytesRead == 0)
			return;
		if (Byte == '\n')
			break;
		Buffer.Add(Byte);
	}

	if (Buffer.Num() > 0 && Buffer.Last() == '\r')
		Buffer.Pop(false);

	FString RequestJson = FString(UTF8_TO_TCHAR(reinterpret_cast<const char*>(Buffer.GetData())));
	FString ResponseJson = Handler(RequestJson);

	FString WithNewline = ResponseJson + TEXT("\n");
	FTCHARToUTF8 ResponseConverter(*WithNewline);

	DWORD BytesWritten = 0;
	WriteFile(Pipe, ResponseConverter.Get(), ResponseConverter.Length(), &BytesWritten, nullptr);
	FlushFileBuffers(Pipe);
#endif
}

void FIpcServer::Exit()
{
}

#if PLATFORM_WINDOWS
#include "Windows/HideWindowsPlatformTypes.h"
#endif
