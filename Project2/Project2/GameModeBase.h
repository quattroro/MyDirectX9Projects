/**
 * Info is the base class of an Actor that isn't meant to have a physical representation in the world,
 * used primarily for "manager" type classes that hold settings data about the world, but might need to be an Actor
 * for replication purposes
 */
class AInfo : public AActor
{

};

class ANavigationObjectBase : public AActor, public INavAgentInterface
{

};

/** this class indicates a location where a player can spawn when the game begins */
class APlayerStart : public ANavigationObjectBase
{
    /** used when searching for which playerstart to use */
    FName PlayerStartTag;
};

/**
 * the GameModeBase defines the game being played. it governs the game rules, scoring, what actors
 * are allowed to exist in this game type, and who may enter the game.
 * 
 * it is only instanced on the server and will never exist on the client
 * 
 * GameModeBase actor is instantiated when the level is initialized for gameplay in C++ UGameEngine::LoadMap()
 * 
 * the class of this GameMode actor is determined by (in order) either the URL ?game=xxx,
 * the GameMode Override value set in the World Settings, or the DefaultGameMode entry set
 * in the game's Project Settings
 */
// 016 - Foundation - CreateInnerProcessPIEGameInstance * - AGameModeBase
// haker: you can think of 'GameMode' as the rule of game:
// - AGameModeBase is spawned in the world:
//   - in standalone game, it will be spawned in client, but in dedicated server, it is only spawned in the server:
//   - it is natural to spawn only in the server, considering that it contains rules of game
//   - e.g. Player's PawnClass, GameState, ...
// - see UWorld::AuthorityGameMode:
//   - the world has GameMode in 'standalone', but in 'dedicated' server/client case, it has different principle:
//     - see the diagram:
//                                                                                                                    
//      ┌─Dedicated Server──────────────────────┐                                  ┌─Client0────────────────────────┐ 
//      │                                       │                                  │                                │ 
//      │                                       │                                  │ World                          │ 
//      │ World                                 │                                  │  │                             │ 
//      │  │                                    │                  ┌───────────────┼─►├──GameState                  │ 
//      │  ├──AuthorityGameMode:AGameModeBase   │                  │               │  │                             │ 
//      │  │   │                                │                  │               │  └──AuthorityGameMode:nullptr  │ 
//      │  │   └──GameState:GameStateBase       │                  │               │                                │ 
//      │  │                                    │                  │               └────────────────────────────────┘ 
//      │  │                                    │                  │                                                  
//      │  │                                    │**replicates**    │                                                  
//      │  └──GameState:GameStateBase ──────────┼──────────────────┤               ┌─Client1────────────────────────┐ 
//      │     :replicated purpose               │                  │               │                                │ 
//      │                                       │                  │               │ World                          │ 
//      └───────────────────────────────────────┘                  │               │  │                             │ 
//                                                                 └───────────────┼─►├──GameState                  │ 
//                                                                                 │  │                             │ 
//                                                                                 │  └──AuthorityGameMode:nullptr  │ 
//                                                                                 │                                │ 
//                                                                                 └────────────────────────────────┘ 
//    
// - see AGameModeBase's member variables (goto 017: CreateInnerProcessPIEGameInstance)                                                                                                                
class AGameModeBase : public AInfo
{
    /** transitions to call BeginPlay() on actors */
    // 064 - Foundation - CreateInnerProcessPIEGameInstance - AGameModeBase::StartPlay
    virtual void StartPlay()
    {
        // haker: GameState?
        // - see AGameStateBase::HandleBeginPlay (goto 065: CreateInnerProcessPIEGameInstance)
        GameState->HandleBeginPlay();
    }

    virtual APawn* SpawnDefaultPawnAtTransform_Implementation(AController* NewPlayer, const FTransform& SpawnTransform) override
    {
        FActorSpawnParameters SpawnInfo;
        SpawnInfo.Instigator = GetInstigator()
        SpawnInfo.ObjectFlags |= RF_Transient;

        // haker: using GetDefaultPawnClassForController_Implementation, get Pawn class and spawn Pawn with Pawn class
        UClass* PawnClass = GetDefaultPawnClassForController(NewPlayer);
        APawn* ResultPawn = GetWorld()->SpawnActor<APawn>(PawnClass, SpawnTransform, SpawnInfo);
        return ResultPawn;
    }
        
