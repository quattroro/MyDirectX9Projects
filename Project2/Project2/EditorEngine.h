#include "EngineBaseTypes.h"
#include "World.h"

/** specifies the goal/source of a UWorld object */
// 14 - Foundation - Entry - EWorldType
// haker: note that it is useful pattern define enum in UE5
// - if you wrap it up with namespace, you can avoid enum name conflicts with other global enum
namespace EWorldType
{
    enum Type
    {
        /** an untyped world */
        None,

        /** the game world */
        Game,

        /** a world being edited in the editor */
        Editor,

        /** a play in editor world */
        PIE,
    };
}

/**
 * FWorldContext
 * a context for dealing with UWorlds at the engine level. as the engine brings up and destroys world, we need a way to keep straight
 * what world belongs to what
 * 
 * WorldContexts can be throught of as a track. by default, we have 1 track that we load and unload levels on. adding a second context
 * is adding a second track; another track of progression for worlds to live on.
 * 
 * for the GameEngine, there will be one WorldContext until we decide to support multiple simultaneous worlds.
 * for the EditorEngine, there may be one WorldContext for EditorWorld and one for the PIE World.
 * 
 * FWorldContext provides both a way to manage 'the current PIE UWorld*' as well as state that goes along with connecting/travelling to
 * new worlds
 * 
 * FWorldContext should remain internal to the UEngine classes. outside code should not keep pointers or try to manage FWorldContexts directly.
 * outside code can still deal with UWorld*, and pass UWorld*s into Engine level functions. the Engine code can look up the relevant context
 * for a given UWorld*
 * 
 * for convenience, FWorldContext can maintain outside pointers to UWorld*s. for example, PIE can tie UWorld* UEditorEngine::PlayWorld to the PIE
 * world context. if the PIE UWorld changes, the UEditorEngine::PlayWorld pointer will be automatically updated. this is done with AddRef() and 
 * SetCurrentWorld()
 */
// 13 - Foundation - Entry - FWorldContext
// haker: 
// - the context for world used in engine level (UEngine, UEditorEngine, etc)
//   - you can think of FWorldContext as descriptor for loosing dependency between engine and world
// - for GameEngine:
//   - usually one WorldContext, when switching worlds like lobby to in-game world, there is the moment when two world contexts exist
// - for EditorEngine:
//   - when we editing the world with level viewport, we normally face one world context
//   - when we execute the PIE, the new world context is generated and simultaneously, two world contexts exists
//   - when you try to run multiplay game in PIE, multiple world contexts can exist
// - as I said, the world context is for engine, so do NOT maintain FWorldContext elsewhere
struct FWorldContext
{
    void SetCurrentWorld(UWorld* World)
    {
        UWorld* OldWorld = ThisCurrentWorld;
        ThisCurrentWorld = World;

        if (OwningGameInstance)
        {
            OwningGameInstance->OnWorldChanged(OldWorld, ThisCurrentWorld);
        }
    }

    UWorld* World() const
    {
        return ThisCurrentWorld;
    }

    // see EWorldType
    TEnumAsByte<EWorldType::Type> WorldType;
    
    // haker: assign separate name to context different from UWorld's name
    FName ContextHandle;

    // haker: GameInstance owns ONE world context
    TObjectPtr<class UGameInstance> OwningGameInstance;

    // haker: the world which the context is referencing
    TObjectPtr<UWorld> ThisCurrentWorld;

    TObjectPtr<UGameViewportClient> GameViewport;

    /** The PIE instance of this world, -1 is default */
    int32 PIEInstance;

    /** if > 0, tick this world at a fixed rate in PIE */
    float PIEFixedTickSeconds = 0.f;
    float PIEAccumulatedTickSeconds = 0.f;
};

// 12 - Foundation - Entry - UEngine
class UEngine : public UObject, public FExec
{
    const FWorldContext* GetWorldContextFromGameViewport(const UGameViewportClient *InViewport) const
    {
        for (const FWorldContext& WorldContext : WorldList)
        {
            if (WorldContext.GameViewport == InViewport)
            {
                return &WorldContext;
            }
        }
        return nullptr;
    }

