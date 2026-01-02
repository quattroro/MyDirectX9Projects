DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnPlayerStatePawnSet, APlayerState*, Player, APawn*, NewPawn, APawn*, OldPawn);

/**
 * A PlayerState is created for every player on a server (or in a standalone game)
 * PlayerStates are replicated to all clients, and contain network game relevant information about the player, such as playername, score etc
 */
// 013 - Foundation - CreateInnerProcessPIEGameInstance * - APlayerState
// haker: try to understand it by the figure:
//                                                       ┌─[Standalone]──────────────────────────────┐                  
//   GameInstance                                        │                                           │                  
//    │                                                  │  PlayerController0                        │                  
//    └──LocalPlayers:TArray<ULocalPlayer>               │    │                                      │                  
//        │                                         ┌───►│    └──PlayerState0                        │                  
//        ├──LocalPlayer0                           │    │                                           │                  
//        │   │                                     │    │                                           │                  
//        │   └──PlayerController0                  │    │                                           │                  
//        │       │                                 │    └───────────────────────────────────────────┘                  
//        │       └──PlayerState0───────────────────┤                                                                   
//        │                                         │    ┌─[Networked]──────────────────────────────────────────────┐   
//        └──LocalPlayer1                           │    │                                                          │   
//                                                  │    │   Client0                   │ Client1                    │   
//                                                  │    │                             │                            │   
//                                                  └───►│   PlayerController0         │ PlayerController1          │   
//                                                       │    │                        │  │                         │   
//                                                       │    └──PlayerState0          │  └──PlayerState1           │   
//                                                       │                             │                            │   
//                                                       │   PlayerState1              │ PlayerState0               │   
//                                                       │                             │                            │   
//                                                       │                             │                            │   
//                                                       │                             │                            │   
//                                                       │                             │                            │   
//                                                       │                             │                            │   
//                                                       │                             │                            │   
//                                                       │                             │                            │   
//                                                       │   ──────────────────────────┴────────────────────────    │   
//                                                       │                                                          │   
//                                                       │   Server                                                 │   
//                                                       │                                                          │   
//                                                       │   PlayerController0                                      │   
//                                                       │    │                                                     │   
//                                                       │    └──PlayerState0                                       │   
//                                                       │                                                          │   
//                                                       │                                                          │   
//                                                       │   PlayerController1                                      │   
//                                                       │    │                                                     │   
//                                                       │    └──PlayerState1                                       │   
//                                                       │                                                          │   
//                                                       │                                                          │   
//                                                       │                                                          │   
//                                                       └──────────────────────────────────────────────────────────┘   
//                                                                                                                      
//          1.Server has All PlayerControllers                                                                          
//                                                                                                                      
//          2.Client0 has its own LocalPlayer0 as its user                                                              
//            :PlayerState1 is controlled by other user, so PlayerState1 has no controller in Client0                   
//                                                                                                                      
//          3.Client1 has its own LocalPlayer1 as its user                                                              
//            :PlayerState0 is controlled by other user, so PlayerState0 has no controller in Client1                   
//                                                                                                                      
//          4.PlayerState is replicated state for player!                                                               
//            ───────────────────────────────────────────                                                               
//                  ***  
//
// - The detail will be relayed to next our network lecture~ :)
// - see APlayerState's properties briefly
class APlayerState : public AInfo
{
    /** set the player name to S locally, does not trigger net updates */
    virtual void SetPlayerNameInternal(const FString& S)
    {
        PlayerNamePrivate = S;
    }
        
    /** set the player name to S */
    virtual void SetPlayerName(const FString& S)
    {
        SetPlayerNameInternal(S);

        // RepNotify callback won't get called by net code if we are the server
        ENetMode NetMode = GetNetMode();
        if (NetMode == NM_Standalone || NetMode == NM_ListenServer)
        {
            OnRep_PlayerName();
        }

        OldNamePrivate = GetPlayerName();
        ForceNetUpdate();
    }

    /** gets the literal value of bOnlySpectator */
    bool IsOnlyASpectator() const
    {
        return bOnlySpectator;
    }

    /** player name, or blank if none */
    FString PlayerNamePrivate;

    /** previous player name: saved on client-side to detect player name changed */
    FString OldNamePrivate;

    /** broadcast whenever this player's possessed pawn is set */
    // haker: if you have listened Lyra Clone Project, this member variable is familiar
    FOnPlayerStatePawnSet OnPawnSet;

    /** the pawn that is controlled by this player state */
    // haker: player state caches PawnPrivate possessed by LocalPlayer's PlayerController
    TObjectPtr<APawn> PawnPrivate;

    /** whether this player can only ever be a spectator */
    uint8 bOnlySpectator : 1;
};

/**
 * controllers are non-physical actors that can possess a Pawn to control its actions.
 * PlayerControllers are used by human players to control pawns, while AIControllers implement the AI for the pawns they control.
 * controllers take control of a pawn using their Possess() method, and relinquish control of the pawn by calling UnPossess()
 * 
 * controllers receive notifications for many of the events occuring for the Pawn they are controlling.
 * this gives the controller the opportunity to implement the behavior in response to this event, intercepting the event and
 * superseding the Pawn's default behavior
 * 
 * ControlRotation (accessed via GetControlRotation()), determines the viewing/aiming direction of the controller Pawn
 * and is affected by input such as from a mouse or gamepad 
 */
