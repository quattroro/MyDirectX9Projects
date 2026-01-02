/**
 * an abstract interface to a viewport's client
 * the viewport's client processes input received by the viewport, and draws the viewport 
 */
// haker: FViewportClient is the interface class:
// 1. recevie and process inputs
// 2. draw viewport
//
// what is the meaning of ViewportClient?
// - ViewportClient means 'Viewport's Client'
// - let's see the picture below:
//                                                                                                  ┌──────────────────┐                                          
//                                                                                                  │ Operating System │                                          
//           ┌────────────────────────────────────────────────────────────────────────────────────┐ └───────┬──────────┘                                          
//           ▼                                                                                    │         │                                                     
//    Viewport in OS                                                                              │         │                                                     
//    ┌──────────────────────────────────────┬─┬─┬─┐                                              │    ┌────▼─────┐                                               
//    │                                      │-│o│x│                                              │    │ Process0 │                                               
//    ├──────────────────────────────────────┴─┴─┴─┤                                              │    ├──────────┤                                               
//    │                                            │                                              ├────┤ Process1 │                                               
//    │                                            │  Viewport's Client?                          │    ├──────────┤                                               
//    │                                            │   :Process(Main Thread)                      │    │ Process2 │                                               
//    │                                            │                        ┌───────────────┐     │    ├──────────┤                                               
//    │                                            │ ──────────────────────►│FViewportClient│◄────┘    │ Process3 │                                               
//    │                                            │                        └───────┬───────┘          └──────────┘                                               
//    │                                            │                                │                                                                             
//    │                                            │                                │                                                                             
//    │                                            │                       1.ViewportClient is the bridge between Process and UnrealEngine                        
//    │                                            │                                                                                                              
//    │                                            │                       2.ViewportClient listen input interrupts from Process (from OS)                        
//    │                                            │                                                                                                              
//    │                                            │                            OS ─────► Process ─────► FViewportClient ────► Unreal System                      
//    │                                            │                                                                                                              
//    └────────────────────────────────────────────┘                            : Transfer input events from OS to Unreal Engine                                  
//                                                                                                                                                                
//                                                                         3.Viewport doesn't mean graphical viewport (D3D11), it is about OS's window' viewport  
//                                                                                                                                                
// - goto UScriptViewportClient
class FViewportClient
{
    virtual void Draw(FViewport* Viewport, FCanvas* Canvas) {}

    /** 
     * check a key event received by the viewport 
     * if the viewport client uses the event, it should return true to consume it
     */
    virtual bool InputKey(const FInputKeyEventArgs& EventArgs)
    {
        //...
    }

    /**
     * check an axis movement received by the viewport
     * if the viewport client uses the movement, it should return true to consume it 
     */
    virtual bool InputAxis(FViewport* Viewport, FInputDeviceId InputDevice, FKey Key, float Delta, float DeltaTime, int32 NumSamples = 1, bool bGamepad = false) { return false; }
};

/** common functionality for game and editor viewport clients */
class FCommonViewportClient : public FViewportClient
{

};

// haker: UScriptViewportClient is the UObject warpper class for Viewport client
class UScriptViewportClient : public UObject, public FCommonViewportClient
{

};

/** a render target */
class FRenderTarget
{
    FTextureRHIRef RenderTargetTextureRHI;
};

/** a rendering resource which is owned by the rendering thread */
class FRenderResource
{

};

/** encapsulate the canvas state */
// 011 - Foundation - SceneRenderer * - FCanvas
// haker: try to understand 'FCanvas' with the diagram:
//
//    FViewport(in GameViewportClient)                                                                                             
//    ┌──────────────────────────┬─────────────────────────────┐                                                                   
//    │                          │                             │                                                                   
//    │ Canvas0                  │ Canvas1◄─────┬──────────────┼────────***Canvas is subregion of FViewport to do RedrawViewport() 
//    │     ▲                    │              │              │                                                                   
//    │     └────────────────────┼──────────────┤              │                                                                   
//    │                          │              │              │                                                                   
//    │                          │              │              │                                                                   
//    │                          │              │              │                                                                   
//    │                          │              │              │                                                                   
//    │                          │              │              │                                                                   
//    │                          │              │              │                                                                   
//    │                          │              │              │                                                                   
//    ├──────────────────────────┼──────────────┼──────────────┤                                                                   
//    │                          │              │              │                                                                   
//    │ Canvas2                  │ Canvas3      │              │                                                                   
//    │     ▲                    │    ▲         │              │                                                                   
//    │     │                    │    │         │              │                                                                   
//    │     └────────────────────┼────┴─────────┘              │                                                                   
//    │                          │                             │                                                                   
//    │                          │                             │                                                                   
//    │                          │                             │                                                                   
//    │                          │                             │                                                                   
//    │                          │                             │                                                                   
//    │                          │                             │                                                                   
//    └──────────────────────────┴─────────────────────────────┘         
class FCanvas
{
    /**
     * sets a rect that should be used to offset rendering into the viewport render target
     * if not set the canvas will render to the full target 
     */
    void SetRenderTargetRect(const FIntRect& InViewRect)
    {
        ViewRect = InViewRect;
    }