    // 019 - Foundation - CreateInnerProcessPIEGameInstance * - GetWorldContextFromGameViewportChecked
    FWorldContext& GetWorldContextFromGameViewportChecked(const UGameViewportClient* InViewport)
    {
        // haker: see GetWorldContextFromGameViewport briefly:
        // - WorldContext has GameViewportClient reference, so we can find world context by GameViewport
        // - in other words, each world context has its own GameViewportClient
        if (FWorldContext* WorldContext = GetWorldContextFromGameViewport(InViewport))
        {
            return *WorldContext;
        }
        check(false);
    }

    FWorldContext& CreateNewWorldContext(EWorldType::Type WorldType)
    {
        FWorldContext* NewWorldContext = new FWorldContext;
        WorldList.Add(NewWorldContext);
        NewWorldContext->WorldType = WorldType;
        NewWorldContext->ContextHandle = FName(*FString::Printf(TEXT("Context_%d"), NextWorldContextHandle++));
        return *NewWorldContext;
    }

    /** initialize the game engine */
    // 17 - Foundation - Entry - UEngine::Init
    virtual void Init(IEngineLoop* InEngineLoop)
    {
        if (GIsEditor)
        {
            // create a WorldContext for the editor to use and create an initially empty world
            // haker: here, we make sure that at least, one editor world exists
            FWorldConext& InitialWorldContext = CreateNewWorldContext(EWorldType::Editor);

            // haker: 
            // see UWorld::CreateWorld (goto 001)
            // 000 - Foundation - CreateWorld * - BEGIN
            InitialWorldContext.SetCurrentWorld(UWorld::CreateWorld(EWorldType::Editor, true));
            GWorld = InitialWorldContext.World();
            // 082 - Foundation - CreateWorld - END
        }
    }

    // haker: recommend to look into TIndirectArray, focusing on differences from TArray
    // see FWorldContext
    TIndirectArray<FWorldContext> WorldList;
    int32 NextWorldContextHandle;

    TSubclassOf<class ULocalPlayer> LocalPlayerClass;
};

UCLASS(config=Engine, defaultconfig, MinimalAPI)
class UGameMapSettings : public UObject
{
    /** the class to use when instantiating the transient GameInstance class */
    UPROPERTY(config, noclear, EditAnywhere, Category=GameInstance, meta=(MetaClass="/Script/Engine.GameInstance"))
    FSoftClassPath GameInstanceClass;
};

/** the result of a UGameInstance PIE operation */
struct FGameInstancePIEResult
{
    bool IsSuccess() const { return bSuccess; }

    /** if not, what was the failure reason */
    FText FailureReason;

    /** did the PIE operation succeed? */
    bool bSuccess;
};

/** sets GWorld to the passed in PlayWorld and sets a global flag indicating that we are playing in the Editor */
UWorld* SetPlayInEditorWorld(UWorld* PlayInEditorWorld)
{
    UWorld* SavedWorld = GWorld;
    GIsPlayInEditorWorld = true;
    GWorld = PlayInEditorWorld;
    return SavedWorld;
}

/** restores GWorld to the passed in one and reset the global flag indicating whether we are a PIE world or not */
void RestoreEditorWorld(UWorld* EditorWorld)
{
    GWorld = EditorWorld;
}

// 11 - Foundation - Entry - UEditorEngine
// see UEngine
class UEditorEngine : public UEngine
{
public:
    /** returns the WorldContext for the editor world. for now, there will always be exactly 1 of these in the editor */
    // 16 - Foundation - Entry - GetEditorWorldContext
    FWorldContext& GetEditorWorldContext(bool bEnsureIsGWorld = false)
    {
        for (int32 i = 0; i < WorldList.Num(); ++i)
        {
            if (WorldList[i].WorldType == EWorldType::Editor)
            {
                return WorldList[i];
            }
        }

        // haker: if we get into this line of code, it will crashed:
        // - where is initial editor world created?
        // - the hint is provided by the below comment
        check(false); // there should have already been one created in ***UEngine::Init***
        // see UEngine::Init()

        return CreateNewWorldContext(EWorldType::Editor);
    }