// 011 - Foundation - CreateInnerProcessPIEGameInstance * - AController
// haker: controllers can possess and control Pawns which are resided in World
// - try to understand AController by the diagram:
//                   [GameInstance]   │   [World]                                                                 
//                                    │                                                                           
//                                    │                                                                           
//    UPlayer(ULocalPlayer)           │                                                                           
//       │                            │                                                                           
//       └───AController──────────────┼────▲──────────► APawn                                                     
//           (APlayerController)      │    │            (ACharacter)                                              
//                                    │    │             │                                                        
//                                    │ Possess          └──RootComponent                                         
//                                    │ (UnPossess)          │                                                    
//                                    │    ▲                 ├──Component0                                        
//                                         │                 │                                                    
//                                         │                 └──Component1                                        
//                                    ┌────┘                                                                      
//                                    │                                                                           
//                                    │                                                                           
//                      - Possess()/UnPossess() can be considered as connecting world object(APawn or ACharacter) 
//                      - AController gives control actions to world object(APawn) to UPlayer                     
//  
// - try to understand the above comments by Epic:
//   1. controller(PlayerController) gives authority to take action on APawn in UWorld to UPlayer(ULocalPlayer)
//      - giving authority      == Possess()
//      - giving up authorithy  == UnPossess()
//   2. controller(AIController) gives authority to order commands on APawn in UWorld to AI object(agent)
//      - we can also think of 'Controller' as a bridge between world and non-world, giving authority to order commands:
//                                                                      
//                   the bridge between non-world and world             
//                   ┌────────────────────────────────────┐             
//                   │             AController            │             
//          ┌────────┴──┬──────────────────────────────┬──┴────┐        
//          │ Non-World │                              │ World │        
//          └──┬────────┘                              └───┬───┘        
//             │                                           │            
//             ├──GameState (?!)                   ┌─────► ├───APawn0   
//             │   │                               │       │            
//             │   └──LyraBotCreationComponent     │       ├───APawn1   
//             │       :TArray<AAIController>──────┘       │            
//             │               ─────────────               └───APawn2   
//             └──GameInstance    ***                            ▲      
//                 │                                             │      
//                 └──LocalPlayers                               │      
//                     :TArray<ALocalPlayer>                     │      
//                                 │                             │      
//                                 │                             │      
//                             APlayerController ────────────────┘      
//                             ──────────────────                       
//                                  ***                 
//
//  3. controller gets world's events of possessing APawn:
//     - controller can implement event's response behavior
//  4. controller determines the viewing/aiming direction of Pawn:
//     - understand by the diagram:
//                                                                                                                       
//       Platforms     │    Unreal Engine(Non-World)                                         │   Unreal Engine(World)    
//                     │                             ┌─────────────┐                         │                           
//                     │                             │EditorEngine │                         │       ┌───World0          
//                     │                             └──┬──────────┘                         │       │                   
//                     │                                │                                    │       ├───World1          
//      ┌────────┐     │                                └──TArray<FWorldContext>◄────────────┼───────┤                   
//      │PC      ├──┐  │                                                                     │       ├───World2          
//      └────────┘  │  │                                                                     │       │                   
//                  │  │                                                                     │       └───World3          
//      ┌────────┐  │  │      ┌───────────────────┐                                          │            │              
//      │Console ├──┼──┼─────►│UGameViewportClient├────►GameInstance───►LocalPlayers         │            └──Level0      
//      └────────┘  │  │  ▲   └───────────────────┘  ▲               ▲   │                   │                    │      
//                  │  │  │                          │               │   ├──Player0────┐     │       ┌───►APawn0──┤      
//      ┌────────┐  │  │  │                          │               │   │             │     │       │            │      
//      │Mobile  ├──┘  │  │                          │               │   ├──Player1────┼─────┼──▲────┼───►APawn1──┤      
//      └────────┘     │  │                          │               │   │             │  ▲  │  │    │            │      
//                     │  │                          │               │   └──Player2────┘  │  │  │    └───►APawn2──┘      
//                     │  │                          │               │                    │  │  │                        
//                     │  │                          │               │                    │  │  │                        
//                     │  │                          │               │                    │  │ APlayerController         
//                     │  │                          │               │                    │  │                           
//                     │  │                          │               │                    │  │                           
//                     │  │                          │               │                    │  │                           
//                        │                          │               │                    │                              
//                        │                          │               │                    │                              
//                        └──────────────────────────┴──────┬────────┴────────────────────┘                              
//                                                          │                                                            
//                                                          │                                                            
//                                                          │                                                            
//                                       1.Input events are propagate from Platform to UnrealEngine(World):              
//                                          Platforms──►Unreal Engine(Non-World)───►Unreal Engine(World)                 
//                                                       │                           │                                   
//                                                       └─GameViewportClient        └──Pawns                            
//                                                               │                                                       
//                                                               ▼                                                       
//                                                         GameInstance                                                  
//                                                               │                                                       
//                                                               ▼                                                       
//                                                         LocalPlayers                                                  
//                                                                                                                       
//                                       2.Input events are propagated to Pawns directly                                 
//  
//
// - see AController's member variables (goto 012: CreateInnerProcessPIEGameInstance)                                                                                                                     
class AController : public AActor, public INavAgentInterface
{
    // 061 - Foundation - CreateInnerProcessPIEGameInstance - AController::ClientSetRotation_Implementation
    void ClientSetRotation_Implementation(FRotator NewRotation, bool bResetCamera)
    {
        // haker: see SetControlRotation briefly
        SetControlRotation(NewRotation);
        if (Pawn != NULL)
        {
            // haker: using ClientSetRotation() we can update Pawn's rotation by calling FaceRotation()
            // - see APawn::FaceRotation (goto 062: CreateInnerProcessPIEGameInstance)
            Pawn->FaceRotation(NewRotation, 0.f);
        }
    }

    template <class T>
    T* GetPawn() const
    {
        return Cast<T>(Pawn);
    }

    /** called to unpossess our pawn for any reason that is not the pawn being destroyed (destruction handled by PawnDestroyed()) */
    virtual void UnPossess() final
    {
        APawn* CurrentPawn = GetPawn();

        // no need to notify if we don't have a pawn
        if (CurrentPawn == nullptr)
        {
            return;
        }

        OnUnPossess();
    }

    /**
     * handles attaching this controller to the specified pawn
     * only runs on the network authority (where HasAuthority() returns true)
     * derived native classes can override OnPossess to filter the specified pawn
     * when possessed pawn changed, blueprint class get notified by ReceivePossess
     * and OnNewPawn delegate is broadcasted 
     */
    // 051 - Foundation - CreateInnerProcessPIEGameInstance - AController::Possess
    // haker: the above comments is NOT helpful, try to understand what Possess() means by analyzing its codes
    virtual void Possess(APawn* InPawn) final
    {
        // haker: if we don't have authority, we can't process to possess()
        if (!HasAuthority())
        {
            return;
        }

        APawn* CurrentPawn = GetPawn();

        // a notification is required when the current assigned pawn is not possessed (i.e. pawn assigned before calling Possess)
        // haker: we are not cached Controller on Pawn, which is the indicator that the Possess() is finished to be done
        const bool bNotificationRequired = (CurrentPawn != nullptr) && (CurrentPawn->GetController() == nullptr);

        // to perserve backward compatibility we keep notifying derived classed for null pawn in case some overrides decided to react differently when asked to possess a null pawn
        // default engine implementation is to unpossess the current pawn

        // haker: call APlayerController::OnPossess() (currently we are in AController)
        // see APlayerController::OnPossess (goto 052: CreateInnerProcessPIEGameInstance)
        OnPossess(InPawn);

        // notify when pawn to possess (different than the assigned one) has been accepted by the native class or notification is explicitly required
        APawn* NewPawn = GetPawn();
        if ((NewPawn != CurrentPawn) || bNotificationRequired)
        {
            ReceivePossess(NewPawn);
            OnNewPawn.Broadcast(NewPawn);
            OnPossessedPawnChanged.Broadcast(bNotificationRequired ? nullptr : CurrentPawn, NewPawn);
        }

        // haker: we can describes the Possess() as the diagram:
        //                                                                                                           
        //                         ***Possess NewPawn0                                                               
        //   PlayerController ────────────────────────────────────────────► NewPawn0                                 
        //    │                                                               │                                      
        //    ├──RootComponent  ──────────────────────────────────────────►   └──RootComponent                       
        //    │                   1.AttachToPawn():                               │                                  
        //    │                     follow the relative transform to Pawn         ├──PlayerController::RootComponent 
        //    │                     ┌──────────────────────────────────────────┐  │                                  
        //    ├──PlayerCameraManager├─────────────────────────────────────►    │  ├──CameraActor::CameraComponent    
        //    │                     │  2.Update ViewTarget:                    │  │                                  
        //    │                     │     it can be NewPawn0                   │  │                                  
        //    │                     │      or CameraActor to NewPawn           │  │                                  
        //    └──PlayerInput        │◄────────────────────────────────────     │  └──InputComponent                  
        //                          │  3.Register NewPawn0 to PlayerController:│                                     
        //                          │     to get input events from LocalPlayer │                                     
        //                          └─┬────────────────────────────────────────┘                                     
        //                            │                                                                              
        //                            │                                                                              
        //                            └──►***APawn::Restart()                                                        
        // 
        // - from the diagram, we can understand Restart() as updating connections between non-world(PC) and world(Pawn) 
        //   1. NewPawn0 can get input events by pushing its InputComponent to PlayerController (non-world ──► world)
        //   2. PlayerController can update camera data(CameraActor or CameraComponent) from the Pawn (world ──► non-world)          
        // - PlayerController is the bridge between non-world and world:
        //                                                                 
        //             the bridge between non-world and world              
        //             ┌────────────────────────────────────┐              
        //             │             AController            │              
        //    ┌────────┴──┬──────────────────────────────┬──┴────┐         
        //    │ Non-World │                              │ World │         
        //    └───────────┘                              └───────┘           
        //                                                                                             
    }