    // 046 - Foundation - CreateInnerProcessPIEGameInstance - AGameModeBase::SpawnDefaultPawnFor_Implementation
    virtual APawn* SpawnDefaultPawnFor_Implementation(AController* NewPlayer, AActor* StartSpot) override
    {
        // don't allow pawn to be spawned with any pitch or roll
        // haker: note that we only allow 'Yaw' on our StartRotation
        FRotator StartRotation(ForceInit);
        StartRotation.Yaw = StartSpot->GetActorRotation().Yaw;

        // haker: the New Pawn's location follows StartSpot(APlayerStart's location)
        FVector StartLocation = StartSpot->GetActorLocation();
        
        FTransform Transform = FTransform(StartRotation, StartLocation);

        // haker: see PawnDefaultPawnAtTransform_Implementation
        return SpawnDefaultPawnAtTransform(NewPlayer, Transform);
    }

    virtual UClass* GetDefaultPawnClassForController_Implementation(AController* InController) override
    {
        // haker: AGameModeBase controls game's rule by overriding gameplay actor's classes
        // - by overriding DefaultPawnClass, it controls game-rule of Pawn controlled by Player
        return DefaultPawnClass;
    }

    /** Handles second half of RestartPlayer */
    // 050 - Foundation - CreateInnerProcessPIEGameInstance - AGameModeBase::FinishRestartPlayer
    virtual void FinishRestartPlayer(AController* NewPlayer, const FRotator& StartRotation)
    {
        // haker: possess newly created Pawn:
        // - by AttachToPawn, PC cached possessed Pawn, so we can get Pawn by GetPawn()
        // - see AController::Possess (goto 051: CreateInnerProcessPIEGameInstance)
        NewPlayer->Possess(NewPlayer->GetPawn());

        // if the Pawn is destroyed as part of possession we have to abort
        // haker: if we successly possess the Pawn, we are getting into else-statement
        if (!IsValid(NewPlayer->GetPawn()))
        {
            //...
        }
        else
        {
            // set initial control rotation to starting rotation 
            // haker: the Pawn is done for possessing, it's time to make PC to follow Pawn's rotation
            // - see AController::ClientSetRotation_Implementation (goto 061: CreateInnerProcessPIEGameInstance)
            NewPlayer->ClientSetRotation(NewPlayer->GetPawn()->GetActorRotation, true);

            // haker: it is worthy to note that initial controller's rotation doesn't accept Roll!
            // - reapply PlayerController's control rotation from the parameter, 'StartRotation'
            FRotator NewControllerRot = StartRotation;
            NewControllerRot.Roll = 0.f;
            NewPlayer->SetControlRotation(NewControllerRot);

            // haker: do you guess the difference between ClientSetRotation and SetControlRotation?
            //                                                                                       
            //                      SetControlRotation(): update ControlRotation                     
            //                  ┌───────┐                                                            
            //                  │       │                                                            
            //  ┌───────────────▼──┐    │                                                            
            //  │ PlayerController ├────┤                                                            
            //  └──┬───────────────┘    │                                                            
            //     │                    │                                                            
            //     └──Pawn ◄────────────┘                                                            
            //                      SetClientRotation(): apply ControlRotation to Possessed Pawn     
            //                                                                    ──────────────     
            //                                                                      ***                          
        }
    }