    // 002 - Foundation - CreateInnerProcessPIEGameInstance * - UEditorEngine::GetWorldContextFromPIEInstance
    FWorldContext* GetWorldContextFromPIEInstance(const int32 PIEInstance)
    {
        // haker: do you remember UEngine has WorldList(FWorldContext)
        for (FWorldContext& WorldContext : WorldList)
        {
            if (WorldContext.WorldType == EWorldType::PIE && WorldContext.PIEInstance == PIEInstance)
            {
                return &WorldContext;
            }
        }
        return nullptr;
    }

    // 10 - Foundation - Entry - UEditorEngine::Tick
    // see UEditorEngine
    virtual void Tick(float DeltaSeconds, bool bIdleMode) override
    {
        // 15 - Foundation - Entry
        // haker: where does GWorld is updated initially?
        UWorld* CurrentGWorld = GWorld;

        // see GetEditorWorldContext()
        FWorldContext& EditorContext = GetEditorWorldContext();
        
        // by default we tick the editor world
        bool bShouldTickEditorWorld = true;

        // 000 - Foundation - TickTaskManager - BEGIN

        ELevelTick TickType = IsRealTime ? LEVELTICK_ViewportOnly : LEVELTICK_TimeOnly;
        if (bShouldTickEditorWorld)
        {
            // NOTE: still allowing the FX system to tick so particle systems don't restart after entering/leaving responsive mode
            // haker: FX(niagara) system should be running even in level-viewport mode
            {
                // haker: here is EditorWorld for level-viewport ticking:
                // - level-viewport's world is ticking in very limited allowance
                // - so, we look into world's tick with PIE Context
                EditorContext.World()->Tick(TickType, DeltaSeconds);
            }
        }

        {
            // determine number of PIE worlds that should tick 
            TArray<FWorldContext*> LocalPieContextPtrs;
            for (FWorldContext& PieContext : WorldList)
            {
                // haker: EWorldType::PIE!
                if (PieContext.WorldType == EWorldType::PIE && PieContext.World() != nullptr)
                {
                    LocalPieContextPtrs.Add(&PieContext);
                }
            }

            // haker: when is WorldContext for PIE initialized? 
            // - when you want to know exactly what happen on this: see the callstack after setting BP(breakpoint) to CreateInnerProcessPIEGameInstance
            // see UEditorEngine::CreateInnerProcessPIEGameInstance()
            // - for now, we are looking into how tick function runs first, then we go back to see how PIE is initialized

            for (FWorldContext* PieContextPtr : LocalPieContextPtrs)
            {
                // haker: after we see CreateInnerProcessPIEGameInstance(), then we understand what GameViewport is for
                FWorldContext& PieContext = *PieContextPtr;
                PlayWorld = PieContext.World();
                GameViewport = PieContext.GameViewport;

                /** how much time to use per tick */
                // haker: nothing special, it allows to run in fixed fps with repsect to DeltaSeconds
                float TickDeltaSeconds;
                if (PieContext.PIEFixedTickSeconds > 0.f)
                {
                    PieContext.PIEAccumulatedTickSeconds += DeltaSeconds;
                    TickDeltaSeconds = PieContext.PIEFixedTickSeconds;
                }
                else
                {
                    // haker: otherwise, we will get into here (one tick per frame)
                    PieContext.PIEAccumulatedTickSeconds = DeltaSeconds;
                    TickDeltaSeconds = DeltaSeconds;
                }

                for (; PieContext.PIEAccumulatedTickSeconds >= TickDeltaSeconds; PieContext.PIEAccumulatedTickSeconds -= TickDeltaSeconds)
                {
                    // update the level
                    {
                        // tick the level
                        // see UWorld::Tick (goto 001 : TickTaskManager)
                        PieContext.World()->Tick(LEVELTICK_All, TickDeltaSeconds);
                    }
                }
            }
        }

        // 039 - Foundation - TickTaskManager - END

        //...

        // 000 - Foundation - SceneRenderer * - BEGIN

        // redraw viewports:
        {
            // gather worlds that need EOF updates
            // - this must be done in two steps as the object hash table is locked during ForEachObjectOfClass so any newObject calls would fail
            // haker: we'll skip ForEachObjectOfClass works:
            // - we collect worlds(UWorld) which has to call EndOfFrame(EOF)Update to 'WorldsToEOFUpdate'
            TArray<UWorld*> WorldsToEOFUpdate;
            ForEachObjectOfClass(UWorld::StaticClass(), [&WorldsToEOFUpdate](UObject* WorldObject)
            {
                UWorld* World = CastChecked<UWorld>(WorldObj);
                if (World->HasEndOfFrameUpdates())
                {
                    WorldsToEOFUpdate.Add(World);
                }
            });

            // make sure deferred component updates have been sent to the rendering thread
            // haker: MarkRenderTransformDirty() explanation with SendAllEndOfFrameUpdate
            // - see UActorComponent::MarkRenderTransformDirty (goto 001: SceneRenderer)
            // - see UWorld::SnedAllEndOfFrameUpdates (goto 005: SceneRenderer)
            for (UWorld* World : WorldsToEOFUpdate)
            {
                World->SendAllEndOfFrameUpdates();
            }

            // haker: iterate contexts (in our case, PieContext is expected)
            for (auto ContextIt = WorldList.CreateIterator(); ContextIt; ++ContextIt)
            {
                FWorldContext& PieContext = *Context.It;
                if (PieContext.WorldType != EWorldType::PIE)
                {
                    continue;
                }

                PlayWorld = PieContext.World();
                GameViewport = PieContext.GameViewport;

                // render playworld:
                // haker: to render scene, UWorld and UGameViewportClient are necessary!
                if (PlayWorld && GameViewport)
                {
                    // use the PlayWorld as the GWorld, because who knows what will happen in the Tick
                    // haker: see SetPlayInEditorWorld briefly
                    // - see the 'RestoreEditorWorld' also
                    // - these function take care of maintaining the GWorld's consistency
                    UWorld* OldGWorld = SetPlayInEditorWorld(PlayWorld);

                    // render everything
                    // haker: the scene draw call is started from GameViewportClient's Viewport 
                    // - FViewport::Draw (goto 010: SceneRenderer)
                    GameViewport->Viewport->Draw();

                    // pop the world
                    RestoreEditorWorld(OldGWorld);
                }
            }
        }

        // 041 - Foundation - SceneRenderer * - END
    }

