/**
 * sorts actors such that parent actors will appear before children actors in the list:
 * Stable Sort 
 */
// 069 - Foundation - CreateWorld - SortActorsHierarchy
static void SortActorsHierarchy(TArray<TObjectPtr<AActor>>& Actors, ULevel* Level)
{
    // haker: we covered conceptually how child-actor works in AActor-UActorComponent structure
    // - this function sorts actor's depth considering AActor-AActor parent-child relationships
    TMap<AActor*, int32> DepthMap;
    TArray<AActor*, TInlineAllocator<10>> VistedActors;

    DepthMap.Reserve(Actors.Num());

    bool bFoundWorldSettings = false;

    // haker: we need to understand how child actor's root-component is attached its parent-actor:
    // - in the last time, I just simply mention how child actor works in unreal
    // - the actual way is related to SceneComponent's AttachParent and AttachChildren:
    //                                                                                                                                              
    //       ┌──────────┐                                                               ┌──────────┐                                                
    //       │  Actor0  ├──────────────────────────────────────────────────────────────►│  Actor1  │                                                
    //       └──┬───────┘                                                               └─┬────────┘                                                
    //          │                                                                         │                                                         
    //       ┌─RootComponent─────────────────────────────────┐                         ┌─RootComponent─────────────────────────────────┐            
    //       │ AttachParent: NULL                            │                         │ AttachParent: Actor0's Component1             │            
    //       │ Children: [Component1, Component3]            │               ┌─────────┤ Children: [Component1, Component2]            │            
    //       └──┬────────────────────────────────────────────┘               │         └──┬────────────────────────────────────────────┘            
    //          │                                                       Actor0──Actor1    │                                                         
    //          │ ┌─Component1────────────────────────────────────┐          │            │ ┌─Component1────────────────────────────────────┐       
    //          ├─┤ AttachParent: RootComponent                   │◄─────────┘            ├─┤ AttachParent: RootComponent                   │       
    //          │ │ Children: [Actor1's RootComponent,Component2] │                       │ │ Children: []                                  │       
    //          │ └─┬─────────────────────────────────────────────┘                       │ └───────────────────────────────────────────────┘       
    //          │   │                                                                     │                                                         
    //          │   │  ┌─Component2────────────────────────────────────┐                  │ ┌─Component2────────────────────────────────────┐       
    //          │   └──┤ AttachParent: Component1                      │                  └─┤ AttachParent: RootComponent                   │       
    //          │      │ Children: []                                  │                    │ Children: []                                  │       
    //          │      └───────────────────────────────────────────────┘                    └───────────────────────────────────────────────┘       
    //          │                                                                                                                                   
    //          │   ┌─Component3────────────────────────────────────┐                                                                               
    //          └───┤ AttachParent: RootComponent                   │                                                                               
    //              │ Children: []                                  │                                                                               
    //              └───────────────────────────────────────────────┘                                                                               
    //                                                                                                                                              
    TFunction<int32(AActor*)> CalcAttachDepth = [
        &DepthMap, 
        &VisitedActors, 
        // haker: we capture TFunction CalcAttachDepth to call lambda recursively
        &CalcAttachDepth, 
        &bFoundWorldSettings]
        (AActor* Actor)
    {
        int32 Depth = 0;
        // haker: not need to do it again if we found the depth of Actor
        if (int32* FoundDepth = DepthMap.Find(Actor))
        {
            Depth = *FoundDepth;
        }
        else
        {
            // WorldSettings is expected to be the first element in the sorted Actors array
            // to accomodate for the known issue where two world settings can exist, we only sort the
            // first one we find to the 0 index
            // haker: as we saw previously, AWorldSettings is the first Actor added when we create UWorld
            // - we need to make sure AWorldSetting is in index-0
            if (Actor->IsA<AWorldSettings>())
            {
                if (!bFoundWorldSettings)
                {
                    // haker: by setting AWorldSetting's depth as lowest value in int32, we can guarantee that AWorldSetting is in index-0
                    Depth = TNumericLimits<int32>::Lowest();
                    bFoundWorldSettings = true;
                }
                else
                {
                    UE_LOG(LogLevel, Warning, TEXT("Detected duplicate WorldSettings actor - UE-62934"));
                }
            }
            // see AActor::GetAttachParentActor (goto 070)
            else if (AActor* ParentActor = Actor->GetAttachParentActor())
            {
                if (!VisitedActors.Contains(ParentActor))
                {
                    VisitedActors.Add(Actor);

                    // Actors attached to a ChildActor have to be registered first or else
                    // they will become detached due to the AttachChildren not yet being populated
                    // and thus not recorded in the ComponentInstanceDataCache
                    // see AActor::IsChildActor (goto 071)
                    if (ParentActor->IsChildActor())
                    {
                        // haker: this case is kind of exception handling... BUT, still can't come up with proper scenario...
                        // *** I need to experiment how it works ?!
                        Depth = CalcAttachDepth(ParentActor) - 1;
                    }
                    else
                    {
                        // haker: 
                        //       ┌──────────────────────────────────────────────────────────┐                                                                           
                        //       │                                                          │                                                                           
                        //       │      Actor0 ◄───  Depth = 0                              │                                                                           
                        //       │       │                                                  │                                                                           
                        //       │       └─RootComponent                                    │                                                                           
                        //       │          │                                               │                                                                           
                        //       │          └─Component1                                    │                                                                           
                        //       │             │                                            │                                                                           
                        //       │             └─Actor1 ◄───  Depth = 1                     │                                                                           
                        //       │                │                                         │                                                                           
                        //       │                └─RootComponent                           │                                                                           
                        //       │                   │                                      │                                                                           
                        //       │                   └─Actor2 ◄────  Depth = 2              │                                                                           
                        //       │                                                          │                                                                           
                        //       │                                                          │                                                                           
                        //       └──────────────────────────────────────────────────────────┘                                                                           
                        Depth = CalcAttachDepth(ParentActor) + 1;
                    }
                }
            }
            DepthMap.Add(Actor, Depth);
        }
        return Depth;
    };

    // haker: iterating Actors in ULevel, calculate depth with respect to AActor (not ActorComponent!)
    for (AActor* Actor : Actors)
    {
        if (Actor)
        {
            CalcAttachDepth(Actor);
            VisitedActors.Reset();
        }
    }

    auto DepthSorter = [&DepthMap](AActor* A, AActor* B)
    {
        const int32 DepthA = A ? DepthMap.FindRef(A) : MAX_int32;
        const int32 DepthB = B ? DepthMap.FindRef(B) : MAX_int32;
        return DepthA < DepthB;
    };

    // haker: merge sort:
    // - leave it to read the code for you ~:)
    StableSortInternal(Actors.GetData(), Actors.Num(), DepthSorter);

    // since all the null entries got sorted to the end, loop them off right now
    // haker: after sorting, there are remaining entries in Actors array, so remove them
    int32 RemoveAtIndex = Actors.Num();
    while (RemoveAtIndex > 0 && Actors[RemoveAtIndex - 1] == nullptr)
    {
        --RemoveAtIndex;
    }
    if (RemoveAtIndex < Actors.Num())
    {
        Actors.RemoveAt(RemoveAtIndex, Actors.Num() - RemoveAtIndex);
    }
}