    // 032 - Foundation - CreateInnerProcessPIEGameInstance * - AController::PostInitializeComponents
    virtual void PostInitializeComponents() override
    {
        AActor::PostInitializeComponents();

        if (IsValid(this))
        {
            // haker: here, we add Controller to the 'World'
            // see UWorld::AddController briefly
            GetWorld()->AddController(this);
        }
    }

    /** set the control rotation; The RootComponent's rotation will also be updated to match it if RootComponent->bAbsoluteRotation is true */
    virtual void SetControlRotation(const FRotator& NewRotation)
    {
        if (!ControlRotation.Equals(NewRotation, 1e-3f))
        {
            ControlRotation = NewRotation;
        }
    }

    /** set the initial location and rotation of the controller, as well as the control rotation; typically used when the controller is first created */
    virtual void SetInitialLocationAndRotation(const FVector& NewLocation, const FRotator& NewRotation)
    {
        SetActorLocationAndRotation(NewLocation, NewRotation, false, nullptr, ETeleportType::ResetPhysics);
        SetControlRotation(NewRotation);
    }

    /** remove dependency that makes us tick before the given pawn */
    virtual void RemovePawnTickDependency(APawn* InOldPawn)
    {
        if (InOldPawn != NULL)
        {
            // haker: when we saw AddPawnTickDependency(), the below code becomes clear naturally
            UPawnMovementComponent* PawnComponent = InOldPawn->GetMovementComponent();
            if (PawnMovement)
            {
                PawnMovement->PrimaryComponentTick.RemovePrerequisite(this, this->PrimaryActorTick);
            }
            InOldPawn->PrimaryActorTick.RemovePrerequisite(this, this->PrimaryActorTick);
        }
    }

    /** add dependency that makes us tick before the given Pawn. this minimizes latency between input processing and Pawn movement */
    // 049 - Foundation - CreateInnerProcessPIEGameInstance - APlayerController::AddPawnTickDependency
    void AddPawnTickDependency(APawn* NewPawn)
    {
        if (NewPawn != NULL)
        {
            bool bNeedsPawnPrereq = true;
            UPawnMovementComponent* PawnMovement = NewPawn->GetMovementComponent();
            if (PawnMovement && PawnMovement->PrimaryComponentTick.bCanEverTick)
            {
                // haker: if we have MovementComponent, set MovementComponent's prerequisite as PC's PrimaryActorTick
                PawnMovement->PrimaryComponentTick.AddPrerequisite(this, this->PrimaryActorTick);

                // Don't need a prereq on the pawn if the movement component already sets up a prereq.
                // haker: if Pawn's PrimaryActorTick has prerequisite for MovementComponent, we don't need to set Pawn tick function's prerequisite as PC's tick function:
                // - try to understand it with the diagram:
                //    World                                                                                                                 
                //     │                                                                                                                    
                //     ├─Pawn0 ◄─────────────────┐                                                                                          
                //     │  │                      ├──────────────1.Pawn0's TickFunction has prerequisite to MovementComponent                
                //     │  └─MovementComponent◄───┘                : The ORDER of TickFunction's execution:                                  
                //     │                  ▲                         1)MovementComponent's TickFunction                                      
                //     │                  └────┐                         ▲                                                                  
                //     │                       │                         │                                                                  
                //     │                       │                    2)Pawn0's TickFunction                                                  
                //     │                       │                       :Prerequisites [MovementComponent,]                                  
                //     │                       │                                                                                            
                //     └─PlayerController ◄────┴────────────────2.MovementComponent has prerequisite to PlayerController                    
                //                                                : The ORDER of TickFunction's execution                                   
                //                                                  [PlayerController,MovementComponent,Pawn0]                              
                //                                                    ****                                                                  
                //                                                    NO NEED TO SET Pawn0's TickFunction Prerequisite as PlayerController! 
                //                                                                                                                                                                                                                                              
                if (PawnMovement->bTickBeforeOwner || NewPawn->PrimaryActorTick.GetPrerequisites().Contains(FTickPrerequisite(PawnMovement, PawnMovement->PrimaryComponentTick)))
                {
                    bNeedsPawnPrereq = false;
                }
            }
            
            // haker: if we need to add PlayerController's TickFunction to Pawn's TickFunction's Prerequisites, add it!
            if (bNeedsPawnPrereq)
            {
                NewPawn->PrimaryActorTick.AddPrerequisite(this, this->PrimaryActorTick);
            }
        }
    }

    /** detach the RootComponent from its parent, but only if bAttachToPawn is true and it was attached to a Pawn */
    virtual void DetachFromPawn();
    {
        if (bAttachToPawn && RootComponent && RootComponent->GetAttachParent() && Cast<APawn>(RootComponent->GetAttachmentRootActor()))
        {
            RootComponent->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
        }
    }

    /**
     * physically attach the controller to the specified Pawn, so that our position reflects theirs
     * the attachment persists during possession of the pawn. the controller's rotation continues to match the ControlRotation
     * attempting to attach to a nullptr Pawn will call DetachFromPawn() instead 
     */
    // 048 - Foundation - CreateInnerProcessPIEGameInstance - PlayerController::AttachToPawn
    // haker: the reason for attaching PC to Pawn is described in the comment:
    // - PC possess the Pawn
    // - PC should follows pawn's rotation and position 
    // - when new input or control values updated in PC, PC reflect its update values to Pawn
    virtual void AttachToPawn(APawn* InPawn)
    {
        if (bAttachToPawn && RootComponent)
        {
            if (InPawn)
            {
                // only attach if not already attached
                // haker: we are in APlayerController!
                if (InPawn->GetRootComponent() && RootComponent->GetAttachParent() != InPawn->GetRootComponent())
                {
                    RootComponent->DetachFromComponent(FDetachmentTransformRules::KeepRelativeTransform);

                    // haker: root component follow's Pawn's root component, so set its rootcomponent's relative location and rotation as 'zero'
                    // - now PC's rotation and location is exactly match to Pawn's values
                    RootComponent->SetRelativeLocationAndRotation(FVector::ZeroVector, FRotator::ZeroRotator);

                    // haker: attach PC's root component to Pawn's root component to possess
                    RootComponent->AttachToComponent(InPawn->GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);
                }
            }
            else
            {
                // haker: if we inserted InPawn as nullptr, we translate it to 'detach'
                // - see DetachFromPawn() briefly
                DetachFromPawn();
            }
        }
    }

