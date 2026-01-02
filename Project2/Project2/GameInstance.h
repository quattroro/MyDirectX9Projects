
/**
 * GameInstance: high-level manager object for an instance of the running game
 * - spawned at game creation and not destroyed until game instance is shut down
 * - running as a standalone game, there will be one of these
 * - running in PIE (play-in-editor) will generate one of these per PIE instance
 */
// 007 - Foundation - CreateInnerProcessPIEGameInstance * - UGameInstance
// haker: we need to answer these questions:
// 1. what is GameInstance?
//    - when we think of Unreal Engine as OS, then GameInstance is a 'process' in OS
//    - we can also interpret GameInstance as Game + Instance
//      -> for each game, one game instance exists!
//    - see GameInstance's member variables (goto 008: CreateInnerProcessPIEGameInstance)
//
// 2. what is relationship between World and GameInstance?
// 3. what is difference between GameInstance and UGameViewportClient?
UCLASS(config=Game, transient, BlueprintType, Blueprintable, MinimalAPI)
class UGameInstance : public UObject, public FExec
{
    GENERATED_UCLASS_BODY()

    // haker: UGameInstance's OuterPrivate is UEngine
    UEngine* GetEngine() const
    {
        return CastChecked<UEngine>(GetOuter());
    }

    UWorld* GetWorld() const
    {
        return WorldContext ? WorldContext->World() : NULL;
    }

    /** Opportunity for blueprints to handle the game instance being initialized. */
    UFUNCTION(BlueprintImplementableEvent, meta=(DisplayName = "Init"))
    void ReceiveInit();

    /** virtual function to allow custom GameInstance an opportunity to set up what it needs */
    virtual void Init()
    {
        // haker: blueprint event call for Init()
        ReceiveInit();

        //...

        SubsystemCollection.Initialize(this);
    }

    /** called to initialize the game instance for PIE instances of the game */
    // 001 - Foundation - CreateInnerProcessPIEGameInstance * - UGameInstance::InitializeForPlayInEditor
    virtual FGameInstancePIEResult InitializeForPlayInEditor(int32 PIEInstanceIndex, const FGameInstancePIEParameters& Params)
    {
        FWorldDelegates::OnPIEStarted.Broadcast(this);

        // haker: see GetEngine briefly:
        // to derive UEngine(UEditorEngine), it uses GetOuter():
        UEditorEngine* const EditorEngine = CastChecked<UEditorEngine>(GetEngine());

        // look for an existing pie world context, may have been created before
        // haker: as I said, PIEInstanceIndex is noramlly zero(0)
        // - see GetWorldContextFromPIEInstance (goto 002: CreateInnerProcessPIEGameInstance)
        WorldContext = EditorEngine->GetWorldContextFromPIEInstance(PIEInstanceIndex);
        if (!WorldContext)
        {
            // if not, create a new one
            // haker: we already covered this function~ :)
            WorldContext = &EditorEngine->CreateNewWorldContext(EWorldType::PIE);
            WorldContext->PIEInstance = PIEInstanceIndex;
        }

        // haker: mark our GameInstance to WorldContext
        // - WorldContext is cached in UGameInstance also
        WorldContext->OwningGameInstance = this;

        // we always need to create a new PIE world unless we're using the editor world for SIE
        UWorld* NewWorld = nullptr;
        bool bNeedsGarbageCollection = false;
        
        // standard PIE path: just duplicate the EditorWorld
        // haker: note that we duplicate the world for PIE world context (does NOT reuse)
        // see UEditorEngine::CreatePIEWorldByDuplication (goto 003: CreateInstanceProcessPIEGameInstance)
        NewWorld = EditorEngine->CreatePIEWorldByDuplication(*WorldContext, EditorEngine->EditorWorld, PIEMapName);

        // duplication can result in unreferenced objects, so indicate that we should do a GC pass after initializing the world context
        // haker: as we saw, in CreatePIEWorldByDuplication, we call StaticDuplicateObjectEx
        // - StaticDuplicateObjectEx causes unused objects to be GC'ed
        bNeedsGarbageCollection = true;

        // haker: see briefly SetCurrentWorld()
        WorldContext->SetCurrentWorld(NewWorld);

        // haker: if NewWorld is immutable to be GC'ed, GameInstance can be also immutable to be GC'ed
        // - I didn't look into the detail how GC works on these objects, which will be tossing to you~ :)
        NewWorld->SetGameInstance(this);

        // haker: note that how to call GC in unreal engine
        // - this is why we call AddToRoot() for UGameInstance
        if(bNeedsGarbageCollection)
        {
            CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);
        }