/** struct that holds on to information about Actors that wish to be auto enabled for input before the player controller has been created */
struct FPendingAutoReceiveInputActor
{
    TWeakObjectPtr<AActor> Actor;
    int32 PlayerIndex;
};

/**
 * the level object:
 * contains the level's actor list, BSP information, and brush list
 * every level has a World as its Outer and can be used as the PersistentLevel, however,
 * when a Level has been streamed in the OwningWorld represents the World that it is a part of 
 */

/**
 * a level is a collection of Actors (lights, volumes, mesh instances etc)
 * multiple levels can be loaded and unloaded into the World to create a streaming experience 
 */

// 010 - Foundation - CreateWorld ** - ULevel class
// haker:
// Level?
// - level == collection of actors:
//   - examples of actors:
//     - light, static-mesh, volume, brush(e.g. BSP brush: binary-search-partitioning), ...
//       [*] explain BSP brush with the editor
// - rest of content will be skipped for now:
//   - when we cover different topics like world-partition, streaming etc, we will visit it again
// see ULevel's member variables (goto 11)
class ULevel : public UObject
{
    /** register an actor that should be added to player's input stack when they are created */
    // 042 - Foundation - CreateInnerProcessPIEGameInstance - ULevel::RegisterActorForAutoReceiveInput
    void RegisterActorForAutoReceiveInput(AActor* Actor, const int32 PlayerIndex)
    {
        // haker: see PendingAutoReceiveInputActors as well as FPendingAutoReceiveInputActor
        PendingAutoReceiveInputActors.Add(FPendingAutoReceiveInputActor(Actor, PlayerIndex));
    }