    /** setter for Pawn; normally should only be used internally when possessing/unpossessing a Pawn */
    // 047 - Foundation - CreateInnerProcessPIEGameInstance - APlayerController::SetPawn
    // haker: SetPawn() initialize to possess Pawn on Controller:
    // 1. Attach PC's root component to Pawn's root component:
    //                                                      
    //    World                                             
    //     │                                                
    //     └──PersistentLevel                               
    //         │                                            
    //         │                                            
    //         ├──PlayerController0                         
    //         │   │                                        
    //         │   └─RootComponent────────────┐             
    //         │                              │             
    //         │                              │ Attached!   
    //         └──Pawn0                       │             
    //             │                          │             
    //             └─RootComponent◄───────────┘             
    //                │                                     
    //                └─PlayerController0::RootComponent    
    //                                                      
    // 2. Modify Tick Dependency:
    //    - do you remember that tick function of all components has dependency based its own scene graph?
    //                                                                                                                          
    //      ┌──────────────────────────────────────────────────────┐                                                            
    //      │  World                                               │                            1.By SceneGraph(SceneComponent) 
    //      │   │                                                  │                                                            
    //      │   └──PersistentLevel                                 │                              Pawn0─────►PlayerController0  
    //      │       │                                              │                                                            
    //      │       │                                              │                                                            
    //      │       ├──PlayerController0                           │                            2.By AddPawnTickDependency():   
    //      │       │   │                                          │  TickFunctions'Order                                       
    //      │       │   └─RootComponent────────────┐               ├─────────────────────────►    PlayerController0─────►Pawn0  
    //      │       │                              │               │                                                            
    //      │       │                              │ Attached!     │                                                            
    //      │       └──Pawn0                       │               │                                                            
    //      │           │                          │               │                                                            
    //      │           └─RootComponent◄───────────┘               │                                                            
    //      │              │                                       │                                                            
    //      │              └─PlayerController0::RootComponent      │                                                            
    //      │                                                      │                                                            
    //      └──────────────────────────────────────────────────────┘                                                            
    //                                                                                                                          
    virtual void SetPawn(APawn* InPawn)
    {
        // haker: see RemovePawnTickDependency briefly
        RemovePawnTickDependency(Pawn);

        Pawn = InPawn;
        Character = (Pawn ? Cast<ACharacter>(Pawn) : NULL);

        // haker: see AttachToPawn (goto 048: CreateInnerProcessPIEGameInstance)
        AttachToPawn(Pawn);

        // haker: AttachToComponent() makes the order of tick function to be followed after Pawn's tick function
        // - we change PC's tick function "order" to be "first"
        // - see APlayerController::AddPawnTickDependency (goto 049: CreateInnerProcessPIEGameInstance)
        AddPawnTickDependency(Pawn);
    }

    // 012 - Foundation - CreateInnerProcessPIEGameInstance * - AController's member variables

    /** PlayerState containing replicated information about the player using this controller (only exists for players, not NPCs) */
    // haker: see APlayerState (goto 013: CreateInnerProcessPIEGameInstance)
    TObjectPtr<APlayerState> PlayerState;

    /** Actor marking where this controller spawned in */
    // haker: the Actor is PlayerStart:
    // [ ] see the example in LyraProject, what PlayerStart is
    TWeakObjectPtr<class AActor> StartSpot;

    /** the control rotation of the Controller */
    // haker: rotation is accumulated by controller:
    // - most of values are derived from input events 
    // - input events are from UGameViewportClient --> GameInstance --> Player --> PlayerController
    FRotator ControlRotation;

    /** Pawn currently being controlled by this controller; use Pawn.Possess() to take control of a pawn */
    // haker: possessed pawn by this controller
    TObjectPtr<APawn> Pawn;

    /** character currently being controlled by this controller: value is same as Pawn if the controlled pawn is a character, otherwise nullptr */
    TObjectPtr<ACharacter> Character;

    /**
     * if true, the controller location will match the possessed Pawn's location:
     * - if false, it will not be updated
     * - rotation will match ControlRotation in either case
     * 
     * since a Controller's location is normally inaccessible, this is intended mainly for purposes of being able to attach an Actor that follows the possessed Pawn location, but that still has the full aim rotation
     * (since a Pawn might update only some components of the rotation) 
     */
    // haker: controller usually doesn't control attached pawn's location:
    // - controller follows the possessed pawns location by attachment
    // - usually only rotation of the pawn is controlled
    // - this variable indicates whether the controller location is matched with the possessed pawn's location
    uint8 bAttachToPawn : 1;
};

/**
 * object within PlayerController that processes player input
 * only exists on the client in netowrk games 
 */
class UPlayerInput : public UObject
{

};

/** A ViewTarget is the primary actor the camera is associated with */
struct FTViewTarget
{
    /** target actor used to compute POV */
    TObjectPtr<class AActor> Target;

    /** computed point of view */
    FMinimalViewInfo POV;
};

/* Near clipping plane */
float GNearClippingPlane = 10.0f;

template<typename T>
struct TReversedZPerspectiveMatrix : public TMatrix<T>
{
    TReversedZPerspectiveMatrix(T HalfFOVX, T HalfFOVY, T MultFOVX, T MultFOVY, T MinZ, T MaxZ)
        : TMatrix<T>(
            TPlane<T>(MultFOVX / FMath::Tan(HalfFOVX),	0.0f,								0.0f,													0.0f),
            TPlane<T>(0.0f,								MultFOVY / FMath::Tan(HalfFOVY),	0.0f,													0.0f),
            TPlane<T>(0.0f,								0.0f,								((MinZ == MaxZ) ? 0.0f : MinZ / (MinZ - MaxZ)),			1.0f),
            TPlane<T>(0.0f,								0.0f,								((MinZ == MaxZ) ? MinZ : -MaxZ * MinZ / (MinZ - MaxZ)),	0.0f)
        )
    {}
};

// 022 - Foundation - SceneRenderer * - FMinimalViewInfo
// haker: as the name implies, the minimal data set for camera(view) using in render-world
// - see FMinimalViewInfo's member variable briefly
struct FMinimalViewInfo
{
    float GetFinalPerspectiveNearClipPlane() const
    {
        return PerspectiveNearClipPlane > 0.f ? PerspectiveNearClipPlane : GNearClippingPlane;
    }

    // 030 - Foundation - SceneRenderer * - FMinimalViewInfo::CalculateProjectionMatrixGivenViewRectangle
    static void CalculateProjectionMatrixGivenViewRectangle(const FMinimalViewInfo& ViewInfo, TEnumAsByte<enum EAspectRatioAxisConstraint> AspectRatioAxisConstraint, const FIntRect& ConstrainedViewRectangle, FSceneViewProjectionData& InOutProjectionData)
    {
        // create the projection matrix (and possibly constrain the view rectangle)
        if (ViewInfo.bConstrainAspectRatio)
        {
            //...
        }
        else
        {
            // haker: we'll skipped the detail of how to calculate the projection data:
            // - one thing I'd like to mention is that project matrix is 'ReversedZ'PerspectiveMatrix
            // - ReversedZ means near-plane(==1) far-plane(==0):
            //   - originally non-revsered Z has near-plane(==0) and far-plane(==1)
            // - the unreal use 'ReversedZ' for relieving floating-point precision problem:
            //   - the detail is covered this web-page:
            //     https://tomhultonharrop.com/mathematics/graphics/2023/08/06/reverse-z.html
            //   - try to understand the detail, you need to know how floating-point works (in terms of exponent and mantissa)
            float XAxisMultiplier;
            float YAxisMultiplier;

            const FIntRect& ViewRect = InOutProjectionData.GetViewRect();
            const int32 SizeX = ViewRect.Width();
            const int32 SizeY = ViewRect.Height();

            float MatrixHalfFOV;
            {
                // the view-info FOV is horizontal: but if we have a different aspect ratio constraint, we need to
                // adjust this FOV value using the aspect ratio it was computed with, so we can compute the complementary
                // FOV value (with the "effective" aspect ratio) correctly
                const float HalfXFOV = FMath::DegreesToRadians(FMath::Max(0.001f, ViewInfo.FOV) / 2.f);
                const float HalfYFOV = FMath::Atan(FMath::(HalfXFOV)/ViewInfo.AspectRatio);
                MatrixHalfFOV = HalfYFOV;
            }

            {
                const float ClippingPlane = ViewInfo.GetFinalPerspectiveNearClipPlane();

                // haker: TReversedZPersepectiveMatrix
                InOutProjectionData.ProjectionMatrix = FReversedZPerspectiveMatrix(
                    MatrixHalfFOV,
                    MatrixHalfFOV,
                    XAxisMultiplier,
                    YAxisMultiplier,
                    ClippingPlane,
                    ClippingPlane,
                );
            }
        }
    }