    /** tries to spawn the player's pawn at the specified actor's location */
    // 045 - Foundation - CreateInnerProcessPIEGameInstance - AGameModeBase::RestartPlayerAtPlayerStart
    // haker: as comment says, this function spawn new Pawn to possess on PlayerSpot
    virtual void RestartPlayerAtPlayerStart(AController* NewPlayer, AActor* StartSpot)
    {
        FRotator SpawnRotation = StartSpot->GetActorRotation();

        // haker: see GetDefaultPawnClassForController_Implementation briefly
        if (GetDefaultPawnClassForController(NewPlayer) != nullptr)
        {
            // haker: see SpawnDefaultPawnFor_Implementation (goto 046: CreateInnerProcessPIEGameInstance)
            APawn* NewPawn = SpawnDefaultPawnFor(NewPlayer, StartSpot);
            if (IsValid(NewPawn))
            {
                // haker: see AController::SetPawn (goto 047: CreateInnerProcessPIEGameInstance)
                NewPlayer->SetPawn(NewPawn);
            }
        }

        // haker: now it's time to possess Pawn onto PC
        // - see FinishRestartPlayer (goto 050: CreateInnerProcessPIEGameInstance)
        FinishRestartPlayer(NewPlayer, SpawnRotation);
    }

    /** tries to spawn the player's pawn, at the location returned by FindPlayerStart */
    // 044 - Foundation - CreateInnerProcessPIEGameInstance - AGameModeBase::RestartPlayer
    virtual void RestartPlayer(AController* NewPlayer)
    {
        if (NewPlayer == nullptr || NewPlayer->IsPendingKillPending())
        {
            return;
        }

        // haker: to spawn our Pawn to possess onto PlayerController, find APlayerStart
        // see FindPlayerStart_Implementation breifly
        AActor* StartSpot = FindPlayerStart(NewPlayer);
        if (StartSpot == nullptr)
        {
            // check for a previously assigned spot
            if (NewPlayer->StartSpot != nullptr)
            {
                StartSpot = NewPlayer->StartSpot.Get();
            }
        }

        // haker: see RestartPlayerAtPlayerStart (goto 045: CreateInnerProcessPIEGameInstance)
        RestartPlayerAtPlayerStart(NewPlayer, StartSpot);

        // *** haker: can you understand what 'RestartPlayer' means?
        // 1. spawn NewPawn to possess by NewPlayer(NewPC)
        // 2. do NewPC's possession toward NewPawn
        //    - (if NewPC already possess, call UnPossess() first)
        //    - AttachToPawn()
        //    - Restart():
        //      - connecting 'Camera' and 'Input' of Pawn to PlayerController
        //
        // Questions:
        // - Can we say Input and Camera is world component or non-world component?
        //   - it is in gray-area, we can't say Camera or Input is world-component definitely...
        //   - so PlayerController has PlayerCameraManager and PlayerInput!
    }

    virtual bool PlayerCanRestart_Implementation(APlayerController* Player) override
    {
        if (Player == nullptr || Player->IsPendingKillPending())
        {
            return false;
        }

        // ask the player controller if it's ready to restart as well
        // haker: see APlayerController::CanRestartPlayer() briefly:
        // - the existance of PlayerState is important
        return Player->CanRestartPlayer();
    }