    /** push any pending auto receive input actor's input components on to the player controller's input stack */
    // 043 - Foundation - CreateInnerProcessPIEGameInstance - ULevel::PushPendingAutoReceiveInput
    // haker: this function process our pending entries of registering InputComponents in the world
    void PushPendingAutoReceiveInput(APlayerController* InPlayerController)
    {
        check(InPlayerController);

        // haker: find the PlayerIndex matching InPlayerController
        int32 PlayerIndex = -1;
        int32 Index = 0;
        for (FConstPlayerControllerIterator Iterator = InPlayerController->GetWorld()->GetPlayerControllerIterator(); Iterator; ++Iterator)
        {
            APlayerController* PlayerController = Iterator->Get();
            if (InPlayerController == PlayerController)
            {
                PlayerIndex = Index;
                break;
            }
            Index++;
        }

        if (PlayerIndex >= 0)
        {
            // haker: get the Actors from PendingAutoReceiveInputActors matching PlayerIndex
            TArray<AActor*> ActorsToAdd;
            for (int32 PendingIndex = PendingAutoReceiveInputActors.Num() - 1; PendingIndex >= 0; --PendingIndex)
            {
                FPendingAutoReceiveInputActor& PendingActor = PendingAutoReceiveInputActors[PendingIndex];
                if (PendingActor.PlayerIndex == PlayerIndex)
                {
                    if (PendingActor.Actor.IsValid())
                    {
                        ActorsToAdd.Add(PendingActor.Actor.Get());
                    }
                    PendingAutoReceiveInputActors.RemoveAtSwap(PendingIndex);
                }
            }

            // haker: iterating ActorsToAdd for enabling InputComponents
            for (int32 ToAddIndex = ActorsToAdd.Num() - 1; ToAddIndex >= 0; --ToAddIndex)
            {
                APawn* PawnToPossess = Cast<APawn>(ActorsToAdd[ToAddIndex]);
                if (PawnToPossess)
                {
                    // haker: we'll see Possess sooner or later
                    InPlayerController->Possess(PawnToPossess);
                }
                else
                {
                    // haker: by calling EnableInput, we add InputComponent to PC
                    // - we already saw EnableInput()
                    ActorsToAdd[ToAddIndex]->EnableInput(InPlayerController);
                }
            }
        }
    }

    // 049 - Foundation - CreateWorld * - ULevel::ULevel() constructor
    ULevel(const FObjectInitializer& ObjectInitializer)
        : UObject(ObjectInitializer)
        // haker: as the function name describes, ULevel allocate new FTickTaskLevel
        // see FTickTaskManager::AllocateTickTaskLevel (goto 50)
        , TickTaskLevel(FTickTaskManagerInterface::Get().AllocateTickTaskLevel())
    {

    }

    bool IsPersistent() const
    {
        bool bIsPersistent = false;
        if (OwningWorld)
        {
            bIsPersistent = (this == OwningWorld->PersistentLevel);
        }
        return bIsPersistent;
    }