    /** calculates the projection matrix (and potentially a constrained view rectangle) given a FMinimalViewInfo and partially configured projection data (must have the view rect already set) */
    // 029 - Foundation - SceneRenderer * - FMinimalViewInfo::CalculateProjectionMatrixGivenView
    static void CalculateProjectionMatrixGivenView(const FMinimalViewInfo& ViewInfo, TEnumAsByte<enum EAspectRatioAxisConstraint> AspectRatioAxisConstraint, class FViewport* Viewport, struct FSceneViewProjectionData& InOutProjectionData)
    {
        // haker: see FMinimalViewInfo::CalculateProjectionMatrixGivenViewRectangle (goto 030: SceneRenderer)
        CalculateProjectionMatrixGivenViewRectangle(ViewInfo, AspectRatioAxisConstraint, InOutProjectionData.GetViewRect(), InOutProjectionData);
    }

    // haker: the below data set is enough to construct View/Proj matrix for the camera

    FVector Location;
    FRotator Rotation;

    /** the horizontal field of view (in degrees) in perspective mode (ignored in orthographic mode) */
    float FOV;

    /** the originally desired horizontal field of view before any adjustments to account for different aspect ratios */
    float DesiredFOV;

    /** the near plane distance of the perspective view (in world units): set to a negative value to use the default global value of GNearClippingPlane */
    float PerspectiveNearClipPlane;
};

/**
 * cached camera POV info, stored as optimization so we only need to do a full camera update once per tick 
 */
// 025 - Foundation - SceneRenderer * - FCameraCacheEntry:
// haker: from the above comment, we can understand what CameraCacheEntry is for:
// - we'll try to understand it by skimming the code
//   - see UWorld::Tick (goto 026: SceneRenderer)
struct FCameraCacheEntry
{
    /** camera POV to cache */
    // haker: familiar data set! MinimalViewInfo!
    FMinimalViewInfo POV;
};

/**
 * APlayerCameraManager is responsible for managing the camera for a particular player
 * it defines the final view properties used by other system (e.g. the renderer), meaning
 * you can think of it as your virtual eyeball in the world. it can compute the final camera
 * properties directly, or it can arbitrate/blend between other objects or actors that influence
 * ths camera (e.g. blending from one CameraActor to another)
 * 
 * The PlayerCameraManagers primary external responsibility is to reliably respond to various
 * Get*() functions, such as GetCameraViewPoint. most everything else is implementation detail
 * and overridable by user projects
 * 
 * by default, a PlayerCameraManager maintains a "view target", which is the primary actor the
 * camera is assocated with. it can also apply various "post" effects to the final view state,
 * such as camera animations, shakes, post-process effects or special effects such as dirt on the lens 
 */
// 015 - Foundation - CreateInnerProcessPIEGameInstance * - APlayerCameraManager
// haker: we need to understand difference between APlayerCameraManager and ACameraActor
// - it has similar principle between UPlayerInput(APlayerController) and UInputComponent
//   - try to understand it with the diagram:
//                                                                GameInstance │ World                              
//                                                                             │                                    
//                                                                             │                                    
//                            PlayerController                                 │                                    
//                             │                                               │                                    
//                             └──PlayerCameraManager                          │                       World        
//                                 │                                           │                        │           
//                                 └──ViewTarget:FTViewTarget                  │                        └──Level0   
//                                     │                                       │                               │    
//                                     └──Target:AActor* ──────────────────────┼────────────────► CameraActor0─┤    
//                                                            UpdateTarget     │                           │   │    
//                                                           ─────────────     │          CameraComponent0─┘   │    
//                                                                  ▲          │          ────────────────     │    
//     PlayerCameraManager collects world data by CameraComponent───┘          │               ***             │    
//     ,then manipulate all camera features e.g. blending,postprocess...etc    │               ▲         Pawn0─┘    
//                                                                                             │         ─────      
//                                                                                             │                    
//                                                                                             │                    
//                                                it has world's properties for camara management                   
// 
// - see PlayerCameraManager's ViewTarget briefly
class APlayerCameraManager : public AActor
{
    /** calculate an updated POV for the given viewtarget */
    virtual void UpdatedViewTarget(FTViewTarget& OutVT, float DeltaTime)
    {
        // ...
        if (ACameraActor* CamActor = Cast<ACameraActor>(OutVT.Target))
        {
            // viewing through a camera actor
            // haker: CameraComponent has real-world data:
            // - CamerActor is just owner of data(CameraComponent)
            // - note that we passed OutVT.POV to update MinimalViewInfo by CameraComponent's data
            CamActor->GetCameraComponent()->GetCameraView(DeltaTime, OutVT.POV);
        }
        else
        {
            //...
        }
    }

    /** caches given final POV info for efficient access from other game code */
    void FillCameraCache(const FMinimalViewInfo& NewInfo)
    {
        CameraCachePrivate.POV = InPOV;
    }

    /** internal function conditionally called from UpdateCamera to do the actual work of updating the camera */
    virtual void DoUpdateCamera(float DeltaTime)
    {
        // haker: I rewrite this function for simplicity:
        // - currently we only interested in FillCameraCache()
        // - the overall steps look like this:
        //   1. UpdateViewTarget
        //      - new data is cached into MinimalViewInfo in ViewTarget.POV
        //   2. cache new POV into APlayerCameraManager::CameraCachePrivate.POV

        // haker: see UpdateViewTarget briefly
        UpdateViewTarget(ViewTarget, DeltaTime);

        FMinimalViewInfo NewPOV = ViewTarget.POV;

        //...

        // cache results
        // haker: now we understand what 'CameraCachePrivate: FCameraCacheEntry' is for:
        // - the up-to-date camera cache after World::Tick
        FillCameraCache(NewPOV);
    }

    /**
     * perform per-tick camera update. called once per tick after all other actors have been ticked
     * non-local players replicate the POV if bUseClientSideCameraUpdate is true 
     */
    // 028 - Foundation - SceneRenderer * - APlayerCameraManager::UpdateCamera
    virtual void UpdateCamera(float DeltaTime)
    {
        // haker: see DoUpdateCamera
        // - the skipped code is related to replicate CameraPOV(Point-of-view data) from server to clients
        DoUpdateCamera(DeltaTime);

        //...
    }

    /** sets a new ViewTarget */
    virtual void SetViewTarget(class AActor* NewViewTarget, FViewTargetTransitionParams TransitionParams = FViewTargetTransitionParams());
    {
        //...

        // if viewport different then new one or we're transitioning from the same target with locked outgoing, then assign it
        if ((NewTarget != ViewTarget.Target) || (PendingViewTarget.Target && BlendParams.bLockOutgoing))
        {
            // if a transition time is specified, then set pending view target accrodingly
            if (TransitionParams.BlendTime > 0)
            {
                //...
            }
            else
            {
                // otherwise, assign new view-target instantly
                AssignViewTarget(NewTarget, ViewTarget);
                // remove old pending ViewTarget so we don't still try to switch to it
                PendingViewTarget.Target = NULL;
            }
        }
        //...
    }

    AActor* GetViewTarget() const
    {
        // to handle verification/caching behavior while preserving constness upstream
        APlayerCameraManager* const NonConstThis = const_cast<APlayerCameraManager*>(this);

        // if blending to another view target, return this one first
        if (PendingViewTarget.Target)
        {
            if (PendingViewTarget.Target)
            {
                return PendingViewTarget.Target;
            }
        }
        return ViewTarget.Target;
    }