    void Flush_GameThread()
    {
        //...
    }

    /** view rect for the render target */
    FIntRect ViewRect;
};

/**
 * encapsulates the I/O of a viewport
 * the viewport display is implemented using the platform independent RHI 
 */
class FViewport : public FRenderTarget, protected FRenderResource
{
    virtual FIntPoint GetSizeXY() const override { return FIntPoint(SizeX, SizeY); }
    FViewportClient* GetClient() const { return ViewportClient; }
    FIntPoint GetInitialPositionXY() const { return FIntPoint(InitialPositionX, InitialPositionY); }

    virtual float GetDesiredAspectRatio() const
    {
        FIntPoint Size = GetSizeXY();
        return (float)Size.X / (float)Size.Y;
    }

    /** updates the viewport's displayed pixels with the results of calling ViewportClient->Draw() */
    // 010 - Foundation - SceneRenderer * - FViewport::Draw()
    void Draw(bool bShouldPresent=true)
    {
        // haker: UGameViewportClient::GetWorld()
        UWorld* World = GetClient()->GetWorld();

        //...

        UWorld* ViewportWorld = World;

        // haker: see FCanvas (goto 011: SceneRenderer)
        // - in our case, the Canvas covers whole one viewport
        FCanvas Canvas(this, nullptr, ViewportWorld, ViewportWorld->GetFeatureLevel(), FCanvas::CDM_DeferDrawing, ViewportClient->ShouldDPIScaleSceneCanvas() ? ViewportClient->GetDPIScale() : 1.0f);

        // haker: SetRenderTargetRect() marks subregion of viewport to draw and then Flush_GameThread clear the subregion of viewport we assigned previously
        // - see SetRenderTargetRect and Flush_GameThread() briefly
        Canvas.SetRenderTargetRect(FIntRect(0, 0, SizeX, SizeY));
        {
            // haker: UGameViewportClient::Draw (goto 012: SceneRenderer)
            ViewportClient->Draw(this, &Canvas);
        }
        Canvas.Flush_GameThread();
    }

    /** the viewport's client */
    // haker: UGameViewportClient
    FViewportClient* ViewportClient;

    /** the initial position of the viewport */
    uint32 InitialPositionX;
    uint32 InitialPositionY;

    /** the width/height of the viewport */
    uint32 SizeX;
    uint32 SizeY;
};

/** a viewport for use with Slate SViewport widgets */
class FSceneViewport : public FViewportFrame, public FViewport, public ISlateViewprt, public IViewportRenderTargetProvider
{

};

/** a set of views into a scene which only have different view transforms and owner actors */ 
// 014 - Foundation - SceneRenderer * - FSceneViewFamily
// haker: it is easier to understand by decomposing 'FSceneViewFamily' to 'FSceneView' + 'Family'
// - SceneViewFamily has multiple SceneViews
class FSceneViewFamily
{
    /**
     * helper struct for creating FSceneViewFamily instances
     * if created with specifying a time, it will retrieve them from the world in the given scene 
     */
    struct ConstructionValues
    {

    };

    /** the views which make up the family */
    // haker: see FSceneView (goto 015: SceneRenderer)
    TArray<const FSceneView*> Views;
};

/** a view family which deletes its views when it goes out of scope */
// 013 - Foundation - SceneRenderer * - FSceneViewFamilyContext
// haker: FSceneViewFamilyContext is RAII(Resource Acquisition Is Initialization) of SceneViewFamily
// - firsly see FSceneViewFamily (goto 014: SceneRenderer)
class FSceneViewFamilyContext : public FSceneViewFamily
{
    FSceneViewFamilyContext(const ConstructionValues& CVS)
        : FSceneViewFamily(CVS)
    {}

    ~FSceneViewFamilyContext()
    {
        // clean-up the views
        for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
        {
            delete Views[ViewIndex];
        }
    }
};

/**
 * a game viewport (FViewport) is high-level abstract interface for the
 * the platform specific rendering, audio, and input subsystems.
 * GameViewportClient is the engine's interface to a game viewport.
 * Exactly, one GameViewportClient is created for each instance of the game.
 * The only case (so far) where you might have a single instance of Engine, but 
 * multiple instances of the game (and thus multiple GameViewportClients) is when
 * you have more than one PIE window running
 * 
 * Responsibilities:
 * - propagate input events to the global interactions list
 */