    // 072 - Foundation - CreateWorld - ULevel::IncrementalRegisterComponents
    bool IncrementalRegisterComponents(bool bPreRegisterComponents, int32 NumComponentsToUpdate, FRegisterComponentContext* Context)
    {
        // find next valid actor to process components registration
        // haker: CurrentActorIndexForIncrementalUpdate is persistent index to keep track of the last index which we have done in IncrementalRegisterComponents
        while (CurrentActorIndexForIncrementalUpdate < Actors.Num())
        {
            AActor* Actor = Actors[CurrentActorIndexForIncrementalUpdate];
            bool bAllComponentsRegistered = true;
            if (IsValid(Actor))
            {
                // haker: bPreRegisterComponent is whether we call PreRegisterComponents for each Actor in Level
                if (bPreRegisterComponents && !bHasCurrentActorCalledPreRegister)
                {
                    // haker: remember AActor's PreRegisterAllComponent() call in here
                    Actor->PreRegisterAllComponents();
                    bHasCurrentActorCalledPreRegister = true;
                }

                // haker: here, we register component in the incremental manner
                // see AActor::IncrementalRegisterComponents (goto 073)
                bAllComponentsRegistered = Actor->IncrementalRegisterComponents(NumComponentsToUpdate, Context);
            }

            // haker: when we successfully register all components in AActor, we prepare next actor
            if (bAllComponentsRegistered)
            {
                // all components have been registered for this actor, move to a next one
                CurrentActorIndexForIncrementalUpdate++;
                bHasCurrentActorCalledPreRegister = false;
            }

            // if we do an incremental registration return to outer loop after each processed actor
            // so outer loop can decide whether we want to continue processing this frame
            // haker: what does this condition means?
            // - we do NOT modify NumComponentsToUpdate in AActor::IncrementalRegisterComponents()
            // - NumComponentsToUpdate != 0 is that we are going to do incremental-update
            //   - when incremental-update is enabled, we are get out of while-loop every actor
            // - ULevel keep track of current actor index to do incremental-update with CurrentActorIndexForIncrementalUpdate
            // - the comment describes:
            //   - ***outer loop calling this function*** determines whether we are going to continue do incremental-update for this Level
            //   - see the src to understand : 
            //     Level->IncrementalUpdateComponents in World.cpp
            //       - it determines by how much time do we left to do incremental-update
            if (NumComponentsToUpdate != 0)
            {
                break;
            }
        }

        // haker: we successfully done to do all incremental-updates for this Level, and return 'true'
        if (CurrentActorIndexForIncrementalUpdate >= Actors.Num())
        {
            // we need to process pending adds prior to rerunning the construction scripts which may internally preform removals / adds themselves
            if (Context)
            {
                Context->Process();
            }
            CurrentActorIndexForIncrementalUpdate = 0;
            return true;
        }

        return false;
    }

