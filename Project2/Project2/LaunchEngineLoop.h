#include "EditorEngine.h"

ENGINE_API UEngine* GEngine = NULL;

/** implements the main engine loop */
class FEngineLoop : public IEngineLoop
{
public:
    /** pre-initialize the main loop - parse command line, sets up GIsEditor, etc */
    // 6 - Foundation - Entry - FEngineLoop::PreInit
    int32 PreInit(const TCHAR* CmdLine)
    {
        //...
    }

    // 9 - Foundation - Entry - FEngineLoop::Tick
    virtual void Tick() override
    {
        // set FApp::CurrentTime, FApp::DeltaTime and potentially wait to enforce max tick rate
        {
            // haker: as the comments say, it provides CurrentTime, DeltaTime:
            // - however, the most important thing is that this function syncs the GameThread and RenderTherad including RHIThread
            // - these threads are running in parallel:
            //   - to prevent one of these threads running pass over GameThread, the variables like FrameNumber are used to sync each other
            // - I'd like to cover this, but for focuing our main goal of this lecture, leave it in the future:
            //   - I already kept this in my note to remember to cover later
            GEngine->UpdateTimeAndHandleMaxTickRate();
        }

        // main game engine tick (world, game objects, etc)
        // haker:
        // - in editor (lyra project), call the following order: ULyraEditorEngine -> UUnrealEdEngine -> UEditorEngine
        // - we get into UEditorEngine directly
        // see UEditorEngine::Tick
        GEngine->Tick(FApp::GetDeltaTime(), bIdleMode);
    }
};