    // 003 - Foundation - CreateInnerProcessPIEGameInstance * - UEditorEngine::CreatePIEWorldByDuplication
    virtual UWorld* CreatePIEWorldByDuplication(FWorldContext& WorldContext, UWorld* InWorld, FString& PlayWorldMapName) override
    {
        // haker: from EditorWorld, reuse its properties (e.g. PackageName, LoadedPath, Guid, ...)
        UPackage* InPackage = InWorld->GetOutermost();
        const FString WorldPackageName = InPackage->GetName();

        // preserve the old path keeping EditorWorld name the same
        PlayWorldMapName = UWorld::ConvertToPIEPackageName(WorldPackageName, WorldContext, PIEInstance);

        // haker: for new PIE world, create new package
        UPackage* PlayWorldPackage = CreatePackage(*PlayWorldMapName);
        PlayWorldPackage->SetPIEInstanceID(WorldContext.PIEInstance);
        PlayWorldPackage->SetLoadedPath(InPackage->GetLoadedPath());
        PlayWorldPackage->SetGuid(InPackage->GetGuid());
        PlayWorldPackage->MarkAsFullyLoaded();

        // Add PKG_NewlyCreated flag to this package so we don't try to resolve its linker as it is unsaved duplicated world package 
        // haker: note that we mark the package as PIE with PKG_PlayInEditor
        PlayWorldPackage->SetPackageFlags(PKG_PlayInEditor | PKG_NewlyCreated);

        // haker: now create new PIE world
        UWorld* NewPIEWorld = NULL;
        {
            // NULL GWorld before various PostLoad functions are called, this makes it easier to debug invalid GWorld accesses
            GWorld = NULL;

            // duplicate the editor world to create the PIE world
            // see UWorld::GetDuplicateWorldForPIE (goto 004: CreateInnerProcessPIEGameInstance)
            NewPIEWorld = UWorld::GetDuplicatedWorldForPIE(InWorld, PlayWorldPackage, WorldContext.PIEInstance);
        }

        NewPIEWorld->SetFeatureLevel(InLevel->GetFeatureLevel());

        // haker: we should mark NewPIEWorld as EWorldType::PIE
        NewPIEWorld->WorldType = EWorldType::PIE;

        // haker: let's understand CreatePIEWorldByDuplication() with the picture:
        //                                                                              
        //                                    *** 1. Create PIEWorld's package object   
        //                                            ┌───────────────────┐             
        //       EditorWorld's Package                │ PIE World Package │             
        //           ▲                                └────────┬──────────┘             
        //           │                                         │                        
        //        OuterMost()                    ┌─────────────┘                        
        //           │                           │                                      
        //     ┌─────┴───────┐                   ▼                 ┌─────────────┐      
        //     │ EditorWorld │    ────StaticDuplicateObject()────► │ NEW PIEWorld│      
        //     └─────────────┘                                     └─────────────┘      
        //                        *** 2. Duplicate Editor World                         
        //                                with PIE World Package                        
        //

        return NewPIEWorld;
    }