    /** incrementally update all components of actor associated with this level */
    // 068 - Foundation - CreateWorld - IncrementalUpdateComponents
    void IncrementalUpdateComponents(int32 NumComponentsToUpdate, bool bRerunConstructionScripts, FRegisterComponentContext* Context = nullptr)
    {
        // a value of 0 means that we want to update all components
        if (NumComponentsToUpdate != 0)
        {
            // only the game can use incremental update functionality
            // haker: the target for debugging is EditorWorld, so we consider NumComponentsToUpdate is 0
            check(OwningWorld->IsGameWorld());
        }

        // haker: if we pass NumComponentsToUpdate as 0, it will update all components in the level:
        // - this is our case!
        bool bFullyUpdateComponents = (NumComponentsToUpdate == 0);

        // the editor is never allowed to incrementally update components; make sure to pass in a value of zero for NumActorsToUpdate
        // haker: you should understand the below check() function conveniently
        check(bFullyUpdateComponents || OwningWorld->IsGameWorld());

        do
        {
            // haker: see IncrementalComponentState:
            // the incremental update is happened:            
            // - for Init and Finalize is done all at once
            // - in RegisterInitialComponent is done incrementally based on NumComponentsToUpdate
            //
            // ┌──────────────────────────────────┐                       
            // │ EIncrementalComponentState::Init │                       
            // └─┬─┬──────────────────────────────┘                       
            //   │ │                                                      
            //   │ └──SortActorHierarchy()                                
            //   │                                                        
            //   │                                                        
            // ┌─▼─────────────────────────────────────────────────────┐  
            // │ EIncrementalComponentState::RegisterInitialComponents │  
            // └─┬─┬───────────────────────────────────────────────────┘  
            //   │ │                                                      
            //   │ └──IncrementalRegisterComponents(NumComponentsToUpdate)
            //   │                                                        
            //   │                                                        
            // ┌─▼────────────────────────────────────┐                   
            // │ EIncrementalComponentState::Finalize │                   
            // └──────────────────────────────────────┘   
            //
            switch (IncrementalComponentState)
            {
            case EIncrementalComponentState::Init:
                // sort actors to ensure that parent actors will be registered before child actors
                // haker: before registering components for all actors in Level, sort actors hierarchically
                // see SortActorsHierarchy (goto 069)
                SortActorsHierarchy(Actors, this);
                IncrementalComponentState = EIncrementalComponentState::RegisterInitialComponents;
                // haker: NOTE that no break expression here!

            case EIncrementalComponentState::RegisterInitialComponents:
                // see ULevel::IncrementalRegisterComponents (goto 072)
                if (IncrementalRegisterComponents(true, NumComponentsToUpdate, Context))
                {
#if WITH_EDITOR || 1
                    // haker: SCS is editor-specific running logic
                    const bool bShouldRunConstructionScripts = !bHasRerunConstructionScripts && bRerunConstructionScripts && !IsTemplate();
                    IncrementalComponentState = bShouldRunConstructionScripts ? EIncrementalComponentState::RunConstructionScripts : EIncrementalComponentState::Finalize; 
#else
                    IncrementalComponentState = EIncrementalComponentState::Finalize;
#endif`
                }
                break;
#if WITH_EDITOR || 1
            // haker: we are not going to look into SCS related codes
            case EIncrementalComponentState::RunConstructionScripts:
                if (IncrementalRunConstructionScripts(bFullyUpdateComponents))
                {
                    IncrementalComponentState = EIncrementalComponentState::Finalize;
                }
                break;
#endif
            case EIncrementalComponentState::Finalize:
                // haker: we finish registering components for all actors in Level
                IncrementalComponentState = EIncrementalComponentState::Init;
                CurrentActorIndexForIncrementalUpdate = 0;
                bHasCurrentActorCalledPreRegister = false;
                bAreComponentsCurrentlyRegistered = true;
                // haker: it is GC related feature, so I skip this for now
                CreateCluster();
                break;
            }
        
        // haker: focus the condition:
        // - we only iterate again only if bFullyUpdateComponents is 'true'
        } while (bFullyUpdateComponents && !bAreComponentsCurrentlyRegistered);
        
        // haker: process all pending physics state creation
        // - by processing deferred creation of physics state, all components are reflected to Physics World (FPhysScene)
        {
            FPhysScene* PhysScene = OwningWorld->GetPhysicsScene();
            if (PhysScene)
            {
                PhysScene->ProcessDeferredCreatePhysicsState();
            }
        }
    }

    /** update all components of actors associated with this level (aka in Actors array) and creates the BSP model components */
    // 067 - Foundation - CreateWorld - UpdateLevelComponents
    void UpdateLevelComponents(bool bRerunConstructionScripts, FRegisterComponentContext* Context = nullptr)
    {
        // update all components in one swoop
        // haker: UpdateComponents for each level is incremental:
        // - NOTE that NumComponentsToUpdate == 0 -> we update all components in the level
        // see ULevel::IncrementalUpdateComponents (goto 068)
        IncrementalUpdateComponents(0, bRerunConstructionScripts, Context);
    }

