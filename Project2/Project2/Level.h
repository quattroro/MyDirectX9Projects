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

/**
* 레벨 객체:
* 레벨의 배우 목록, BSP 정보 및 브러시 목록을 포함합니다
* 모든 레벨에는 외부 세계가 있으며 지속적인 레벨로 사용할 수 있습니다,
* OwningWorld에서 레벨이 스트리밍된 경우 레벨은 해당 레벨이 속한 월드를 나타냅니다
*/

/**
* 레벨은 배우들(빛, 볼륨, 메쉬 인스턴스 등)의 모음입니다
* 여러 레벨을 로드하고 언로드하여 스트리밍 경험을 만들 수 있습니다
*/

// 10 - 파운데이션 - 크리에이트월드 - U레벨 클래스
// 해커:
// 레벨?
// - 레벨 == 배우 모음:
// - 배우들의 예:
// - 가볍고, 정적인 메쉬, 볼륨, 브러시(예: BSP 브러시: 이진 검색 분할), ...
// 편집자와 함께 BSP 브러시를 설명합니다
// - 나머지 콘텐츠는 당분간 생략하겠습니다:
// - 월드 partition, 스트리밍 등 다양한 주제를 다룰 때 다시 방문할 예정입니다
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

    /** 이 수준의 모든 배우 배열, FActorIteratorBase 및 파생 클래스에서 사용됨 */
// 하커: 이것은 AActor 목록을 포함하는 멤버 변수입니다
    TArray<TObjectPtr<AActor>> Actors;

    /** cached level collection that this level is contained in */
    /** 이 레벨이 에 포함된 캐시 레벨 컬렉션*/
    //레벨의 CollectionType을 정해준다.(레벨들은 해당 CollectionType에 따라서 분류된다.) 그로인해 파티클들믄 들어있는 레벨, 맵만 들어있는 레벨 등등의 엑터의 분류가 가능해진다.
    // see FLevelCollection (goto 19)
    FLevelCollection* CachedLevelCollection;

    /**
     * the world that has this level in its Levels array
     * this is not the same as GetOuter(), because GetOuter() for a streaming level is a vestigial world that is not used
     * it should not be accessed during BeginDestroy(), just like any other UObject references, since GC may occur in any order
     */
     /**
     * 레벨 배열에 이 레벨이 있는 세계
     * 이것은 GetOutter()와 다릅니다. 왜냐하면 스트리밍 레벨의 GetOutter()는 사용되지 않는 잔여 세계이기 때문입니다
     * 다른 UObject 참조와 마찬가지로 BeginDestroy() 동안에는 GC가 어떤 순서로든 발생할 수 있으므로 액세스해서는 안 됩니다
     */
    // haker: let's understand OwningWorld vs. OuterPrivate (여기 나오는 OuterPrivate은 Level이 상속받고 있는 UObject.h에서 가지고있는 월드 객체이다)
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
    // 한번에 컴포넌트들을 전부다 로드하면 느리기때문에 증분적으로 진행한다. 그 상태를 저장하기위한 변수
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
    // 엑터가 틱을 도는데 사용
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