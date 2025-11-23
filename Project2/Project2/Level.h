/**
 * sorts actors such that parent actors will appear before children actors in the list:
 * Stable Sort 
 */
static void SortActorsHierarchy(TArray<TObjectPtr<AActor>>& Actors, ULevel* Level)
{
    TMap<AActor*, int32> DepthMap;
    TArray<AActor*, TInlineAllocator<10>> VistedActors;

    DepthMap.Reserve(Actors.Num());

    bool bFoundWorldSettings = false;

    TFunction<int32(AActor*)> CalcAttachDepth = [
        &DepthMap, 
        &VisitedActors, 
        // haker: we capture TFunction CalcAttachDepth to call recursively
        &CalcAttachDepth, 
        &bFoundWorldSettings]
        (AActor* Actor)
    {
        int32 Depth = 0;
        if (int32* FoundDepth = Depth.Find(Actor))
        {
            Depth = *FoundDepth;
        }
        else
        {
            // WorldSettings is expected to be the first element in the sorted Actors array
            // to accomodate for the known issue where two world settings can exist, we only sort the
            // first one we find to the 0 index
            if (Actor->IsA<AWorldSettings>())
            {
                if (!bFoundWorldSettings)
                {
                    // haker: it should be set as lowest value in int32
                    Depth = TNumericLimits<int32>::Lowest();
                    bFoundWorldSettings = true;
                }
                else
                {
                    UE_LOG(LogLevel, Warning, TEXT("Detected duplicate WorldSettings actor - UE-62934"));
                }
            }
            else if (AActor* ParentActor = Actor->GetAttachParentActor())
            {
                if (!VisitedActors.Contains(ParentActor))
                {
                    VisitedActors.Add(Actor);

                    // Actors attached to a ChildActor have to be registered first or else
                    // they will become detached due to the AttachChildren not yet being populated
                    // and thus not recorded in the ComponentInstanceDataCache
                    if (ParentActor->IsChildActor())
                    {
                        Depth = CalcAttachDepth(ParentActor) - 1;
                    }
                    else
                    {
                        // haker: 
                        // @todo - explain with picture
                        Depth = CalcAttachDepth(ParentActor) + 1;
                    }
                }
            }
            DepthMap.Add(Actor, Depth);
        }
        return Depth;
    };

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

// 10 - Foundation - CreateWorld - ULevel class
// haker:
// Level?
// - level == collection of actors:
//   - examples of actors:
//     - light, static-mesh, volume, brush(e.g. BSP brush: binary-search-partitioning), ...
//       [ ] explain BSP brush with the editor
// - rest of content will be skipped for now:
//   - when we cover different topics like world-partition, streaming etc, we will visit it again
class ULevel : public UObject
{
    // 49 - Foundation - CreateWorld - ULevel::ULevel() constructor
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

    bool IncrementalRegisterComponents(bool bPreRegisterComponents, int32 NumComponentsToUpdate, FRegisterComponentContext* Context)
    {
        // find next valid actor to process components registration
        while (CurrentActorIndexForIncrementalUpdate < Actors.Num())
        {
            AActor* Actor = Actors[CurrentActorIndexForIncrementalUpdate];
            bool bAllComponentsRegistered = true;
            if (IsValid(Actor))
            {
                if (bPreRegisterComponents && !bHasCurrentActorCalledPreRegister)
                {
                    // haker: remember AActor's PreRegisterAllComponent() call in here
                    Actor->PreRegisterAllComponents();
                    bHasCurrentActorCalledPreRegister = true;
                }

                // haker: here, we register component in the incremental manner
                bAllComponentsRegistered = Actor->IncrementalRegisterComponents(NumComponentsToUpdate, Context);
            }

            if (bAllComponentsRegistered)
            {
                // all components have been registered for this actor, move to a next one
                CurrentActorIndexForIncrementalUpdate++;
                bHasCurrentActorCalledPreRegister = false;
            }

            // if we do an incremental registration return to outer loop after each processed actor
            // so outer loop can decide whether we want to continue processing this frame
            if (NumComponentsToUpdate != 0)
            {
                break;
            }
        }

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
    void IncrementalUpdateComponents(int32 NumComponentsToUpdate, bool bRerunConstructionScripts, FRegisterComponentContext* Context = nullptr)
    {
        // a value of 0 means that we want to update all components
        if (NumComponentsToUpdate != 0)
        {
            // only the game can use incremental update functionality
            // haker: the target for debugging is EditorWorld, so we consider NumComponentsToUpdate is 0
            check(OwningWorld->IsGameWorld());
        }

        bool bFullyUpdateComponents = (NumComponentsToUpdate == 0);

        // the editor is never allowed to incrementally update components; make sure to pass in a value of zero for NumActorsToUpdate
        check(bFullyUpdateComponents || OwningWorld->IsGameWorld());

        do
        {
            switch (IncrementalComponentState)
            {
            case EIncrementalComponentState::Init:
                // sort actors to ensure that parent actors will be registered before child actors
                SortActorsHierarchy(Actors, this);
                IncrementalComponentState = EIncrementalComponentState::RegisterInitialComponents;
                // haker: NOTE that no break expression here!

            case EIncrementalComponentState::RegisterInitialComponents:
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
                IncrementalComponentState = EIncrementalComponentState::Init;
                CurrentActorIndexForIncrementalUpdate = 0;
                bHasCurrentActorCalledPreRegister = false;
                bAreComponentsCurrentlyRegistered = true;
                // haker: it is GC related feature, so I skip this for now
                CreateCluster();
                break;
            }
        } while (bFullyUpdateComponents && !bAreComponentsCurrentlyRegistered);
        
        // haker: process all pending physics state creation
        {
            FPhysScene* PhysScene = OwningWorld->GetPhysicsScene();
            if (PhysScene)
            {
                PhysScene->ProcessDeferredCreatePhysicsState();
            }
        }
    }

    /** update all components of actors associated with this level (aka in Actors array) and creates the BSP model components */
    void UpdateLevelComponents(bool bRerunConstructionScripts, FRegisterComponentContext* Context = nullptr)
    {
        // update all components in one swoop
        IncrementalUpdateComponents(0, bRerunConstructionScripts, Context);
    }

    // 11 - Foundation - CreateWorld - ULevel's member variables

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
    EIncrementalComponentState IncrementalComponentState;

    /** whether the actor referenced by CurrentActorIndexForUpdateComponents has called PreRegisterAllComponents */
    uint8 bHasCurrentActorCalledPreRegister : 1;

    /** whether components are currently registered or not */
    uint8 bAreComponentsCurrentlyRegistered : 1;

    /** current index into actors array for updating components */
    // haker: tracking actor index in ULevel's ActorList to support incremental update
    int32 CurrentActorIndexForIncrementalUpdate;

    /** data structures for holding the tick functions */
    // haker: for now, member variables related to tick function are skipped
    FTickTaskLevel* TickTaskLevel;
    
    // goto 9 (UWorld's member variables)
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