    virtual void HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer) override
    {
        // haker: see PlayerCanRestart_Implementation
        if (PlayerCanRestart(NewPlayer))
        {
            // see RestartPlayer (goto 044: CreateInnerProcessPIEGameInstance)
            RestartPlayer(NewPlayer);
        }

        // haker: simulate by LyraProject:
        // [ ] see sequence diagram of 'Lyra Experience Loading' (see Foundation/Images/LyraExperienceLoading.png)
        // [ ] see ALyraGameMode::HandleStartingNewPlayer_Implementation()
        // - using the sequence diagram, debug how the 'Experience' in Lyra is working~ :)
    }

    /** called after a successful login: this is the first place it is safe to call replicated functions on the PlayerController */
    // 043 - Foundation - CreateInnerProcessPIEGameInstance - AGameModeBase::PostLogin
    virtual void PostLogin(APlayerController* NewPlayer)
    {
        //...

        // now that initialization is done, try to spawn the player's pawn and start match
        // haker: see HandlingStartingNewPlayer_Implementation
        HandleStartingNewPlayer(NewPlayer);
    }

    /**
     * spawns the appropriate PlayerController for the given options: split out from Login() for easier overriding
     * override this to conditionally spawn specialized PlayerControllers, for instance 
     */
    // 030 - Foundation - CreateInnerProcessPIEGameInstance * - AGameModeBase::SpawnPlayerController
    virtual APlayerController* SpawnPlayerController(ENetRole InRemoteRole, const FString& Options)
    {
        // haker: I remove wrapper function calls
        FActorSpawnParameters SpawnInfo;
        SpawnInfo.Instigator = GetInstigator();

        // haker: note that we enable 'bDeferConstruction'
        SpawnInfo.bDeferConstruction = true;

        // we never want to save player controllers into a map
        // haker: from here, we can assume that PC is not for world (cuz, it is not serialized)
        SpawnInfo.ObjectFlags |= RF_Transient;

        // 000 - Foundation - SpawnActor - BEGIN
        // see SpawnActor (goto 001: SpawnActor)
        APlayerController* NewPC = GetWorld()->SpawnActor<APlayerController>(PlayerControllerClass, FVector::ZeroVector, FRotator::ZeroRotator, SpawnInfo);
        // 013 - Foundation - SpawnActor - END
        if (NewPC)
        {
            // haker: because we enable 'bDeferConstruction', we need to make sure to call FinishSpawning() on NewPC(PlayerController)
            if (InRemoteRole == ROLE_SimulatedProxy)
            {
                // this is a local player because it has no authority/autonomous remote role
                NewPC->SetAsLocalPlayerController();
            }

            // haker: where is PlayerController added/managed?
            // - the FinishSpawning() calls PostInitializeComponents()
            // - PostInitializeComponents() has the answer
            //   - see briefly where PostInitializeComponent() is called
            // see APlayerController::PostInitializeComponents (goto 031: CreateInnerProcessPIEGameInstance)
            NewPC->FinishSpawning(FTransform(FVector::ZeroVector, FRotator::ZeroRotator), false, nullptr, ESpawnActorScaleMethod::MultiplyWithRoot);
        }

        return NewPC;
    }

    /** return true if FindPlayerStart should use the StartSpot stored on Player instead of calling ChoosePlayerStart */
    virtual bool ShouldSpawnAtStartSpot(AController* Player)
    {
        return (Player != nullptr && Player->StartPos != nullptr);
    }

    virtual AActor* ChoosePlayerStart_Implementation(AController* Player)
    {
        // choose a player start
        APlayerStart* FoundPlayerStart = nullptr;

        // haker: GetDefaultPawnClassForController_Implementation()
        UClass* PawnClass = GetDefaultPawnClassForController(Player);

        APawn* PawnToFit = PawnClass ? PawnClass->GetDefaultObject<APawn>() : nullptr;

        TArray<APlayerStart*> UnOccupiedStartPoints;
        TArray<APlayerStart*> OccupiedStartPoints;

        UWorld* World = GetWorld();
        for (TActorIterator<APlayerStart> It(World); It; ++It)
        {
            APlayerStart* PlayerStart = *It;
            if (PlayerStart->IsA<APlayerStartPIE>())
            {
                // always prefer the first "Player from Here" PlayerStart, if we find one while in PIE mode
                FoundPlayerStart = PlayerStart;
                break;
            }
            else
            {
                FVector ActorLocation = PlayerStart->GetActorLocation();
                const FRotator ActorRotation = PlayerStart->GetActorRotation();
                if (!World->EncroachingBlockingGeometry(PawnToFit, ActorLocation, ActorRotation))
                {
                    UnOccupiedStartPoints.Add(PlayerStart);
                }
                else if (World->FindTeleportSpot(PawnToFit, ActorLocation, ActorRotation))
                {
                    OccupiedStartPoints.Add(PlayerStart);
                }
            }
        }

        if (FoundPlayerStart == nullptr)
        {
            if (UnOccupiedStartPoints.Num() > 0)
            {
                FoundPlayerStart = UnOccupiedStartPoints[FMath::RandRange(0, UnOccupiedStartPoints.Num() - 1)];
            }
            else if (OccupiedStartPoints.Num() > 0)
            {
                FoundPlayerStart = OccupiedStartPoints[FMath::RandRange(0, OccupiedStartPoints.Num() - 1)];
            }
        }

        return FoundPlayerStart;
    }

    AActor* FindPlayerStart_Implementation(AController* Player, const FString& IncomingName)
    {
        UWorld* World = GetWorld();

        // if incoming start is specified, then just use it
        if (!IncomingName.IsEmpty())
        {
            const FName IncomingPlayerStartTag = FName(*IncomingName);
            for (TActorIterator<APlayerStart> It(World); It; ++It)
            {
                APlayerStart* Start = *It;
                if (Start && Start->PlayerStartTag == IncomingPlayerStartTag)
                {
                    return Start;
                }
            }
        }

        // always pick StartSpot at start of match
        if (ShouldSpawnAtStartSpot(Player))
        {
            if (AActor* PlayerStartSpot = Player->StartSpot.Get())
            {
                return PlayerStartSpot;
            }
        }

        // haker: ChoosePlayerStart_Implementation()
        // we are not going to see this in detail, I'll leave it you~ :)
        AActor* BestStart = ChoosePlayerStart(Player);
        if (BestStart == nullptr)
        {
            // this is a bit odd, but there was a complex chunk of code that in the end always resulted in this, so we may as well just
            // short cut it down to this. basically we are saying spawn at 0,0,0 if we didn't find a proper player start
            BestStart = World->GetWorldSettings();
        }

        return BestStart;
    }

    /** attempt to initialize the 'StartSpot' of the Player */
    // 035 - Foundation - CreateInnerProcessPIEGameInstance * - AGameModeBase::UpdatePlayerStartSpot
    virtual bool UpdatePlayerStartSpot(AController* Player, const FString& Portal, FString& OutErrorMessage)
    {
        OutErrorMessage.Reset();

        // haker: FindPlayerStart_Implementation()
        // - see briefly FindPlayerStart_Implementation()
        // - I'll leave this part to you~ :)
        AActor* const StartSpot = FindPlayerStart(Player, Portal);
        if (StartSpot)
        {
            // haker: when we found PlayerStart sucessfully, update transform of PlayerController using Location and only Yaw (not whole rotation)
            FRotator StartRotation(0, StartSpot->GetActorRotation().Yaw, 0);
            Player->SetInitialLocationAndRotation(StartSpot->GetActorLocation(), StartRotation);
            Player->StartSpot = StartSpot;
            return true;
        }

        OutErrorMessage = FString::Printf(TEXT("Could not find a starting spot"));
        return false;
    }

    /** set the name for a controller */
    virtual void ChangeName(AController* Controller, const FString& S, bool bNameChange)
    {
        if (Other && S.IsEmpty())
        {
            Other->PlayerState->SetPlayerName(S);
            K2_OnChangename(Other, S, bNameChange);
        }
    }

    /** customize incoming player based on URL options */
    // 034 - Foundation - CreateInnerProcessPIEGameInstance * - AGameModeBase::InitNewPlayer
    virtual FString InitNewPlayer(APlayerController* NewPlayerController, const FUniqueNetIdRepl& UniqueId, const FString& Options, const FString& Portal = TEXT(""))
    {
        // the player needs a PlayerState to register successfully
        if (NewPlayerController->PlayerState == nullptr)
        {
            return FString(TEXT("PlayerState is null"));
        }

        // find a starting spot
        // haker: it is important to have PlayerStart in world:
        // - if we failed to find proper PlayerStart, it failed to initialize PlayerController (or Login)
        //   [ ] see PlayerStart in the editor
        // - see AGameModeBase::UpdatePlayerStartSpot (goto 035: CreateInnerProcessPIEGameInstance)
        FString ErrorMessage;
        if (!UpdatePlayerStartSpot(NewPlayerController, Portal, ErrorMessage))
        {
            UE_LOG(LogGameMode, Warning, TEXT("InitNewPlayer: %s"), *ErrorMessage);
        }

        // init player's name
        FString InName = UGameplayStatics::ParseOption(Options, TEXT("Name")).Left(20);
        if (InName.IsEmpty())
        {
            InName = FString::Printf(TEXT("%s%i"), *DefaultPlayerName.ToString(), NewPlayerController->PlayerState->GetPlayerId());
        }

        // haker: assign unique player name:
        // - we are not going to cover this in detail, skip!
        ChangeName(NewPlayerController, InName, false);

        return ErrorMessage;
    }

    /**
     * called to login new players by creating a player controller, overridable by the game
     * 
     * sets up basic properties of the player (name, unique id, registers with backend, etc) and should not be used to do
     * more complicated game logic. the player controller is not fully initialized within this function as far as networking is concerned
     * save "game logic" for PostLogin which is called shortly afterward
     * 
     * if login is successful, returns a new PlayerController to associate with this player
     * login fails if ErrorMessage string is set
     */
    // 029 - Foundation - CreateInnerProcessPIEGameInstance * - AGameModeBase::Login
    // haker: we login a game by creating PlayerController:
    // - we can think of 'Login' as 'a process of connecting PlayerController to LocalPlayer'
    virtual APlayerController* Login(UPlayer* NewPlayer, ENetRole InRemoteRole, const FString& Portal, const FString& Options, const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage)
    {
        // haker: create player controller
        // - see AGameModeBase::SpawnPlayerController (goto 030: CreateInnerProcessPIEGameInstance)
        APlayerController* const NewPlayerController = SpawnPlayerController(InRemoteRole, Options);
        if (NewPlayerController == nullptr)
        {
            return nullptr;
        }

        // haker: by calling SpawnPlayerController(), we are ready for:
        // 1. PlayerController
        // 2. PlayerController is cached in World (WeakObjectPtr)
        // 3. PlayerState: PostInitalizeComponents

        // haker: see GameModeBase::InitNewPlayer (goto 034: CreateInnerProcessPIEGameInstance)
        // - if we failed to find PlayerStart, it will return ErrorMessage!
        ErrorMessage = InitNewPlayer(NewPlayerController, UniqueId, Options, Portal);
        if (!ErrorMessage.IsEmpty())
        {
            NewPlayerController->Destroy();
            return nullptr;
        }

        return NewPlayerController;
    }

    // 026 - Foundation - CreateInnerProcessPIEGameInstance * - UGameModeBase::PreInitializeComponents
    virtual void PreInitializeComponents() override
    {
        AInfo::PreInitializeComponents();

        // haker: the only thing you have to note is that GameState is spawned here!
        // - GameModeBase has ownership of GameState!

        FActorSpawnParameters SpawnInfo;
        SpawnInfo.Instigator = GetInstigator();
        SpawnInfo.ObjectFlags |= RF_Transient; // we never want to save game states or network managers into a map

        // fallback to default GameState if none was specified
        if (GameStateClass == nullptr)
        {
            GameStateClass = AGameStateBase::StaticClass();
        }

        UWorld* World = GetWorld();
        GameState = World->SpawnActor<AGameStateBase>(GameStateClass, SpawnInfo);
        World->SetGameState(GameState);
        if (GameState)
        {
            GameState->AuthorityGameMode = this;
        }

        //...
    }

    // 017 - Foundation - CreateInnerProcessPIEGameInstance * - AGameModeBase's member variables

    /** the class of PlayerController to spawn for players logging in */
    // haker: AGameModeBase has override class for PlayerController:
    // - we can think of these class definitions to override as game's rules
    // - depending on class types, the behavior of unreal object is changed == rules of game
    TSubclassOf<APlayerController> PlayerControllerClass;

    /** the default player name asigned to players that join with no name specified */
    FText DefaultPlayerName;

    /** the default pawn class used by players */
    TSubclassOf<APawn> DefaultPawnClass;

    /** GameState is used to replicate game state relevant properties to all clients */
    // haker: GameState is created on the server in dedicated server, and replicates to clients
    TObjectPtr<AGameStateBase> GameState;
};