    virtual const FMinimalViewInfo& GetCameraCacheView() const
    {
        return CameraCachePrivate.POV;
    }

    virtual float GetFOVAngle() const
    {
        return GetCameraCacheView().FOV;
    }

    /**
     * primary function to retrieve Camera's actual view point
     * consider calling PlayerController::GetPlayerViewPoint() instead 
     */
    virtual void GetCameraViewPoint(FVector& OutCamLoc, FRotator& OutCamRot) const
    {
        const FMinimalViewInfo& CurrentPOV = GetCameraCacheView();
        OutCamLoc = CurrentPOV.Location;
        OutCamRot = CurrentPOV.Rotation;
    }

    /** PlayerController that owns this Camera Manager */
    TObjectPtr<APlayerController> PCOwner;

    /** current view target */
    struct FTViewTarget ViewTarget;

    /** pending view target for blending */
    struct FTViewTarget PendingViewTarget;

    /** cached camera properties */
    // 024 - Foundation - SceneRenderer * - APlayerCameraManager::CameraCachePrivate
    // haker: see FCameraCacheEntry (goto 025: SceneRenderer)
    FCameraCacheEntry CameraCachePrivate;
};

/**
 * PlayerControllers are used by human players to control Pawns
 * 
 * ControlRotation (accessed via GetControlRotation()) determines the aiming orientation of the controller Pawn
 * 
 * in networked games, PlayerControllers exist on the server for every player-controlled pawn
 * and also on the controlling client's machine. they do NOT exist on client's machine for pawns
 * controlled by remote players elsewhere on the netowrk.
 */
// 010 - Foundation - CreateInnerProcessPIEGameInstance * - APlayerController
// haker: user control APawn in UWorld by APlayerController:
// - see AController first (goto 011: CreateInnerProcessPIEGameInstance)
// - next see APlayerController's member variables (goto 014: CreateInnerProcessPIEGameInstance)
class APlayerController : public AController, public IWorldPartitionStreamingSourceProvider
{
    // 027 - Foundation - SceneRenderer * - APlayerController::UpdateCameraManager
    void UpdateCameraManager(float DeltaSeconds)
    {
        if (PlayerCameraManager != NULL)
        {
            // see APlayerCameraManager::UpdateCamera (goto 028: SceneRenderer)
            PlayerCameraManager->UpdateCamera(DeltaSeconds);
        }
    }

    void SetViewTarget(class AActor* NewViewTarget, struct FViewTargetTransitionParams TransitionParams = FViewTargetTransitionParams());
    {
        if (PlayerCameraManager)
        {
            PlayerCameraManager->SetViewTarget(NewViewTarget, TransitionParams);
        }
    }

    virtual ACameraActor* GetAutoActivateCameraForPlayer() const
    {
        UWorld* CurWorld = GetWorld();
        if (!CurWorld)
        {
            return NULL;
        }

        // only bother if there are any registered cameras
        FConstCameraActorIterator CameraIterator = CurWorld->GetAutoActivateCameraIterator();
        if (!CameraIterator)
        {
            return NULL;
        }

        // find our player index
        int32 IterIndex = 0;
        int32 PlayerIndex = INDEX_NONE;
        for (FConstPlayerControllerIterator Iterator = CurWorld->GetPlayerControllerIterator(); Iterator; ++Iterator, ++IterIndex)
        {
            const APlayerController* PlayerController = Iterator->Get();
            if (PlayerController == this)
            {
                PlayerIndex = IterIndex;
                break;
            }
        }

        if (PlayerIndex != INDEX_NONE)
        {
            // find the matching camera
            for (; CameraIterator; ++CameraIterator)
            {
                ACameraActor* CameraActor = CameraIterator->Get();
                if (CameraActor && CameraActor->GetAutoActivatePlayerIndex() == PlayerIndex)
                {
                    return CameraActor;
                }
            }
        }

        return NULL;
    }

    /**
     * if bAutoManageActiveCameraTarget is true, then automatically manage the active camera target
     * if there is a CameraActor placed in the level with an auto-activate player assigned to it, that will be preferred, otherwise SuggestedTarget will be used 
     */
    virtual void AutoManageActiveCameraTarget(AActor* SuggestedTarget)
    {
        // haker: bAutoManageActiveCameraTarget is set as 'true' in PlayerController's constructor
        if (bAutoManageActiveCameraTarget)
        {
            // see if there is a CameraActor with an auto-activate index that matches us
            if (GetNetMode() == NM_Client)
            {
                //...
            }
            else
            {
                // see if there is a CameraActor in the level that auto-activates for this PC
                // haker: I do not want to increase the depth of codes, so I'll leave to read GetAutoActivateCameraForPlayer() and SetViewTarget()
                // - my version of GetAutoActivateCameraForPlayer() and SetViewTarget() is abbreviated forms
                ACameraActor* AutoCameraTarget = GetAutoActivateCameraForPlayer();
                if (AutoCameraTarget)
                {
                    SetViewTarget(AutoCameraTarget);
                    return;
                }
            }

            SetViewTarget(SuggestedTarget);
        }
    }

    virtual bool CanRestartPlayer()
    {
        return PlayerState && !PlayerState->IsOnlyASpectator() /* && ...*/ ;
    }

    virtual class AActor* GetViewTarget() const override
    {
        AActor* CameraManagerViewTarget = PlayerCameraManager ? PlayerCameraManager->GetViewTarget() : NULL;
        return CameraManagerViewTarget ? CameraManagerViewTarget : const_cast<APlayerController*>(this);
    }

    /** spawns and initializes the PlayerState for this controller */
    // 033 - Foundation - CreateInnerProcessPIEGameInstance * - APlayerController::InitPlayerState
    virtual void InitPlayerState()
    {
        if (GetNetMode() != NM_Client)
        {
            UWorld* const World = GetWorld();
            const AGameModeBase* GameMode = World ? World->GetAuthGameMode() : NULL;
            if (GameMode)
            {
                FActorSpawnParameters SpawnInfo;
                // haker: note that PlayerState's owner is PlayerController!
                SpawnInfo.Owner = this;
                SpawnInfo.Instigator = GetInstigator();
                SpawnInfo.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
                SpawnInfo.ObjectFlags |= RF_Transient; // we never want player states to save into a map

                // haker: derive PlayerState class from AGameModeBase, then spawn PlayerState
                TSubclassOf<APlayerState> PlayerStateClassToSpawn = GameMode->PlayerStateClass;
                if (PlayerStateClassToSpawn.Get() == nullptr)
                {
                    PlayerStateClassToSpawn = APlayerState::StaticClass();
                }

                PlayerState = World->SpawnActor<APlayerState>(PlayerStateClassToSpawn, SpawnInfo);
            }
        }
    }

    // 031 - Foundation - CreateInnerProcessPIEGameInstance * - APlayerController::PostInitializeComponents
    virtual void PostInitializeComponents() override
    {
        // see AController::PostInitializeComponents (goto 032: CreateInnerProcessPIEGameInstance)
        AController::PostInitializeComponents();

        // haker: if we are in dedicated server or standalone, we initialize PlayerState in PlayerController
        // see InitPlayerState (goto 033: CreateInnerProcessPIEGameInstance)
        if (IsValid(this) && (GetNetMode() != NM_Client))
        {
            // create a new player replication info
            InitPlayerState();
        }
    }

    /** designate this player controller as local (public for GameModeBase to use, not expected to be called anywhere else) */
    void SetAsLocalPlayerController() { bIsLocalPlayerController = true; }