        // this creates the game instance subsystems
        // haker: see briefly UGameInstance::Init()
        Init();

        // initialize the world after setting world context and initializing the game instance to be consistent with normal loads
        // this creates the world subsystems and prepares to begin play

        // haker: need to init newly created UWorld
        // see UEditorEngine::PostCreatePIEWorld (goto 005: CreateInnerProcessPIEGameInstance)
        EditorEngine->PostCreatePIEWorld(NewWorld);

        return FGameInstancePIEResult::Success();
    }

    FWorldContext* GetWorldContext() const { return WorldContext; };

    UGameViewportClient* GetGameViewportClient() const
    {
        FWorldContext* const WC = GetWorldContext();
        return WC ? WC->GameViewport : nullptr;
    }

    /** adds a LocalPlayer to the local and global list of Players */
    virtual int32 AddLocalPlayer(ULocalPlayer* NewLocalPlayer, FPlatformUserId UserId)
    {
        if (NewLocalPlayer == nullptr)
        {
            return INDEX_NONE;
        }

        const int32 InsertIndex = LocalPlayers.AddUnique(NewLocalPlayer);
        return InsertIndex;
    }

    /** add a new player */
    ULocalPlayer* CreateLocalPlayer(FPlatformUserId UserId, FString& OutError, bool bSpawnPlayerController)
    {
        ULocalPlayer* NewPlayer = NULL;

        // haker: first we get the GameViewportClient from GameInstance
        // - how? WorldContext has GameViewport!
        int32 InsertIndex = INDEX_NONE;
        UGameViewportClient* GameViewport = GetGameViewportClient();
        check(GameViewport);

        // haker: when we create ULocalPlayer, please note its parameters:
        // 1. OuterPrivate as 'EditorEngine'
        // 2. LocalPlayer's Class is resided in 'Engine'
        NewPlayer = NewObject<ULocalPlayer>(GetEngine(), GetEngine()->LocalPlayerClass);

        // haker: now new LocalPlayer is cached in GameInstance as we saw in the diagram:
        // see AddLocalPlayer briefly:
        //      GameInstance      
        //      │                         
        //      └──LocalPlayers           
        //          :TArray<ALocalPlayer> 
        InsertIndex = AddLocalPlayer(NewPlayer, UserId);

        return NewPlayer;  
    }

    // 020 - Foundation - CreateInnerProcessPIEGameInstance * - CreateInitialPlayer
    virtual ULocalPlayer* CreateInitialPlayer(FString& OutError)
    {
        // haker: goto CreateLocalPlayer()
        return CreateLocalPlayer(IPlatformInputDeviceMapper::Get().GetPrimaryPlatformUser(), OutError, false);
    }

    /** call to create the game mode for a given map URL */
    // 023 - Foundation - CreateInnerProcessPIEGameInstance * - UGameInstance::CreateGameModeForURL
    virtual AGameModeBase* CreateGameModeForURL(FURL InURL, UWorld* InWorld)
    {
        // init the game info
        FString Options;
        FString GameParam;
        for (int32 i = 0; i < InURL.Op.Num(); ++i)
        {
            Options += TEXT("?");
            Options += InURL.Op[i];
            FParse::Value(*InURL.Op[i], TEXT("GAME="), GameParam);
        }

        UWorld* World = InWorld ? InWorld : GetWorld();

        // haker: do you remember world setting is in UWorld's persistent level?
        // - in WorldSetting, the GameMode class is defined
        // - it is not always the case who has GameModeClass in WorldSetting!
        AWorldSettings* Settings = World->GetWorldSettings();

        // get the GameMode class
        TSubclass<AGameModeBase> GameClass = Settings->DefaultGameMode;

        // spawn the GameMode
        FActorSpawnParameters SpawnInfo;
        SpawnInfo.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        SpawnInfo.ObjectFlags |= RF_Transient;
        return World->SpawnActor<AGameModeBase>(GameClass, SpawnInfo);
    }

    /** called to actually start the game when doing play/simulate in editor */
    // 021 - Foundation - CreateInnerProcessPIEGameInstance * - StartPlayInEditorGameInstance
    virtual FGameInstancePIEResult StartPlayInEditorGameInstance(ULocalPlayer* LocalPlayer, const FGameInstancePIEParameters& Params)
    {
        UEditorEngine* const EditorEngine = CastChecked<UEditorEngine>(GetEngine());
        {
            // we're going to be playing in the current world, get it ready for play
            UWorld* const PlayWorld = GetWorld();

            // make URL
            // haker: in URL, it contains Map we starting on (PIEWorld)
            FURL URL;
            {
                //...
            }

            // haker: we create GameMode here
            // see UWorld::SetGameMode (goto 022: CreateInnerProcessPIEGameInstance)
            PlayWorld->SetGameMode(URL);

            // haker: create/initialize levels' actors
            // - see UWorld::InitializeActorsForPlay (goto 024: CreateInnerProcessPIEGameInstance)
            PlayWorld->InitializeActorsForPlay(URL);

            if (LocalPlayer)
            {
                FString Error;

                // haker: now try to spawn APlayerController for LocalPlayer
                // see ULocalPlayer::SpawnPlayActor (goto 027: CreateInnerProcessPIEGameInstance)
                if (!LocalPlayer->SpawnPlayActor(URL.ToString(1), Error, PlayWorld))
                {
                    return FGameInstancePIEResult::Failure(FText::Format(NSLOCTEXT("UnrealEd", "Error_CouldntSpawnPlayer", "Couldn't spawn player: {0}"), FText::FromString(Error)));
                }
            }

            // haker: now finally we reach to the UWorld::BeginPlay()!
            // - after SpawnPlayerActor() is finished, we are ready to call UWorld::BeginPlay()
            // - see UWorld::BeginPlay (goto 063: CreateInnerProcessPIEGameInstance)
            PlayWorld->BeginPlay();
        }
    }

    // 008 - Foundation - CreateInnerProcessPIEGameInstance * - UGameInstance member variables

    // haker: PIEMapName is updated on UEditorEngine::CreatePIEWorldByDuplication()
    FString PIEMapName;

    // haker: the WorldContext where the GameInstance is currently dependent on world
    // - we need to understand the relation between GameInstance and World:
    //                                                                                
    // UEngine             In Frame0, GameInstance is dependent                       
    //  │                   on WorldContext0                      ┌─Frame0──────────┐ 
    //  ├───WorldContext0◄────────────────────────────────────────┤   GameInstance  │ 
    //  │                                                         └─────────────────┘ 
    //  ├───WorldContext1                                                             
    //  │                                                         ┌─Frame1──────────┐ 
    //  └───WorldContext2◄────────────────────────────────────────┤   GameInstance  │ 
    //                     In Frame1, GameInstance is dependent   └─────────────────┘ 
    //                      on WorldContext1                                          
    //                                                                                
    // - Actually, GameInstance has "indirect" relation to world by introducing WorldContext
    FWorldContext* WorldContext;

    /** list of locally participating players in this game instance */
    // haker: GameInstance manages 'players'
    // - see ULocalPlayer (goto 009: CreateInnerProcessPIEGameInstance)
    TArray<TObjectPtr<ULocalPlayer>> LocalPlayers;

    // haker: like UWorld, UGameInstance has its own subsystems
    FObjectSubsystemCollection<UGameInstanceSubsystem> SubsystemCollection;
};