    /** routes pre and post initialization for the next time this level is streamed in */
    // 025 - Foundation - CreateInnerProcessPIEGameInstance * - ULevel::RouteActorInitialize
    void RouteActorInitialize(int32 NumActorsToProcess)
    {
        // haker: here, we follow initialization steps:
        // 1. PreInitialize
        // 2. Initialize
        // 3. (if the World's BeginPlay() is called), BeginPlay

        // haker: if we pass NumActorsToProcess as '0', it will update all actors!
        const bool bFullProcessing = (NumActorsToProcess <= 0);
        switch (RouteActorInitializationState)
        {
            case ERouteActorInitializationState::Preinitialize:
            {
                // actor pre-initialization may spawn new actors so we need to incrementally process until actor count stablizes
                while (RouteActorInitializationIndex < Actors.Num())
                {
                    AActor* const Actor = Actors[RouteActorInitializationIndex];
                    if (Actor && !Actor->IsActorInitialized())
                    {
                        // haker: before going further, let's consider AGameModeBase cases:
                        // - see AGameModeBase::PreInitializeComponents (goto 026: CreateInnerProcessPIEGameInstance)
                        Actor->PreInitializeComponents();
                    }

                    ++RouteActorInitializationIndex;
                    if (!bFullProcessing && (--NumActorsToProcess == 0))
                    {
                        return;
                    }
                } 

                RouteActorInitializationIndex = 0;
                RouteActorInitializationState = ERouteActorInitializationState::Initialize;
                // haker: note that we don't have break statement for case statement!
            }

            // intental fall-through, proceeding if we haven't expired our actor count budget
            case ERouteActorInitializationState::Initialize:
            {
                while (RouteActorInitializationIndex < Actors.Num())
                {
                    AActor* const Actor = Actors[RouteActorInitializationIndex];
                    if (Actor)
                    {
                        if (!Actor->IsActorInitialized())
                        {
                            // haker: all tick functions of all components in Actor are enabled, if possible
                            Actor->InitializeComponents();
                            Actor->PostInitializeComponents();
                        }
                    }

                    ++RouteActorInitializationIndex;
                    if (!bFullProcessing && (--NumActorsToProcess == 0))
                    {
                        return;
                    }
                }

                RouteActorInitializationIndex = 0;
                RouteActorInitializationState = ERouteActorInitializationState::BeginPlay;
            }

            // intentional fall-through, proceeding if we haven't expired our actor count budget
            case ERouteActorInitializationState::BeginPlay:
            {
                // haker: our world is not called BeginPlay()
                // - we will call this later, so for now skip it
                if (OwningWorld->HasBegunPlay())
                {
                    while (RouteActorInitializationIndex < Actors.Num())
                    {
                        // child actors have play begun explicitly by their parents
                        AActor* const Actor = Actors[RouteActorInitializationIndex];
                        if (Actor && !Actor->IsChildActor())
                        {
                            const bool bFromLevelStreaming = true;
                            Actor->DispatchBeginPlay(bFromLevelStreaming);
                        }

                        ++RouteActorInitializationIndex;
                        if (!bFullProcessing && (--NumActorsToProcess == 0))
                        {
                            return;
                        }
                    }
                }

                RouteActorInitializationState = ERouteActorInitializationState::Finished;
            }

            // intentional fall-through if we're done
            case ERouteActorInitializationState::Finished:
            {
                break;
            }
        }
    }

    /**
     * sorts the actor list by net relevancy and static behavior
     * first all not net relevant static actors, then all net relevant static actors and then the rest
     * this is done to allow the dynamic and net relevant actor iterators to skip large amount of actors 
     */
    void SortActorList()
    {
        if (Actors.Num() == 0)
        {
            return;
        }

        TArray<AActor*> NewActors;
        TArray<AActor*> NewNetActors;
        NewActors.Reserve(Actors.Num());
        NewNetActors.Reserve(Actors.Num());

        if (WorldSettings)
        {
            // the WorldSettings tries to stay at index 0
            NewActors.Add(WorldSettings);

            if (OwningWorld != nullptr)
            {
                OwningWorld->AddNetworkActor(WorldSettings);
            }
        }

        // add non-net actors to the NewActors immediately, cache off the net actors to Append after
        for (AActor* Actor : Actors)
        {
            if (IsValid(Actor) && Actor != WorldSettings)
            {
                if (IsNetActor(Actor))
                {
                    NewNetActors.Add(Actor);
                    if (OwningWorld != nullptr)
                    {
                        OwningWorld->AddNetworkActor(Actor);
                    }
                }
                else
                {
                    NewActors.Add(Actor);
                }
            }
        }

        NewActors.Append(MoveTemp(NewNetActors));

        // replace with sorted list
        Actors = ObjectPtrWrap(MoveTemp(NewActors));
    }

    // 011 - Foundation - CreateWorld ** - ULevel's member variables

    /** array of all actors in this level, used by FActorIteratorBase and derived classes */
    // haker: this is the member variable which contains a list of AActor
    // see AActor (goto 12)
    TArray<TObjectPtr<AActor>> Actors;

    /** cached level collection that this level is contained in */
    // see FLevelCollection (goto 19)
    FLevelCollection* CachedLevelCollection;