    /** allows the PlayerController to set up custom input bindings */
    virtual void SetupInputComponent()
    {
        // a subclass could create a different InputComponent class but still want the default bindings
        if (InputComponent == NULL)
        {
            // haker: InputComponent is in AActor::InputComponent
            InputComponent = NewObject<UInputComponent>(this, UInputSettings::GetDefaultInputComponentClass(), TEXT("PC_InputComponent0"));

            // haker: APlayerController is already spawned:
            // - APlayerController is not BeginPlay() yet, but all components are finished to create and initialize
            // - new 'InputComponent' is done by create/register/initialize except for BeginPlay()
            InputComponent->RegisterComponent();

            // haker: see AActor::PreInitializeComponents (goto 040: CreateInnerProcessPIEGameInstance)
        }
    }

    /** spawn the appropriate class of PlayerInput */
    // 038 - Foundation - CreateInnerProcessPIEGameInstance - APlayerController::InitInputSystem
    virtual void InitInputSystem()
    {
        // haker: PlayerInput manages inputs for local player, create PlayerInput if it doesn't exist
        if (PlayerInput == nullptr)
        {
            const UClass* OverrideClass = OverridePlayerInputClass.Get();
            PlayerInput = NewObject<UPlayerInput>(this, OverrideClass ? OverrideClass : UInputSettings::GetDefaultPlayerInputClass());
        }

        // haker: create a default InputComponent to handle inputs from UGameViewportClient's events
        // - see SetupInputComponent
        SetupInputComponent();

        // haker: see PushPendingAutoReceiveInput (goto 043: CreateInnerProcessPIEGameInstance)
        // - we now understand how Inputs are initialized in the world to get input events from LocalPlayer
        UWorld* World = GetWorld();
        World->PersistentLevel->PushPendingAutoReceiveInput(this);
    }

    /** associate a new UPlayer for this PlayerController */
    // 037 - Foundation - CreateInnerProcessPIEGameInstance * - APlayerController::SetPlayer
    virtual void SetPlayer(UPlayer* InPlayer)
    {
        // haker: link Player <-> PlayerController
        Player = InPlayer;
        InPlayer->PlayerController = this;

        // initializations only for local players
        ULocalPlayer* LP = Cast<ULocalPlayer>(InPlayer)
        if (LP)
        {
            // clients need this marked as local (server already knew at construction time)
            SetAsLocalPlayerController();

            // haker: initialize input system binded to LocalPlayer
            // - see InitInputSystem (goto 038: CreateInnerProcessPIEGameInstance)
            InitInputSystem();

            // haker: let's summerize how InputSystem is initialized:
            //                                                                                                                                             
            //  ┌─────────────────────────────────┐                                                                                                        
            //  │CreateInnerProcessPIEGameInstance│                                                                                                        
            //  └───┬─────────────────────────────┘                                                                                                        
            //      │                                                                                                                                      
            //      │                                                                                                                                      
            //  ┌───▼─────────────────────────────┐                                                                                                        
            //  │...                              │:Create GameInstance/WorldContext/PIEWorld/GameViewportClient/...                                       
            //  └───┬─────────────────────────────┘                                                                                                        
            //      │                                                                                                                                      
            //      │                                                                                                                                      
            //  ┌───▼─────────────────────────────┐                                                                                                        
            //  │GameInstance:                    │                                                                                                        
            //  │StartPlayInEditorGameInstance    │                                                                                                        
            //  └─┬───────────────────────────────┘                                                                                                        
            //    │                                                                                                                                        
            //    ├──InitializeActorsForPlay                                                                                                               
            //    │   │                                                                                                                                    
            //    │   ├──1.UpdateWorldComponents(): Register all actor's components and actors                                                             
            //    │   │                                                                                                                                    
            //    │   └──2.Iterating Levels in World, call ULevel::RouteActorInitialize(): Initialize all actor's component and actors                     
            //    │         │                                                                                                                              
            //    │         └──AActor::PreInitializeComponents():                                                                                          
            //    │             │  ┌─────────────────────────────────────────────────────────────────────────────┐                                         
            //    │             └──┤GetWorld()->PersistLevel->RegisterActorForAutoReceiveInput(this, PlayerIndex)│                                         
            //    │                └─────────────────────────────────────────────────────────────────────────────┘                                         
            //    │                 *** create new InputComponent and add it pending register-list on PersistentLevel                                      
            //    │                                                          ────────┬────────────                                                         
            //    │                                                                  │                                                                     
            //    │                                                                  │                                                                     
            //    └──LocalPlayer::SpawnPlayActor():                                  │                                                                     
            //        │                                                              │                                                                     
            //        ├──Create LocalPlayer's New PlayerController                   │                                                                     
            //        │                                                              │                                                                     
            //        │                                                              │ ***In ULevel::PushPendingAutoReceiveInput()                         
            //        └──PlayerController::SetPlayer(LocalPlayer)                    │    ,Add pending InputComponents to PlayerController's PlayerInput   
            //             │                                                         │                                                                     
            //             │ ┌──────────────────┐                                    │                                                                     
            //             └─┤InitInputSystem() │                                    │                                                                     
            //               └─┬────────────────┘                                    │                                                                     
            //                 │                                                     ▼                                                                     
            //                 └──ULevel::PushPendingAutoReceiveInput(New-PlayerController)                                                                
            //                    ───────────────────────────────────────────────────┬─────                                                                
            //                                                                       │                                                                     
            //                                                                       │                                                                     
            //                                                                       │                                                                     
            //                                                                       │                                                                     
            //                                                                       │                                                                     
            //                                                                       ▼                                                                     
            //                                                              *** We're ready to get input events from UGameViewportClient───►LocalPlayer (APlayerController::TickPlayerInput())    
            //                                                                                                                              
        }
        else
        {
            // haker: if current Player is not local-player, the player is replicated from the server:
            // - it creates NetConnection for the player
            // - it is out of scope in this tutorial, so skip it 
            NetConnection = Cast<UNetConnection>(InPlayer);
            if (NetConnection)
            {
                NetConnection->OwningActor = this;
            }
        }
    }

    /** called on the client to do local pawn setup after possession, before calling ServerAcknowledgePossession */
    virtual void AcknowledgePossession(class APawn* P)
    {
        if (Cast<ULocalPlayer>(Player) != NULL)
        {
            // haker: only local player is allowed to update AcknowledgedPawn
            // - see RecalculateBaseEyeHeight() briefly
            AcknowledgedPawn = P;
            if (P != NULL)
            {
                P->RecalculateBaseEyeHeight();
            }
            ServerAcknowledgePossession(P);
        }
    }

    // 056 - Foundation - CreateInnerProcessPIEGameInstance - APlayerController::ClientRestart_Implementation
    virtual void ClientRestart_Implementation(APawn* NewPawn) override
    {
        AcknowledgedPawn = NULL;

        SetPawn(NewPawn);
        
        // only ackknowledge non-null Pawns here:
        // ClientRestart is only ever called by Server for valid params, but we may receive the function call before Pawn is replicated over, so it will resolve to NULL
        // haker: to understand what AcknowledgePossession() does, the replication understanding is necessary
        // - just skimming it and try to understand simply updating PC's Pawn provided by 'Server'
        // - see AcknowledgePossession() briefly
        AcknowledgePossession(GetPawn());

        // haker: update Pawn's Controller with 'this'(PC)
        AController* OldController = GetPawn()->Controller;
        GetPawn()->Controller = this;
        if (OldController != this)
        {
            // in case this is received before APawn::OnRep_Controller is called
            GetPawn()->NotifyControllerChanged();
        }

        // haker: see APawn::DispatchRestart (goto 057: CreateInnerProcessPIEGameInstance)
        GetPawn()->DispatchRestart(true);
    }