// 006 - Foundation - CreateInnerProcessPIEGameInstance * - UGameViewportClient
// haker: see all base classes of UScriptViewportClient:
// 1. UScriptViewportClient, FCommonViewportClient, FViewportClient
//
// 2. when we consider our case, PIE, it has one GameViewportClient
//    - we already covered what FViewportClient means, let's expand it by introducing platforms:
//                                   │                                                                                                                
//    Platform ~= Operating System   │            Unreal                                                                                              
//                                   │                                                                                                                
//     ┌────────┐                    │                                                                                                                
//     │  PC    ├──────────┐         │                                        ┌───────────────┐                                                       
//     └────────┘          │         │                                  ┌────►│ViewportConsole│                                                       
//                         │         │                       InputAxis  │     └───────────────┘                                                       
//     ┌────────┐          │         │                                  │                                                                             
//     │ Xbox S ├──────────┤         │          ┌───────────────────┐   │     ┌───────────────┐                                                       
//     └────────┘          ├─────────┼──────────►UGameViewportClient├───┴────►│ULocalPlayer   ├───────►PlayerController────────►Character             
//                         │         │      ▲   └───────────────────┘         └───────────────┘                                                       
//     ┌────────┐          │         │      │                                                                                                         
//     │ PS5    ├──────────┤         │      │                                                                                                         
//     └────────┘          │         │      │                                                                                                         
//                         │         │      │                                                                                                         
//     ┌────────┐          │         │   - In Unreal, various system wraps platform specific handling: e.g. SlateApplication                          
//     │ Switch ├──────────┘         │   - Various systems puts platform events like input,viewport(window) size changes, ... to UGameViewportClient  
//     └────────┘                    │                                                                                                                
//                                   │                                                                                                                
//                                   │                                                                                                                

class UGameViewportClient : public UScriptViewportClient, public FExec
{
    // 018 - Foundation - CreateInnerProcessPIEGameInstance * - UGameViewportClient::SetupInitialLocalPlayer
    virtual ULocalPlayer* SetupInitialLocalPlayer(FString& OutError)
    {
        // haker: we can get GameInstance from WorldContext:
        // see UEngine::GetWorldContextFromGameViewportChecked (goto 019: CreateInnerProcessPIEGameInstance)
        // - from WorldContext, we can look up FWorldContext::OwningGameInstance
        UGameInstance* ViewportGameInstance = GEngine->GetWorldContextFromGameViewportChecked(this).OwningGameInstance;
        if (!ensure(ViewportGameInstance))
        {
            return NULL;
        }

        // create the initial player - this is necessary or we can't render anything in-game
        // haker: using GameInstance, we create new LocalPlayer
        // see UGameInstance::CreateInitialPlayer (goto 020: CreateInnerProcessPIEGameInstance)
        return ViewportGameInstance->CreateInitialPlayer(OutError);
    }

    /** returns a relative world context for this viewport */
    virtual UWorld* GetWorld() const override
    {
        return World;
    }

    // 012 - Foundation - SceneRenderer * - UGameViewportClient::Draw
    virtual void Draw(FViewport* InViewport, FCanvas* SceneCanvas) override
    {
        UWorld* MyWorld = GetWorld();
        if (MyWorld == nullptr)
        {
            return;
        }

        // haker: before continuing, let's see the classes first:
        // - see FSceneViewFamilyContext (goto 013: SceneRenderer)

        // create the view family for rendering the world scene to the viewport's render target
        // haker: every 'FRAME', we create SceneViewFamilyContext to draw the scene
        // - I'll leave the detail of FSceneViewFamily::ConstructionValues and FSceneViewFamilyContext's constructor/destructor
        // - see briefly FSceneViewFamilyContext's constructor and destructor
        FSceneViewFamilyContext ViewFamily(FSceneViewFamily::ConstructionValues(
            InViewport,
            InWorld->Scene,
            EngineShowFlags)
            .SetRealtimeUpdate(true));
        
        TArray<FSceneView*> Views;

        // calculate the player's view information
        // haker: iterating LocalPlayers in the world, create SceneView for this frame
        for (FLocalPlayerIterator Iterator(GEngine, MyWorld); Iterator; ++Iterator)
        {
            ULocalPlayer* LocalPlayer = *Iterator;
            if (LocalPlayer)
            {
                // haker: note that 'LocalPlayer' calculates camera information for RenderThread not 'World'
                // - the 'World' has the state of camera, but 'LocalPlayer' has the behavior ownership to calculate and reflect camera data to RenderWorld(Scene)
                // - see ULocalPlayer::CalcSceneView (goto 017: SceneRenderer)
                FSceneView* View = LocalPlayer->CalcSceneView(&ViewFamily, ViewLocation, ViewRotation, InViewport, nullptr, INDEX_NONE);
                if (View)
                {
                    Views.Add(View);
                }

                //...
            }
        }

        // 032 - Foundation - SceneRenderer * - End of CalcSceneView

        // draw the player views
        {
            // haker: see BeginRenderinvViewFamily (goto 033: SceneRenderer)
            GetRendererModule().BeginRenderingViewFamily(SceneCanvas, &ViewFamily);
        }
    }

    /** the platform-specific viewport which this viewport client is attached to */
    // haker: FSceneViewport
    FViewport* Viewport;

    /** relative world context for this viewport */
    TObjectPtr<UWorld> World;
};