    /**
     * the world that has this level in its Levels array
     * this is not the same as GetOuter(), because GetOuter() for a streaming level is a vestigial world that is not used
     * it should not be accessed during BeginDestroy(), just like any other UObject references, since GC may occur in any order
     */
    // haker: let's understand OwningWorld vs. OuterPrivate
    // - note that my explanation is based on WorldComposition's level streaming or LevelBlueprint's level load/unload manipulation
    //   - World Partition has different concept which usually OwningWorld and OuterPrivate is same
    // - Diagram:                                                                                                        
    //      World0(OwningWorld)──[OuterPrivate]──►Package0(World.umap)                                          
    //       ▲                                                                                                  
    //       │                                                                                                  
    // [OuterPrivate]                                                                                           
    //       │                                                                                                  
    //       │                                                                                                  
    //      Level0(PersistentLevel)                                                                             
    //       │                                                                                                  
    //       │                                                                                                  
    //       ├────Level1──[OuterPrivate]──►World1───[OuterPrivate]───►Package1(Level1.umap)                     
    //       │                                                                                                  
    //       └────Level2───────►World2───────►Package2(Level2.umap)                                               
    TObjectPtr<UWorld> OwningWorld;

    enum class EIncrementalComponentState : uint8
    {
        Init,
        RegisterInitialComponents,
#if WITH_EDITOR || 1
        RunConstructionScripts,
#endif
        Finalize,
    };

    /** the current stage for incrementally updating actor components in the level */
    // haker: we already covered AActor's initialization steps
    // - incremental register components for each AActor
    EIncrementalComponentState IncrementalComponentState;

    /** whether the actor referenced by CurrentActorIndexForUpdateComponents has called PreRegisterAllComponents */
    uint8 bHasCurrentActorCalledPreRegister : 1;

    /** whether components are currently registered or not */
    // haker: the incremental update is based on the count of components:
    // - it could be ceased when it reach maximum component count in Actor
    uint8 bAreComponentsCurrentlyRegistered : 1;

    /** current index into actors array for updating components */
    // haker: tracking actor index in ULevel's ActorList to support incremental update
    int32 CurrentActorIndexForIncrementalUpdate;

    /** data structures for holding the tick functions */
    // haker: for now, member variables related to tick function are skipped
    FTickTaskLevel* TickTaskLevel;
    
    // goto 9 (UWorld's member variables)

    enum class ERouteActorInitializationState : uint8
    {
        Preinitialize,
        Initialize,
        BeginPlay,
        Finished,
    };
    ERouteActorInitializationState RouteActorInitializationState;
    int32 RouteActorInitializationIndex;

    TObjectPtr<AWorldSettings> WorldSettings;

    /** array of actors to be exposed to GC in this level; all other actors will be referenced through ULevelActorContainer */
    TArray<TObjectPtr<AActor>> ActorsForGC;

    /** Actors awaiting input to be enabled once the appropriate PlayerController has been created */
    TArray<FPendingAutoReceiveInputActor> PendingAutoReceiveInputActors;
};

/**
 * abstract base class of container object encapsulating data required for streaming and providing
 * interface for when a level should be streamed in and out of memory 
 */
class ULevelStreaming : public UObject
{
    static ULevelStreaming* FindStreamingLevel(const ULevel* Level)
    {
        ULevelStreaming* FoundLevelStreaming = nullptr;
        if (Level && Level->OwningWorld && !Level->IsPersistentLevel())
        {
            FLevelAnnotation LevelAnnotation = LevelAnnotations.GetAnnotation(Level);
            if (LevelAnnotation.LevelStreaming)
            {
                FoundLevelStreaming = LevelAnnotation.LevelStreaming;
            }
        }
        return FoundLevelStreaming;
    }

    /** annotation for fast inverse lookup */
    struct FLevelAnnotation
    {
        ULevelStreaming* LevelStreaming = nullptr;
    };

    // haker: we skip FUObjectAnnotationSparse:
    // - explain what FUObjectAnnotationSparse is
    static FUObjectAnnotationSparse<FLevelAnnotation, false> LevelAnnotations;
};

/**
 * a set of static methods for common editor operations that operate on ULevel objects 
 */
class FLevelUtils
{
    /** return the streaming level corresponding to the specified ULevle, or NULL if none exists */
    static ULevelStreaming* FindStreamingLevel(const ULevel* Level)
    {
        return ULevelStreaming::FindStreamingLevel(Level);
    }
};