    // 005 - Foundation - CreateInnerProcessPIEGameInstance * - UEditorEngine::PostCreatePIEWorld
    virtual void PostCreatePIEWorld(UWorld* InWorld) override
    {
        // make sure we can clean up this world
        // haker: as RF_Standalone describes, in the PIE world, we don't need to keep unreferenced objects
        NewPIEWorld->ClearFlags(RF_Standalone);

        // init the PIE world
        // haker: we covered InitWorld() already!~ :)
        NewPIEWorld->InitWorld();
    }

    /** create an GameInstance with the given settings. a window is created if this isn't server */
    UGameInstance* CreateInnerProcessPIEGameInstance(FRequestPlaySessionParams& InParams, const FGameInstancePIEParameters& InPIEParameters, int32 InPIEInstanceIndex)
    {
        // 000 - Foundation - CreateInnerProcessPIEGameInstance * - BEGIN
    
        // create a GameInstance for this new instance
        FSoftClassPath GameInstanceClassName = GetDefault<UGameMapsSettings>()->GameInstanceClass;

        // if an invalid class type was specified we fall back to the default
        UClass* GameInstanceClass = GameInstanceClassName.TryLoadClass<UGameInstance>();
        if (!GameInstanceClass)
        {
            GameInstanceClass = UGameInstance::StaticClass();
        }

        // haker: note that new GameInstance has its outer as UEditorEngine
        UGameInstance* GameInstance = NewObject<UGameInstance>(this, GameInstanceClass);

        // we need to temporarily add the GameInstance to the root because the InitializeForPlayInEditor call can do garbage collection wiping out the GameInstance
        // haker: GameInstance can be GC'ed in InitializeForPlayInEditor, so call AddToRoot() to prevent from GC'ing
        // - in the next line, you can see the function, InitializeForPlayInEditor() which calls GC' in its scope
        // - see the below where GameInstance calls RemoveFromRoot() at the end line of this function
        GameInstance->AddToRoot();

        // attempt to initialize the GameInstance; this will construct the world
        // haker: InPIEInstanceIndex is normally 0 for level-viewport
        // see UGameInstance::InitializeForPlayInEditor (goto 001: CreateInnerProcessPIEGameInstance)
        const FGameInstancePIEResult InitializeResult = GameInstance->InitializeForPlayInEditor(InPIEInstanceIndex, InPIEParameters);
        if (!InitializeResult.IsSuccess())
        {
            GameInstance->RemoveFromRoot();
            return nullptr;
        }

        // our game instance was successfully created
        // haker: we successfully create game instance, so we can derive the world connected to new game instance
        FWorldContext* const PieWorldContext = GameInstance->GetWorldContext();
        PlayWorld = PieWorldContext->World();

        // haker: let's look through class headers (declarations) to understand the relationships between classes which we'll cover further
        // 1. UGameViewportClient (goto 006: CreateInnerProcessPIEGameInstance)
        // 2. UGameInstance (goto 007: CreateInnerProcessPIEGameInstance)
        // 3. AGameModeBase (goto 016: CreateInnerProcessPIEGameInstance)
        // - now we covered main classes for our CreateInnerProcessPIEGameInstance() 

        // haker: from here, we prepared the followings:
        // 1. GameInstance
        // 2. PIEWorld from UGameInstance::InitializeForPlayInEditor() by duplicating EditorWorld

        // initialize a local player and viewport-client for non-dedicated server instances
        // haker: create ViewportClient and LocalPlayer
        UGameViewportClient* ViewportClient = nullptr;
        ULocalPlayer* NewLocalPlayer = nullptr;
        {
            // create an instance of the GameViewportClient, with the class specified by the engine
            ViewportClient = NewObject<UGameViewportClient>(this, GameViewportClientClass);

            // haker: skip the detail of UGameViewportClient's initialization
            // - by the initializations, we'll connect to platform API like getting input events
            ViewportClient->Init(*PreWorldContext, GameInstance);

            GameViewport = ViewportClient;
            GameViewport->bIsPlayInEditorViewport = true;

            // update our WorldContext to know which ViewportClient is associated
            // haker: the connection between world and ViewportClient is maintained in WorldContext
            PieWorldContext->GameViewport = ViewportClient;

            // add a callback for Game Input that isn't absorbed by the GameViewport:
            // this allows us to make editor commands work (such as Shift+F1 etc) from within PIE

            // haker: as you can see, UGameViewportClient take an role of input binding
            ViewportClient->OnGameViewportInputKey().BindUObject(this, &UEditorEngine::ProcessDebuggerCommands);

            // attempt to initialize a local player
            // haker: we create LocalPlayer by calling UGameViewportClient::SetupInitialLocalPlayer()
            // see SetupInitialLocalPlayer (goto 018: CreateInnerProcessPIEGameInstance)
            FString Error;
            NewLocalPlayer = ViewportClient->SetupInitialLocalPlayer(Error);

            // broadcast that the viewport has been successfully created
            UGameViewportClient::OnViewportCreated().Broadcast();
        }

        // by this point it is safe to remove GameInstance from the root and allow it to garbage collected as per usual 
        // haker: from here, no more worry about GC'ing:
        // - in other words, we are not calling GarbageCollect() anymore until the function is finished
        GameInstance->RemoveFromRoot();

        // haker: now we successfully create all necessary objects for PIE:
        // - it's time to initialize 'GameInstance' to play a game
        // - see UGameInstance::StartPlayInEditorGameInstance (goto 021: CreateInnerProcessPIEGameInstance)
        FGameInstancePIEResult StartResult = FGameInstancePIEResult::Success();
        {
            StartResult = GameInstance->StartPlayInEditorGameInstance(NewLocalPlayer, InPIEParameters);
        }

        //...

        // 068 - Foundation - CreateInnerProcessPIEGameInstance - END

        return GameInstance;
    }

    /** a pointer to a UWorld that is the duplicated/saved-loaded to be played with "Play From Here" */
    TObjectPtr<class UWorld> PlayWorld;

    TSubclassOf<class UGameViewportClient> GameViewportClientClass;

    /** the viewport representing the current game instance; can be 0 so don't use without checking */
    TObjectPtr<class UGameViewportClient> GameViewport;

    /** when simulating in editor, a pointer to the original (non-simulating) editor world */
    TObjectPtr<class UWorld> EditorWorld;
};