#include "LaunchEngineLoop.h"

FEngineLoop GEngineLoop;

/** preinit the engine loop */
// 5 - Foundation - Entry - EnginePreInit
int32 EnginePreInit(const TCHAR* CmdLine)
{
    // see FEngineLoop::PreInit()
    int32 ErrorLevel = GEngineLoop.PreInit(CmdLine);
    return ErrorLevel;
}

// 7 - Foundation - Entry - EditorInit
int32 EditorInit(IEngineLoop& EngineLoop)
{
    //...
}

void EngineExit()
{
    RequestEngineExit();
    GEngineLoop.Exit();
}

/** tick the engine loop */
// 8 - Foundation - Entry - EngineTick
void EngineTick()
{
    // see FEngineLoop::Tick
    GEngineLoop.Tick();
}

/**
 * static guarded main function; rolled into own function so we can have error handling for debug/release builds depending on
 * whether a debugger is attached or not
 */
// 4 - Foundation - Entry - GuardMain
int32 GuardMain(const TCHAR* CmdLine)
{
    //...
    GStartTime = FPlatformTime::Seconds();

#if !(UE_BUILD_SHIPPING)
    // haker: passing command arguments with "waitforattach" is useful when you want to debug at the starting of the engine
    if (FParse::Param(CmdLine, TEXT("waitforattach")))
    {
        // haker: when debugger is attached, it will be out of inf-loop
        while (!FPlatformMisc::IsDebuggerPresent())
        {
            FPlatformProcess::Sleep(0.1f);
        }
        // haker: it stops here:
        // - it is VERY useful to debugging the VERY starting point
        UE_DEBUG_BREAK();
    }
#endif

    // super early init code: DO NOT MOVE THIS ANYWHERE ELSE:
    // haker:
    // - CoreDelegates, CoreUObjectDelegates, WorldDelegates, ... 
    // - these delegate classes are good to remember
    // - the unreal engine gives a way to inject the code in a form of provding delegate class like above
    // - here, you **can inject the code very starting point of the unreal engine**
    FCoreDelegates::GetPreMainInitDelegate().Broadcast();

    // make sure GEngineLoop::Exit() is always called:
    // haker: this pattern is frequently used in unreal engine
    struct EngineLoopCleanupGuard
    {
        ~EngineLoopCleanupGuard()
        {
            EngineExit();
        }
    } CleanupGuard;

    // see EnginePreInit()
    int32 ErrorLevel = EnginePreInit(CmdLine);

    {
#if WITH_EDITOR || 1
        if (GIsEditor)
        {
            // haker:
            // - what we are focusing on is the editor build to analyze engine code
            // see EditorInit()
            ErrorLevel = EditorInit(GEngineLoop);
        }
        else
#endif
        {
            ErrorLevel = EngineInit();
        }
    }

    // haker: the pattern to calculate the elapsed time:
    double EngineInitializationTime = FPlatformTime::Seconds() - GStartTime;

    // haker: IsEngineExitRequested() == GIsRequestingExit
    // - here is main loop of the engine
    while (!IsEngineExitRequested())
    {
        // haker: before diving into huge code of EngineTick, let's finished reading rest of code in GuardMain() briefly
        // see EngineTick()
        EngineTick();
    }

#if WITH_EDITOR || 1
    if (GIsEditor)
    {
        EditorExit();
    }
#endif

    return ErrorLevel;
}

/**
 * Windows platform-specific 
 */
// 3 - Foundation - Entry - GuardedMainWrapper
int32 GuardedMainWrapper(const TCHAR* CmdLine)
{
    int32 ErrorLevel = 0;
    // see GuardMain()
    ErrorLevel = GuardMain(CmdLine);
    return ErrorLevel;
}

// 2 - Foundation - Entry - LaunchWindowsStartup
int32 LaunchWindowsStartup(HINSTANCE hInInstance, HINSTANCE hPrevInstance, char*, int32 nCmdShow, const TCHAR* CmdLine)
{
    int32 ErrorLevel = 0;

    if (!CmdLine)
    {
        // haker:
        // - if you have any experience to write your own small game engine or app, it is natural to proces command line:
        // - unreal engine also process command line here to transform command line in their own flavor
        CmdLine = GetCommandLineW();
        if (ProcessCommandLine())
        {
            CmdLine = *GSavedCommandLine;
        }
    }

    {
        // use SEH (structured exception handling) to trap the crash:
        // haker: if you don't know what SEH is, plz searching it and make sure to understand this!
        // - one thing I want to point out is that SEH is triggered by OS (kernel mode):
        //   - the way of behaving SEH is not static, it could be changed anytime by OS provider like MS:
        //      - cuz, it's related OS security itself
        //      - if you are interested in this, start from:
        //          - IDTR register
        //          - IDT(interrupt descriptor table)
        //          - ISRs(interrupt service routines)
        //          - interrupt dispatching
        //          - kernel patch protection
        //      - for my recommendation, just skip this, just good to know ~ :)
        __try
        {
            GIsGuarded = 1;
            // see GuardedMainWrapper()
            ErrorLevel = GuardedMainWrapper(CmdLine);
            GIsGuarded = 0;
        }
        // haker: when exception triggered, catch here
        // - when a crash occurs in the engine, a crash report is generated from here normally
        __except(FPlatformMisc::GetCrashHandlingType == ECrashHandlingType::Default
            ? ReportCrash(GetExceptionInformation())
			: EXCEPTION_CONTINUE_SEARCH)
        {
            ErrorLevel = 1;
            FPlatformMisc::RequestExit(true);
        }
    }

    return ErrorLevel;
}

// 1 - Foundation - Entry - LaunchWindowsShutdown
void LaunchWindowsShutdown()
{
    FEngineLoop::AppExit();
}

// 0 - Foundation - Entry - BEGIN
/** haker: this is the entry in windows platform in the engine */
int32 WinMain(_In_ HINSTANCE hInInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ char* pCmdLine, _In_ int32 nCmdShow)
{
    // see LaunchWindowsShutdown()
    // see LaunchWindowsStartup()
    int32 Result = LaunchWindowsStartup(hInInstance, hPrevInstance, pCmdLine, nCmdShow, nullptr);
    LaunchWindowsShutdown();
    return Result;
}