    // 052 - Foundation - CreateInnerProcessPIEGameInstance - APlayerController::OnPossess
    virtual void OnPossess(APawn* PawnToPossess) override
    {
        if (PawnToPossess != NULL &&
            (PlayerState == NULL || !PlayerState->IsOnlyASpectator()))
        {
            // haker: I am going to leave the details of unpossess's functions to you~ :)
            const bool bNewPawn = (GetPawn() != PawnToPossess);
            if (GetPawn() && bNewPawn)
            {
                // haker: Target PlayerController who is going to possess PawnToPossess, UnPossess previous Pawn first
                UnPossess();
            }

            // haker: make sure that Source PlayerController unpossesses PawnToPossess previously possessed
            if (PawnToPossess->Controller != NULL)
            {
                PawnToPossess->Controller->UnPossess();
            }

            // haker: now Target PC(this) start to possess PawnToPossess
            //
            //                               ┌──────────2.UnPossess()                      
            //                               │                                             
            //                               ▼                                             
            //           PawnToPossess ◄─────x───────── PlayerController(Source)           
            //                     ▲                                                       
            //                     │     3.Possess()***                                    
            //                     └───────────────────────┐                               
            //                                             │                               
            //                                             │                               
            //                   Pawn0 ◄─────x───────── PlayerController(Target)           
            //                               ▲                                             
            //                               │                                             
            //                               └──────────1.UnPossess()                      
            //                                                                                                                             
            // - see APawn::PossessedBy (goto 053: CreateInnerProcessPIEGameInstance)
            // - APawn::PossessedBy() is not straight-forward to understand, I recommend to read multiple times, then you can get how it works
            PawnToPossess->PossessedBy(this);

            // update rotation to match possessed pawn's rotation
            // haker: see SetControlRotation() briefly
            // - update our PC's control rotation with possessed Pawn's rotation
            SetControlRotation(PawnToPossess->GetActorRotation());

            // haker: we do SetPawn() again (it is for direct-call of OnPossess())
            SetPawn(PawnToPossess);
            check(GetPawn() != NULL);

            // haker: properly update Pawn's possession, if bStartsWithTickEnabled is 'true', force to run Pawn's tick function
            if (GetPawn() && GetPawn()->PrimaryActorTick.bStartsWithTickEnabled)
            {
                GetPawn()->SetActorTickEnabled(true);
            }

            // haker: see ClientRestart_Implementation (goto 056: CreateInnerProcessPIEGameInstance)
            // - now we understand what ClientRestart() is done:
            //   - by possession of Pawn is changed, update Pawn's data (dependent to PC: LocalPlayer) and PC's data (dependent to Pawn: World)
            ClientRestart(GetPawn());
        }
    }

    virtual void OnUnPossess() override
    {
        if (GetPawn() != NULL)
        {
            if (GetLocalRole() == ROLE_Authority)
            {
                GetPawn()->SetReplicates(true);
            }

            GetPawn()->UnPossessed();

            if (GetViewTarget() == GetPawn())
            {
                SetViewTarget(this);
            }
        }

        SetPawn(NULL);
    }

    /** adds an inputcomponent to the top of the input stack */
    virtual void PushInputComponent(UInputComponent* Input)
    {
        if (InInputComponent)
        {
            bool bPushed = false;
            CurrentInputStack.RemoveSingle(InInputComponent);
            for (int32 Index = CurrentInputStack.Num() - 1; Index >= 0; --Index)
            {
                UInputComponent* IC = CurrentInputStack[Index].Get();
                if (IC == nullptr)
                {
                    CurrentInputStack.RemoveAt(Index);
                }
                else if (IC->Priority <= InInputComponent->Priority)
                {
                    CurrentInputStack.Insert(InInputComponent, Index + 1);
                    bPushed = true;
                    break;
                }
            }
            if (!bPushed)
            {
                CurrentInputStack.Insert(InInputComponent, 0);
            }
        }
    }

    /** removes given input component from the input stack (regardless of if it's the top, actually) */
    virtual bool PopInputComponent(UInputComponent* Input)
    {
        if (InInputComponent)
        {
            if (CurrentInputStack.RemoveSingle(InInputComponent) > 0)
            {
                InInputComponent->ClearBindingValues();
                return true;
            }
        }
        return false;
    }

    virtual void GetPlayerViewPoint(FVector& out_Location, FRotator& out_Rotation) const override
    {
        //...

        {
            PlayerCameraManager->GetCameraViewPoint(out_Location, out_Rotation);
        }
    }

    // 014 - Foundation - CreateInnerProcessPIEGameInstance * - APlayerController's member variables

    /** UPlayer associated with this PlayerController; could be a local player or net connection */
    // haker: UPlayer has AController(or ACharacterController) to control the pawn in the UWorld
    TObjectPtr<UPlayer> Player;

    /** set during SpawnActor once and never again to indicate the intent of this controller instance (SEVER ONLY) */
    mutable bool bIsLocalPlayerController;

    /** object that manages player input */
    // haker: all input events propgate from UGameViewpotClient to UPlayerInput(APlayerController):
    // - well-known UEnhancedPlayerInput is derived from UPlayerInput
    // - the detail of how it works can be referenced in APlayerController::TickPlayerInput()
    //   - in our lecture, we are not going to cover this, leave it to you~ :)
    TObjectPtr<UPlayerInput> PlayerInput;

    /** internal: current stack of InputComponents */
    // haker: we need to understand the relationship between UPlayerInput and UInputComponent:
    //                                                   │                                                                                                             
    //                                     GameInstance  │  World    ┌────────────────────────Pawn registers to get input events from PlayerController                 
    //                                                   │           │                        :by creating UInputComponent and push the component to CurrentInputStack 
    //  PlayerController                                 │           │       World              -the detail will be in APlayerController::TickPlayerInput()            
    //   │                                               │           │        │                                                                                        
    //   ├──PlayerInput:UPlayerInput                     │           ▼        └──Level0                                                                                
    //   │                                               │PushInputComponent()       │                                                                                 
    //   ├──CurrentInputStack:TArray<UInputComponent>◄───┼──────────┐          Pawn0─┘                                                                                 
    //   │                                               │          │            │ ▲                                                                                   
    //   └──Pawn                                         │      InputComponent0──┘ │                                                                                   
    //       │                                           │                         │                                                                                   
    //       └───────────────────────────────────────────┼─────────────────────────┘                                                                                   
    //                                                   │      Possess()                                                                                              
    //                                                                                                                                                                 
    TArray<TWeakObjectPtr<UInputComponent>> CurrentInputStack;

    /** if set, then this UPlayerInput class will be used instead of the Input Settings' DefaultPlayerInputClass */
    // haker: you can redirect how PlayerInput can be instantiated by overriding PlayerInputClass
    TSubclassOf<UPlayerInput> OverridePlayerInputClass;

    /** camera manager associated with this Player Controller */
    // haker: see APlayerCameraManager (goto 015: CreateInnerProcessPIEGameInstance)
    TObjectPtr<APlayerCameraManager> PlayerCameraManager;

    /** used in net games so client can acknowledge it possessed a specific pawn */
    TObjectPtr<APawn> AcknowledgedPawn;

    /** the net connection this controller is communicating on, nullptr for local players on server */
    // haker: we are not covered dedicated server components in detail
    TObjectPtr<UNetConnection> NetConnection;

    /** 
     * True to allow this player controller to manage the camera target for you,
     * typically by using the possessed pawn as the camera target. Set to false
     * if you want to manually control the camera target.
     */
    bool bAutoManageActiveCameraTarget;
};