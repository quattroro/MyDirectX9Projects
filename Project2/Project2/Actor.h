
/** thread safe container for actor related global variables */
// haker: 
// - what is TLS(Thread-Local Storage)?
// - why is it thread-safe?
// - understand TLS with TThreadSingleton
// - VERY USEFUL concept to use parallel programming in client-side
// 075 - Foundation - CreateWorld - FActorThreadContext
class FActorThreadContext : public TThreadSingleton<FActorThreadContext>
{
    FActorThreadContext()
        : TestRegisterTickFunctions(nullptr)
    {}

    /** tests tick function registration */
    AActor* TestRegisterTickFunctions;
};

/**
 * tick function that calls Actor::TickActor 
 */
// 044 - Foundation - CreateWorld * - FActorTickFunction
// see FTickFunction (goto 45)
struct FActorTickFunction : public FTickFunction
{
    // 029 - Foundation - TickTaskManager - FActorTickFunction::ExecuteTick
    virtual void ExecuteTick(float DeltaTime, ELevelTick TickType, ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent) override
    {
        if (Target && IsValidChecked(Target) && !Target->IsUnreachable())
        {
            if (TickType != LEVELTICK_ViewportsOnly || Target->ShouldTickIfViewportsOnly())
            {
                // see AActor::TickActor (goto 030 : TickTaskManager)
                Target->TickActor(DeltaTime * Target->CustomTimeDilation, TickType, *this);
            }
        }
    }

    /** AActor that is the target of this tick */
    class AActor* Target;
};

/**
 * Default number of components to expect in TInlineAllocators used with AActor component arrays.
 * Used by engine code to try to avoid allocations in AActor::GetComponents(), among others.
 */
enum { NumInlinedActorComponents = 24 };

/**
 * walks through components hierarchy and returns closest to root parent component that is unregistered
 * only for components that belong to the same owner 
 */
// 081 - Foundation - CreateWorld - GetUnregisteredParent
// haker: return closest un-registered parent component:
// - constraints:
//   - AttachParent should have same owner(AActor) to Component
//   - AttachParent should be bAutoRegister as true
static USceneComponent* GetUnregisteredParent(UActorComponent* Component)
{
    USceneComponent* ParentComponent = nullptr;
    USceneComponent* SceneComponent = Cast<USceneComponent>(Component);

    while (SceneComponent
        // haker: we check all conditions for Component's AttachParent
        // - whether AttachParent's owner and Component's owner is same
        // - whether Component's AttachParent is registered
        //   --> this is final condition we exactly look for
        && SceneComponent->GetAttachParent()
        && SceneComponent->GetAttachParent()->GetOwner() == Component->GetOwner()
        && !SceneComponent->GetAttachParent()->IsRegistered())
    {
        SceneComponent = SceneComponent->GetAttachParent();
        if (SceneComponent->bAutoRegister && IsValidChecked(SceneComponent))
        {
            // we found unregistered parent that should be registered
            // but keep looking up the tree
            ParentComponent = SceneComponent;
        }
    }

    return ParentComponent;
}

/** utils to call OnComponentCreated on components */
// 004 - Foundation - SpawnActor - DispatchOnComponentsCreated()
static void DispatchOnComponentsCreated(AActor* NewActor)
{
    TInlineComponentArray<UActorComponent*> Components;
    NewActor->GetComponents(Components);

    for (UActorComponent* ActorComp : Components)
    {
        if (!ActorComp->HasBeenCreated())
        {
            ActorComp->OnComponentCreated();
        }
    }
}

// simple and short-lived cache for storing transforms between beginning and finishing spawning
static TMap<TWeakObjectPtr<AActor>, FTransform> GSpawnActorDeferredTransformCache;

// 006 - Foundation - SpawnActor - ValidateDeferredTransformCache
static void ValidateDeferredTransformCache()
{
    // clean out any entries where the actor is no longer valid
    // could happen if an actor is destroyed before FinishSpawning is called
    // haker: iterate GSpawnActorDeferredTransformCache and remove it when Actor is invalidated
    for (auto It = GSpawnActorDeferredTransformCache.CreateIterator(); It; ++It)
    {
        const TWeakObjectPtr<AActor>& ActorRef = It.Key();
        if (ActorRef.IsValid() == false)
        {
            It.RemoveCurrent();
        }
    }
}

/** the network role of an actor on a local/remote network context */
enum ENetRole : int32
{
    /** no role at all */
    ROLE_None,
    /** locally simulated proxy of this actor */
    ROLE_SimulatedProxy,
    /** locally autonomous proxy proxy of this actor */
    ROLE_AutonomousProxy,
    /** authoritative control over the actor */
    ROLE_Authority,
    ROLE_MAX,
};

/** determines how the transform being passed into actor spawning methods interact with the actor's default root component */
enum class ESpawnActorScaleMethod : uint8
{
    /** ignore the default scale in the actor's root component and hard-set it to the value of SpawnTransform Parameter */
    OverrideRootScale,
    /** multiply value of the SpawnTransform Parameter with the default scale in the actor's root component */
    MultiplyWithRoot,
    SelectDefaultAtRuntime,
};

// 019 - Foundation - ExecuteConstruction * - FindFirstFreeName
// haker: what we are interested in is 'unique_number' of FName
// - FName consists of two parts:
//   1. name entry: e.g. "ActorComponent"
//   2. unique number: e.g. "21", "1", ...
//   -> "ActorComponent_21", "ActorComponent_1", ...
static FName FindFirstFreeName(UObject* Outer, FName BaseName)
{
    int32 Lower = 0;
    FName Ret = FName(BaseName, Lower);

    // binary search if we appear to have used this name a lot, else just linear search for the first free index
    // haker: if the unique number is less than 100, we try to find it by binary-search
    // - we don't cover how the binary-search works, but recommend to look at how it works
    if (FindObjectFast<UObject>(Outer, FName(BaseName, 100)))
    {
        // could be replaced by Algo::LowerBound if TRange or some other integer range type could be made compatiable with the algo's implementation
        int32 Upper = INT_MAX;
        while (true)
        {
            int32 Next = (Upper - Lower) / 2 + Lower;
            if (FindObjectFast<UObject>(Outer, FName(BaseName, Next)))
            {
                Lower = Next + 1;
            }
            else
            {
                Upper = Next;
            }

            if (Upper == Lower)
            {
                Ret = FName(BaseName, Lower);
                break;
            }
        }
    }
    // haker: otherwise, we try to looping FindObjectFast until we find the unique FName
    else
    {
        while (FindObjectFast<UObject>(Outer, Ret))
        {
            Ret = FName(BaseName, ++Lower);
        }
    }
}

/** at which point in the rerun construction script process is ApplyToActor being called for */
enum class ECacheApplyPhase
{
    PostSimpleConstructionScript, // after the simple construction scripts has been run
    PostUserConstructionScript,   // after the user construction script has been run
    NonConstructionScript,        // not called during the construction script process
};

class FStructOnScope
{
    TWeakObjectPtr<const UStruct> ScriptStruct;
    uint8* SampleStructMemory;
    TWeakObjectPtr<UPackage> Package;
    /** whether the struct memory is owned by this instance */
    bool OwnsMemory;

    virtual void Initialize()
    {
        if (const UStruct* ScriptStructPtr = ScriptStruct.Get())
        {
            SampleStructMemory = (uint8*)FMemory::Malloc(ScriptStructPtr->GetStructureSize() ? ScriptStructPtr->GetStructureSize() : 1, ScriptStructPtr->GetMinAlignment());
            ScriptStructPtr->InitializeStruct(SampleStructMemory);
            OwnsMemory = true;
        }
    }

    FStructOnScope(const UStruct* InScriptStruct)
        : ScriptStruct(InScriptStruct)
        , SampleStructMemory(nullptr)
        , OwnsMemory(false)
    {
        Initialize();
    }
};

template <typename T>
struct TBaseStructureBase
{
	static UScriptStruct* Get()
	{
		return T::StaticStruct();
	}
};

template <typename T>
struct TBaseStructure : TBaseStructureBase<T>
{
};

/** typed FStructOnScope that exposes type-safe access to the wrapped struct */
template <typename T>
class TStructOnScope final : public FStructOnScope
{
    template <typename U, typename = typename TEnableIf<TIsDerivedFrom<typename TRemoveReference<U>::Type, T>::IsDerived, void>::Type>
    explicit TStructOnScope(U&& InStruct)
        : FStructOnScope(TBaseStructure<typename TRemoveReference<U>::Type>::Get())
    {
        if (const UStructScript* ScriptStructPtr = Cast<UScriptStruct>(ScriptStruct.Get()))
        {
            ScriptStructPtr->CopyScriptStruct(SampleStructMemory, &InStruct);
        }
    }
};

struct FActorComponentInstanceSourceInfo
{
    FActorComponentInstanceSourceInfo(TObjectPtr<const UObject> InSourceComponentTemplate, EComponentCreationMethod InSourceComponentCreationMethod, int32 InSourceComponentTypeSerializedIndex)
        : SourceComponentTemplate(InSourceComponentTemplate)
        , SourceComponentCreationMethod(InSourceComponentCreationMethod)
        , SourceComponentTypeSerializedIndex(InSourceComponentTypeSerializedIndex)
    {
    }

    /** the template used to create the source component */
    TObjectPtr<const UObject> SourceComponentTemplate = nullptr;

    /** the method that was used to create the source component */
    EComponentCreationMethod SourceComponentCreationMethod = EComponentCreationMethod::Native;

    /**
     * the index of the source component in its owner's serialized array
     * when filtered to just that component type 
     */
    int32 SourceComponentTypeSerializedIndex = INDEX_NONE;
};

/** base class for instance cached data of a particular type */
struct FInstanceCacheDataBase
{
    TArray<uint8> SavedProperties;

    /** duplicated objects created when saving instance properties */
    TArray<FDataCacheDuplicatedObjectData> DuplicatedObjects;
};

struct FDataCacheDuplicatedObjectData
{
    /** the duplicated object */
    UObject* DuplicatedObject;

    /** object outer depth so we can sort creation order */
    int32 ObjectPathDepth;
};

class FComponentPropertyReader : public FDataCachePropertyReader
{

};

/** base calss for component instance cached data of a particular type */
struct FActorComponentInstanceData : public FInstanceCacheDataBase
{
    /** determines if any instance data was actually saved */
    bool MatchesComponent(const UActorComponent* Component, const UObject* ComponentTemplate) const
    {
        return FActorComponentInstanceSourceInfo(SourceComponentTemplate, SourceComponentCreationMethod, SourceComponentTypeSerializedIndex).MatchesComponent(Component, ComponentTemplate);
    }

    /** applies this component instance data to the supplied component */
    virtual void ApplyToComponent(UActorComponent* Component, const ECacheApplyPhase CacheApplyPhase)
    {
        // after the user construction script has run we will re-apply all the cached changes that do not conflict with a change that the user construction script made
        if ((CacheApplyPhase == ECacheApplyPhase::PostUserConstructionScript || CacheApplyPhase == ECacheApplyPhase::NonConstructionScript) && SavedProperties.Num() > 0)
        {
            if (CacheApplyPhase == ECacheApplyPhase::PostUserConstructionScript)
            {
                Component->DetermineUCSModifiedProperties();
            }
            else if (CacheApplyPhase == ECacheApplyPhase::NonConstructionScript)
            {
                // when this case is used, we want to apply all properties, even UCS modified ones
                Component->ClearUCSModifiedProperties();
            }

            for (const FDataCacheDuplicatedObjectData& DuplicatedObjectData : GetDuplicatedObjects())
            {
                if (DuplicatedObjectData.DuplicatedObject)
                {
                    if (UObject* OtherObject = StaticFindObjectFast(nullptr, Component, DuplicatedObjectData.DuplicatedObject->GetFName()))
                    {
                        OtherObject->Rename(nullptr, GetTransientPackage(), REN_DontCreateRedirectors | REN_ForceNoResetLoaders);
                    }
                    DuplicatedObjectData.DuplicatedObject->Rename(nullptr, Component, REN_DontCreateRedirectors | REN_ForceNoResetLoaders);
                }
            }

            FComponentPropertyReader ComponentPropertyReader(Component, *this);
            Component->PostApplyToComponent();
        }
    }

    /** The template used to create the source component */
    TObjectPtr<const UObject> SourceComponentTemplate;

    /** The method that was used to create the source component */
    EComponentCreationMethod SourceComponentCreationMethod;

    /** 
     * The index of the source component in its owner's serialized array
     * when filtered to just that component type 
     */
    int32 SourceComponentTypeSerializedIndex;
};

/**
 * cache for component instance data
 * NOTE: does not collect references for GC, so is not safe to GC, if the cache is only reference to a UObject 
 */
class FComponentInstanceDataCache
{
    static void GetComponentHierarchy(const AActor* Actor, TArray<UActorComponent*>& OutComponentHierarchy)
    {
        // we want to apply instance data from the root node down to ensure such as transforms
        OutComponentHierarchy.Reset(Actor->GetComponents().Num());

        auto AddComponentHierarchy = [Actor, &OutComponentHierarchy](USceneComponent* Component)
        {
            int32 FirstProcessIndex = OutComponentHierarchy.Num();

            // add this to our list and make it our starting node
            OutComponentHierarchy.Add(Component);

            int32 CompsToProcess = 1;
            while (CompsToProcess)
            {
                // track how many elements where here:
                const int32 StartingProcessedCount = OutComponentHierarchy.Num();

                // process the currently unprocessed elements
                for (int32 ProcessIndex = 0; ProcessIndex < CompsToProcess; ++ProcessIndex)
                {
                    USceneComponent* SceneComponent = CastChecked<USceneComponent>(OutComponentHierarchy[FirstProcessIndex + ProcessIndex]);

                    // add all children to the end of the array
                    for (int32 ChildIndex = 0; ChildIndex < SceneComponent->GetNumChildrenComponents(); ++ChildIndex)
                    {
                        if (USceneComponent* ChildComponent = SceneComponent->GetChildComponent(ChildIndex))
                        {
                            // we don't want to recurse in to child actors (or any other attached actor) when applying the instance cache
                            // components within child actor are handled by applying the instance data to the child actor component
                            if (ChildComponent->GetOwner() == Actor)
                            {
                                OutComponentHierarchy.Add(ChildComponent);
                            }
                        }
                    }
                }

                // next loop starting with the nodes we just added
                FirstProcessIndex = StartingProcessedCount;
                CompsToProcess = OutComponentHierarchy.Num() - StartingProcessedCount;
            }
        };

        if (USceneComponent* RootComponent = Actor->GetRootComponent())
        {
            AddComponentHierarchy(RootComponent);
        }

        for (UActorComponent* Component : Actor->GetComponents())
        {
            if (USceneComponent* SceneComponent = Cast<USceneComponent>(Component))
            {
                if (SceneComponent != Actor->GetRootComponent())
                {
                    // SceneComponents that aren't attached to the root component hierarchy won't already been processed so process them now
                    // - if there is an unattached scene component
                    // - if there is a scene component attached to another Actor's hierarchy
                    // - if the scene is not registered (likely because bAutoRegister is false or component is marked pending kill), then we may not have successfully attached to our parent and properly been handled
                    USceneComponent* ParentComponent = SceneComponent->GetAttachParent();
                    if ((ParentComponent == nullptr)
                        || (ParentComponent->GetOwner() != Actor)
                        || (!SceneComponent->IsRegistered() && !ParentComponent->GetAttachChildren().Contains(SceneComponent)))
                    {
                        AddComponentHierarchy(SceneComponent);
                    }
                }
            }
            else if (Component)
            {
                OutComponentHierarchy.Add(Component);
            }
        }
    }

    /** iterate over an Actor's components and applies the stored component instance data to each */
    void ApplyToActor(AActor* Actor, const ECacheApplyPhase CacheApplyPhase)
    {
        if (Actor != nullptr)
        {
            TInlineComponentArray<UActorComponent*> Components;
            GetComponentHierarchy(Actor, Components);

            // apply per-instance data
            TBitArray<> ComponentInstanceDataToConsider(true, ComponentsInstanceData.Num());
            for (int32 Index = 0; Index < ComponentsInstanceData.Num(); ++Index)
            {
                if (!ComponentsInstanceData[Index].IsValid())
                {
                    ComponentInstanceDataToConsider[Index] = false;
                }
            }

            const bool bIsChildActor = Actor->IsChildActor();
            for (UActorComponent* ComponentInstance : Components)
            {
                // only try and apply data to 'created by construction script' components
                if (ComponentInstance && (bIsChildActor || ComponentInstance->IsCreatedByConstructionScript()))
                {
                    // cache template here to avoid redundant calls in the loop below
                    const UObject* ComponentTemplate = ComponentInstance->GetArchetype();

                    for (TConstSetBitIterator<> ComponentInstanceDataIt(ComponentInstanceDataToConsider); ComponentInstanceDataIt; ++ComponentInstanceDataIt)
                    {
                        const TStructOnScope<FActorComponentInstanceData>& ComponentInstanceData = ComponentsInstanceData[ComponentInstanceDataIt.GetIndex()];
                        if (ComponentInstanceData->MatchesComponent(ComponentInstance, ComponentTemplate))
                        {
                            ComponentInstanceData->ApplyToComponent(ComponentInstance, CacheApplyPhase);
                            ComponentInstanceDataToConsider[ComponentInstanceDataIt.GetIndex()] = false;
                            break;
                        }
                    }
                }
            }

            // once we're done attaching, if we have any unattached instance components move them to the root
            for (const auto& InstanceTransformPair : InstanceComponentTransformToRootMap)
            {
                USceneComponent* SceneComponent = InstanceTransformPair.Key;
                if (SceneComponent && !IsValid(SceneComponent))
                {
                    SceneComponent->AttachToComponent(Actor->GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);
                    SceneComponent->SetRelativeTransform(InstanceTransformPair.Value);
                }
            }
        }
    }

    /** map of components instance data struct (template -> instance data) */
    TArray<TStructOnScope<FActorComponentInstanceData>> ComponentsInstanceData;

    /** map of the actor instanced scene component to their transform relative to the root */
    TMap<TObjectPtr<USceneComponent>, FTransform> InstanceComponentTransformToRootMap;
};

/**
 * TInlineComponentArray is simply a TArray that reserves a fixed amount of space on the stack
 * to try to avoid heap allocation when there are fewer than a specified number of elements expected in the result 
 */
template <class T, uint32 NumElements = NumInlinedActorComponents>
class TInlineComponentArray : public TArray<T, TInlineAllocator<NumElements>>
{
    typedef TArray<T, TInlineAllocator<NumElements>> Super;

    TInlineComponentArray(const AActor* Actor, bool bIncludeFromChildActors = false)
        : Super()
    {
        if (Actor)
        {
            Actor->GetComponents(*this, bIncludeFromChildActors);
        }
    }
};

DECLARE_DYNAMIC_MULTICAST_SPARSE_DELEGATE_TwoParams(FActorBeginOverlapSignature, AActor, OnActorBeginOverlap, AActor*, OverlappedActor, AActor*, OtherActor);
DECLARE_DYNAMIC_MULTICAST_SPARSE_DELEGATE_TwoParams(FActorEndOverlapSignature, AActor, OnActorEndOverlap, AActor*, OverlappedActor, AActor*, OtherActor);

/**
 * Actor is the base class for an Object that can be placed or spawned in a level
 * Actors may contain a collection of ActorComponents, which can be used to control how actors move, how they are renderered, etc
 * the other main function of an Actor is the replication of properties and function calls across the network during play
 * 
 * Actor initialization has multiple steps, here's the order of important virtual functions that get called:
 * - UObject::PostLoad:
 *     for actors statically placed in a level, the normal UObject PostLoad gets called both in the editor an during gameplay
 *     this is not called for newly spawned actors
 * 
 * - UActorComponent::OnComponentCreated:
 *     when an actor is spawned in the editor or during gameplay, this gets called for any native components
 *     for blueprint-created components, this gets called during construction for that component
 *     this is not called for components loaded from a level
 * 
 * - AActor::PreRegisterAllComponents:
 *     for statically placed actors and spawned actors that have native root components, this gets called now
 *     for blueprint actors without a native root component, these registration functions get called later during construction
 * 
 * - UActorComponent::RegisterComponent:
 *     all components are registered in editor and at runtime, this creates their physical/visual representation
 *     these calls may be distributed over multiple frames, but are always after PreRegisterAllComponents
 * 
 * - AActor::PostRegisterAllComponents:
 *     called for all actors both in the editor and in gameplay, this is the last function that is called in all cases
 * 
 * - AActor::PostActorCreated:
 *     when an actor is created in the editor or during gameplay, this gets called right before construction.
 *     this is not called for components loaded from a level
 * 
 * - AActor::UserConstructionScript:
 *     called for blueprints that implement a construction script
 * 
 * - AActor::OnConstruction:
 *     called at the end of ExecuteConstruction, which calls the blueprint construction script.
 *     this is called after all blueprint-created components are fully created and registered
 *     this is only called during gameplay for spawned actors, and may get rerun in the editor when changing blueprints
 * 
 * - AActor::PreInitializeComponents:
 *     called before InitializeComponent is called on the actor's components
 *     this is only called during gameplay and in certain editor preview windows
 * 
 * - UActorComponent::Activate:
 *     this will be called only if the component has bAutoActivate set
 *     it will also got called later on if a component has bAutoActivate set
 * 
 * - UActorComponent::InitializeComponent:
 *     this will be called only if the component has bWantsInitializeComponentSet
 *     this only happens once per gameplay session
 * 
 * - AActor::PostInitializeComponents:
 *     called after the actor's components have been initialized, only during gameplay and some editor previews
 * 
 * - AActor::BeginPlay:
 *     called when the level starts ticking, only during actual gameplay
 *     this normally happens right after PostInitializeComponents but can be delayed for networked or child actros
 */

// 012 - Foundation - CreateWorld ** - AActor
// haker: 
// - Actor:
//   - AActor == collection of ActorComponents (-> Entity-Component structure)
//   - ActorComponent's examples:
//     - UStaticMeshComponent, USkeletalMeshComponent, UAudioComponent, ... etc.
//   - (networking) the unit of replication for propagating updates for state and RPC calls
//
// - Actor's Initializations:
//   1. UObject::PostLoad:
//     - when you place AActor in ULevel, AActor will be stored in ULevel's package
//     - in game build, when ULevel is streaming (or loaded), the placed AActor is loaded and call UObject::PostLoad()
//       - FYI, the UObject::PostLoad() is called when [object need-to-be-spawned]->[asset need-to-be-load]->[after loaded call post-event UObject::PostLoad()]
//   
//   2. UActorComponent::OnComponentCreated:
//     - AActor is already 'spawned'
//     - notification per UActorComponent
//     - one thing to remember is 'component-creation' first!
//   
//   3. AActor::PreRegisterAllComponents:
//     - pre-event for registering UActorComponents to AActor
//     - one thing to remember:
//       - *** [UActorComponent Creation] ---then---> [UActorComponent Registration] ***
//   
//   4. AActor::RegisterComponent:
//     - incremental-registration: register UActorComponent to AActor over multiple frames
//     - what is register stage done?
//       - register UActorComponent to worlds (GameWorld[UWorld], RenderWorld[FScene], PhysicsWorld[FPhysScene], ...)
//       - ***initialize state*** required from each world
//   
//   5. AActor::PostRegisterAllComponents:
//     - post-event for registering UActorComponents to AActor
//
//   6. AActor::UserConstructionScript:
//     [*] explain UserConstructionScript vs. SimpleConstructionScript in the editor:
//        - UserConstructionScript: called a function for creating UActorComponent in BP event graph
//        - SimpleConstructionScript: construct UActorComponent in BP viewport (hierarchical)
//
//   7. AActor::PreInitializeComponents:
//     - pre-event for initializing UActorComponents of UActor
//     - [UActorComponent Creation]-->[UActorComponent Register]-->[UActorComponent Initialization]
//
//   8. UActorComponent::Activate:
//     - before initializing UActorComponent, UActorComponent's activation is called first
//
//   9. UActorComponent::InitializeComponent:
//     - we can think of UActorComponent's initialization as two stages:
//       1. Activate
//          - register FTickFunction to FTickTaskLevel
//       2. InitializeComponent
//
//  10. AActor::PostInitializeComponents:
//     - post-event when UActorComponents initializations are finished
// 
//  11. AActor::Beginplay:
//     - when level is ticking, AActor calls BeginPlay()
//     - it is called when Actor starts to initialize
//
//  Diagrams:
// ┌─────────────────────┐                                                                    
// │ UObject::PostLoad() │                                                                    
// └─────────┬───────────┘                                                                    
//           │                                                                                
//           │                                                                                
// ┌─────────▼───────────┐      ┌────────────────────────────────────────────────────────────┐
// │ AActor Spawn        ├─────►│                                                            │
// └─────────┬───────────┘      │  UActorComponent Creation:                                 │
//           │                  │   │                                                        │
//           │                  │   └──UActorComponent::OnComponentCreated()                 │
//           │                  │                                                            │
//           │                  ├────────────────────────────────────────────────────────────┤
//           │                  │                                                            │
//           │                  │  UActorComponent Register:                                 │
//           │                  │   │                                                        │
//           │                  │   ├──AActor::PreRegisterAllComponents()                    │
//           │                  │   │                                                        │
//           │                  │   ├──For each UActorComponent in AActor's UActorComponents │
//           │                  │   │   │                                                    │
//           │                  │   │   └──AActor::RegisterComponent()                       │
//           │                  │   │                                                        │
//           │                  │   └──AActor::PostRegisterAllComponents()                   │
//           │                  │                                                            │
//           │                  ├────────────────────────────────────────────────────────────┤
//           │                  │  AActor::UserConstructionScript()                          │
//           │                  ├────────────────────────────────────────────────────────────┤
//           │                  │                                                            │
//           │                  │  UActorComponent Initialization:                           │
//           │                  │   │                                                        │
//           │                  │   ├──AActor::PreInitializeComponents()                     │
//           │                  │   │                                                        │
//           │                  │   ├──For each UActorComponent in AActor's UActorComponents │
//           │                  │   │   │                                                    │
//           │                  │   │   ├──UActorComponent::Activate()                       │
//           │                  │   │   │                                                    │
//           │                  │   │   └──UActorComponent::InitializeComponent()            │
//           │                  │   │                                                        │
//           │                  │   └──AActor::PostInitializeComponents                      │
//           │                  │                                                            │
//           │                  └────────────────────────────────────────────────────────────┘
//           │                                                                                
//  ┌────────▼───────────┐      ┌────────────────────┐                                        
//  │ AActor Preparation ├─────►│ AActor::BeginPlay()│                                        
//  └────────────────────┘      └────────────────────┘                                        

// see Actor's member variables (goto 13)                                                              
class AActor : public UObject
{
    /**
     * returns a list of all actors spawned by our Child Actor Components, including children of children
     * this does not return the contents of the Children array 
     */
    void GetAllChildActors(TArray<AActor*>& ChildActors, bool bIncludeDescendants = true) const
    {
        TInlineComponentArray<UChildActorComponent*> ChildActorComponents(this);

        ChildActors.Reserve(ChildActors.Num() + ChildActorComponents.Num());
        for (UChildActorComponent* CAC : ChildActorComponents)
        {
            if (AActor* ChildActor = CAC->GetChildActor())
            {
                ChildActors.Add(ChildActor);
                if (bIncludeDescendants)
                {
                    ChildActor->GetAllChildActors(ChildActors, true);
                }
            }
        }
    }

    AWorldSettings* GetWorldSettings() const
    {
        UWorld* World = GetWorld();
        return (World ? World->GetWorldSettings() : nullptr);
    }

    ENetRole GetLocalRole() const { return Role; }

    bool HasAuthority() const
    {
        return (GetLocalRole() == ROLE_Authority);
    }

    bool SetActorRotation(FRotator NewRotation, ETeleportType Teleport = ETeleportType::None)
    {
        if (RootComponent)
        {
            return RootComponent->MoveComponent(FVector::ZeroVector, NewRotation, true, nullptr, MOVECOMP_NoFlags, Teleport);
        }

        return false;
    }

    // 030 - Foundation - TickTaskManager - AActor::TickActor
    virtual void TickActor(float DeltaSeconds, ELevelTick TickType, FActorTickFunction& ThisTickFunction) override
    {
        if (IsValidChecked(this) && GetWorld())
        {
            // perform any tick functions unique to an actor subclass
            // haker: finally we reached Tick() function!!!~
            Tick(DeltaSeconds);	
        }
    }

    virtual void Tick(float DeltaSeconds)
    {
        //...
    }
    
    /** Returns this actor's root component. */
    USceneComponent* GetRootComponent() const { return RootComponent; }

    // 071 - Foundation - CreateWorld - AActor::IsChildActor
    // haker: to check whether the actor is attached to another actor, use ParentComponent
    bool IsChildActor() const { return ParentComponent.IsValid(); }

    /** 
     * walk up the attachment chain from RootComponent until we encounter a different actor, and return it 
     * if we are not attached to a component in a different actor, returns nullptr;
     */
    // 070 - Foundation - CreateWorld - AActor::GetAttachParentActor()
    AActor* GetAttachParentActor() const
    {
        // haker: if the current AActor is child-actor of another actor, its rootcomponent's AttachParent is valid
        // - if rootcomponent's AttachParent is valid, the AttachParent is another actor's ActorComponent
        // - from our example, the AttachParent will be Actor0's Component1
        // see GetAttachParent
        if (GetRootComponent() && GetRootComponent()->GetAttachParent())
        {
            return GetRootComponent()->GetAttachParent()->GetOwner();
        }
        return nullptr;
    }

    /** called before all components in the Components array are registered */
    virtual void PreRegisterAllComponents() {}

    /** return the ULevel that this Actor is part of */
    ULevel* GetLevel() const
    {
        // haker: we can know how Actor connected to Level and also to World
        return GetTypedOuter<ULevel>();
    }

    /** 
     * get a direct reference to the components set rather than a copy with the null pointers removed 
     * WARNING: anything that could cause the component to change ownership or be destroyed will invalidate
     * this array, so use caution when iterating this set!
     */
    // 077 - Foundation - CreateWorld - AActor::GetComponents
    const TSet<UActorComponent*>& GetComponents() const
    {
        // haker: ObjectPtrDecal means removing ObjectPtr:
        // - OwnedComponents is TSet<TObjectPtr<UActorComponent>>
        // - Output is TSet<UActorComponent*>
        return ObjectPtrDecay(OwnedComponents);
    }

    /** getter for the cached world pointer, will return null if the actor is not actually spawned in a level */
    virtual UWorld* GetWorld() const override final
    {
        // CDO objects do not belong to a world
        // if the actors outer is destroyed or unreachable we are shutting down and the world should be nullptr
        // haker: it is beneficial to focus on CDO's property: CDO is not belonging to any of worlds
        // - you can think CDO is resided in Core like globally
        if (!HasAnyFlags(RF_ClassDefaultObject)
            // if actor is spawned to specific UWorld, OuterPrivate should be exist!
            && ensure(GetOuter())
            && !GetOuter()->HasAnyFlags(RF_BeginDestroyed)
            && !GetOuter()->IsUnreachable()
            )
        {
            if (ULevel* Level = GetLevel())
            {
                return Level->OwningWorld;
            }
        }
        return nullptr;
    }

    /** virtual call chain to register all tick functions for the actor class hierarchy */
    // 076 - Foundation - CreateWorld - AActor::RegisterActorTickFunctions
    virtual void RegisterActorTickFunctions(bool bRegister)
    {
        if (bRegister)
        {
            if (PrimaryActorTick.bCanEverTick)
            {
                PrimaryActorTick.Target = this;
                PrimaryActorTick.SetTickFunctionEnable(PrimaryActorTick.bStartsWithTickEnabled || PrimaryActorTick.IsTickFunctionEnabled());
                PrimaryActorTick.RegisterTickFunction(GetLevel());
            }
        }
        else
        {
            if (PrimaryActorTick.IsTickFunctionRegistered())
            {
                PrimaryActorTick.UnRegisterTickFunction();
            }
        }

        // haker: cache the current Actor's TickFunction in TLS
        // - this TLS variable could be used in successive virtual call 
        FActorThreadContext::Get().TestRegisterTickFunctions = this;
    }

    /** when called, will call the virtual call chain to register all of the tick functions for both the actor and optionally all components */
    // 074 - Foundation - CreateWorld - AActor::RegisterAllActorTickFunctions
    void RegisterAllActorTickFunctions(bool bRegister, bool bDoComponents)
    {
        if (!IsTemplate())
        {
            // prevent repeated redundant attempts
            if (bTickFunctionsRegistered != bRegister)
            {
                // see FActorThreadContext (goto 075)
                FActorThreadContext& ThreadContext = FActorThreadContext::Get();

                // see RegisterActorTckFunctions (goto 076)
                RegisterActorTickFunctions(bRegister);
                bTickFunctionsRegistered = bRegister;

                // haker: validate TestRegisterTickFunctions updated in RegisterActorTickCuntions, then reset it
                check(ThreadContext.TestRegisterTickFunctions == this);
                ThreadContext.TestRegisterTickFunctions = nullptr;
            }

            // haker: remember we set bDoComponents as false in our previous callstack
            if (bDoComponents)
            {
                for (UActorComponent* Component : GetComponents())
                {
                    if (Component)
                    {
                        Component->RegisterAllComponentTickFunctions(bRegister);
                    }
                }
            }

            if (bAsyncPhysicsTickEnabled)
            {
                //...
            }

            // search (goto 074)
        }
    }

    /** returns whether an actor has had BeginPlay() called on it (and not subsequently has EndPlay() called) */
    bool HasActorBegunPlay() const { return ActorHasBegunPlay == EActorBeginPlayState::HasBegunPlay; }

    /** returns whether an actor is in the process of beginning play */
    bool IsActorBeginningPlay() const { return ActorHasBegunPlay == EActorBeginPlayState::BeginningPlay; }

    /** finish initializing the component and register tick functions and BeginPlay() if it's the proper time to do so */
    // 065 - Foundation - CreateWorld * - AActor::HandleRegisterComponentWithWorld
    void HandleRegisterComponentWithWorld(UActorComponent* Component)
    {
        const bool bOwnerBeginPlayStarted = HasActorBegunPlay() || IsActorBeginningPlay();

        // haker: if component is not initialized, try to initialize the component
        // see IsActorInitialized()
        if (!Component->HasBeenInitialized() && Component->bWantsInitializeComponent && IsActorInitialized())
        {
            Component->InitializeComponent();

            // the component was finally initialized, it can now be replicated
            // NOTE that if this component does not ask to be initialized, it would have started to be replicated inside AddOwnedComponent 
            // haker: in editor, if we don't play in PIE, BeginPlay() isn't called
            if (bOwnerBeginPlayStarted)
            {
                // haker: we skip the replication stuff for now
                AddComponentForReplication(Component);
            }
        }

        if (bOwnerBeginPlayStarted)
        {
            // haker: when are we getting into this code?
            // - maybe actor is activated, and it will be triggered, if we add new component to actor which is already activated
            Component->RegisterAllComponentTickFunctions(true);
            if (!Component->HasBegunPlay())
            {
                // haker: we skip BeginPlay for now
                Component->BeginPlay();
            }
        }
    }

    /**
     * internal helper function to call a compile-time lambda on all components of a given-type
     * use template parameter bClassIsActorComponent to avoid doing unnecessary IsA checks when the ComponentClass is exactly UActorComponent
     * use template parameter bIncludeFromChildActors to recurse in to ChildActor components and find components of the appropriate type in those actors as well  
     */
    // 080 - Foundation - CreateWorld - template<ComponentType, bClassIsActorComponent, bIncludeFromChildActors, typename Func> ForEachComponent_Internal
    template <class ComponentType, bool bClassIsActorComponent, bool bIncludeFromChildActors, typename Func>
    void ForEachComponent_Internal(TSubclassOf<UActorComponent> ComponentClass, Func InFunc) const
    {
        check(bClassIsActorComponent == false || ComponentClass == UActorComponent::StaticClass());
        check(ComponentClass->IsChildOf(ComponentType::StaticClass()));

        // static check, so that the most common case (bIncludeFromChildActors) doesn't need to allocate an additional array
        // haker: we are evaluating branch (condition) here:
        // - in compiled-time, it was fixed how the logic is flowed
        if (bIncludeFromChildActors)
        {
            TArray<AActor*, TInlineAllocator<NumInlinedActorComponents>> ChildActors;
            for (UActorComponent* OwnedComponent : OwnedComponents)
            {
                if (OwnedComponent)
                {
                    // haker: by calling IsA(), we filter ActorComponent type
                    if (bClassIsActorComponent || OwnedComponent->IsA(ComponentClass))
                    {
                        InFunc(static_cast<ComponentType*>(OwnedComponent));
                    }

                    // haker: do you remember what ChildActorComponent is?
                    // - for child-actor, we accumulate child-actor separately rather calling them in hierarchical manner
                    if (UChildActorComponent* ChildActorComponent = Cast<UChildActorComponent>(OwnedComponent))
                    {
                        if (AActor* ChildActor = ChildActorComponent->GetChildActor())
                        {
                            ChildActors.Add(ChildActor);
                        }
                    }
                }
            }

            // haker: recursively call ForEachComponent_Internal() for each ChildActor
            for (AActor* ChildActor : ChildActors)
            {
                ChildActor->ForEachComponent_Internal<ComponentType, bClassIsActorComponent, bIncludeFromChildActors>(ComponentClass, InFunc);
            }
        }
        else
        {
            // haker: if we don't specify to include child-actor, just call OwnedComponents
            for (UActorComponent* OwnedComponent : OwnedComponents)
            {
                if (OwnedComponent)
                {
                    if (bClassIsActorComponent || OwnedComponent->IsA(ComponentClass))
                    {
                        InFunc(static_cast<ComponentType*>(OwnedComponent));
                    }
                }
            }
        }
    }

    // 079 - Foundation - CreateWorld - template<ComponentType, typename Func> ForEachComponent_Internal
    // haker: focus on how it uses ForEachComponent_Internal
    // - bIsIncludeFromChildActor is dynamic variables (not static-compiled variable)
    //   - so we call separate ForEachComponent_Internal with static-compiled variable
    template <class ComponentType, typename Func>
    void ForEachComponent_Internal(TSubclassOf<UActorComponent> ComponentClass, bool bIncludeFromChildActors, Func InFunc) const
    {
        if (ComponentClass == UActorComponent::StaticClass())
        {
            if (bIncludeFromChildActors)
            {
                // see ForEachComponent_Internal (goto 080)
                ForEachComponent_Internal<ComponentType, true /*bClassIsActorComponent*/, true /*bIncludeFromChildActors*/>(ComponentClass, InFunc);
            }
            else
            {
                ForEachComponent_Internal<ComponentType, true /*bClassIsActorComponent*/, false /*bIncludeFromChildActors*/>(ComponentClass, InFunc);
            }
        }
        else
        {
            if (bIncludeFromChildActors)
            {
                ForEachComponent_Internal<ComponentType, false /*bClassIsActorComponent*/, true /*bIncludeFromChildActors*/>(ComponentClass, InFunc);
            }
            else
            {
                ForEachComponent_Internal<ComponentType, false /*bClassIsActorComponent*/, false /*bIncludeFromChildActors*/>(ComponentClass, InFunc);
            }
        }
    }

    /**
     * UActorComponent specialization of GetComponents() to avoid unnecessaray casts
     * it is recommended to use TArrays with a TInlineAllocator to potentially avoid memory allocation costs
     * TInlineComponentArray is defined to make this easier, for example:
     * {
     *  TInlineComponentArray<UActorComponent*> PrimComponents;
     *  Actor->GetComponents(PrimComponents);
     * } 
     */
    // 078 - Foundation - CreateWorld - template AActor::GetComponents
    // haker: you can specify whether you include ChildActors or not
    template <class AllocatorType>
    void GetComponents(TArray<UActorComponent*, AllocatorType>& OutComponents, bool bIncludeFromChildActors = false)
    {
        OutComponents.Reset();
        // see ForEachComponent_Internal (goto 079)
        ForEachComponent_Internal<UActorComponent>(UActorComponent::StaticClass(), bIncludeFromChildActor, [&](UActorComponent* InComp)
        {
            OutComponents.Add(InComp);
        });
    }

    /**
     * called after all the components in the Components array are registered, called both in editor and during gameplay
     * bHasRegisteredAllComponents must be set true prior to calling this function
     */
    virtual void PostRegisterAllComponents()
    {
        ensure(bHasRegisteredAllComponents == true);
    }

    /** incrementally registers components associated with this actor, used during level streaming */
    // 073 - Foundation - CreateWorld - AActor::IncrementalRegisterComponents
    bool IncrementalRegisterComponents(int32 NumComponentsToRegister, FRegisterComponentContext* Context = nullptr)
    {
        if (NumComponentsToRegister == 0)
        {
            // 0 - means register all components
            NumComponentsToRegister = MAX_int32;
        }

        UWorld* const World = GetWorld();

        // if we are not a game world, then register tick functions now, if we are a game world we wait until right before BeginPlay()
        // so as to not actually tick until BeginPlay() executes (which could otherwise happen in network games)
        // haker: PIE is GameWorld!
        if (!World->IsGameWorld())
        {
            // haker: in our case, we analyze the engine code in editor environment, let's get into this function
            // - note that bDoComponents is false!
            // - when debugging in PIE, we will not enter this function:
            //   - BUT, lets look into it
            // see RegisterAllActorTickFunctions (goto 074)
            RegisterAllActorTickFunctions(true, false);
        }

        // register RootComponent first so all other children components can reliably use it (i.e., call GetLocation) when they register
        if (RootComponent != nullptr && !RootComponent->IsRegistered())
        {
            if (RootComponent->bAutoRegister)
            {
                // before we register our component, save it to our transaction buffer so if "undone" it will return to an unregistered state
                // this should prevent unwanted components hanging around when undoing a copy/paste or duplication action
                // haker: we skip to get into Modify() function:
                // - Modify() will add history (or information) into transaction buffer
                // - transaction buffer is used to deal with undo operation
                RootComponent->Modify(false);

                RootComponent->RegisterComponentWithWorld(World, Context);
            }
        }
        
        // see GetComponents (goto 077)
        // see template GetComponents (goto 078)
        TInlineComponentArray<UActorComponent*> Components;
        GetComponents(Components);

        // haker:
        // NumTotalRegisteredComponents - includes previous registered components
        // NumRegisteredComponentsThisRun - do not include previous registered components:
        //   - incremental-update variable to limit by NumComponentsToRegister
        int32 NumTotalRegisteredComponents = 0;
        int32 NumRegisteredComponentsThisRun = 0;

        // haker: we start from 0 to Components.Num()
        TSet<UActorComponent*> RegisteredParents;
        for (int32 CompIdx = 0; CompIdx < Components.Num() && NumRegisteredComponentsThisRun < NumComponentsToRegister; ++CompIdx)
        {
            UActorComponent* Component = Components[CompIdx];

            // haker: we skip already registered ActorComponent:
            // - we do incremental-update, so it could be already registered
            if (!Component->IsRegistered() && Component->bAutoRegister && IsValidChecked(Component))
            {
                // ensure that all parents are registered first
                // see GetUnregisteredParent (goto 081)
                USceneComponent* UnregisteredParentComponent = GetUnregisteredParent(Component);

                // haker: if all parent components are registered, UnregisteredParentComponent will be nullptr
                if (UnregisteredParentComponent)
                {
                    bool bParentAlreadyHandled = false;
                    // haker: see RegisteredParents, now we can understand what this variable is for
                    RegisteredParents.Add(UnregisteredParentComponent, &bParentAlreadyHandled);
                    if (bParentAlreadyHandled)
                    {
                        UE_LOG(LogActor, Error, TEXT("AActor::IncrementalRegisterComponents parent component '%s' cannot be registered in actor '%s'"), *GetPathNameSafe(UnregisteredParentComponent), *GetPathName());
                        break;
                    }

                    // register parent first, then return to this component on a next iteration
                    // haker: let's think about how we process unregistered parent
                    // - can you imagine how this works?
                    // - Diagrams:                                                                                                                              
                    //   ┌───AActor::OwnedComponents─────────────────────────────────────────────────────────────────────────────────────────────────────┐
                    //   │                                                                                                                               │
                    //   │                                                                                                                               │
                    //   │                                                    ┌────AttachParent───┐                                                      │
                    //   │                                                    │                   │                                                      │
                    //   │                                                    │                   │                                                      │
                    //   │       ┌────────────┐     ┌────────────┐      ┌─────┴──────┐      ┌─────▼──────┐      ┌────────────┐       ┌────────────┐      │
                    //   │       │ Component0 ├─────┤ Component1 ├──────┤ Component2 ├──────┤ Component3 ├──────┤ Component4 ├───────┤ Component5 │      │
                    //   │       └─────┬──────┘     └────────────┘      └─────▲──────┘      └─────┬──────┘      └────────────┘       └──────▲─────┘      │
                    //   │             │                                      │                   │                                         │            │
                    //   │             │                                      │                   │                                         │            │
                    //   │             └──────────────AttachParent────────────┘                   └──────────────AttachParent───────────────┘            │
                    //   │                                                                                                                               │
                    //   │                                                                                                                               │
                    //   │                                                                                                                               │
                    //   └───────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────┘
                    //
                    //    1. CompIdx == 0
                    //       - Component == Component0 (Components[0])
                    //       - UnregisteredParentComponent == Component2
                    //       - CompIdx == -1
                    //       - NumTotalRegisteredComponents == -1
                    //       - NumRegisteredComponentsThisRun == 1
                    //    2. CompIdx == 0 (0 = -1 + 1)
                    //       - Component == Component0 (Components[0])
                    //       - UnregisteredParentComponent == Component3
                    //       - CompIdx == -1
                    //       - NumTotalRegisteredComponents == -1
                    //       - NumRegisteredComponentsThisRun == 2
                    //    3. CompIdx == 0 (0 = -1 + 1)
                    //       - Component == Component0 (Components[0])
                    //       - UnregisteredParentComponent == Component5
                    //       - CompIdx == -1
                    //       - NumTotalRegisteredComponents == -1
                    //       - NumRegisteredComponentsThisRun == 3
                    //    4. CompIdx == 0 (0 = -1 + 1)
                    //       - Component == Component0 (Components[0])
                    //       - UnregisteredParentComponent == nullptr **** finally can update Component0
                    //       - CompIdx == 0
                    //       - NumTotalRegisteredComponents == 1
                    //       - NumRegisteredComponentsThisRun == 4
                    //    5. CompIdx == 1 
                    //       - Component == Component1 (Components[1])
                    //       - UnregisteredParentComponent == nullptr
                    //       - CompIdx == 1
                    //       - NumTotalRegisteredComponents == 2
                    //       - NumRegisteredComponentsThisRun == 5
                    //    6. CompIdx == 2
                    //       - Component == Component2 (Components[2]) **** it is already updated
                    //       - NumTotalRegisteredComponents == 3
                    //       - NumRegisteredComponentsThisRun == 5 *** we are NOT update component, so count is remained
                    //    7. CompIdx == 3
                    //       - Component == Component3 (Components[3]) **** it is already updated
                    //       - NumTotalRegisteredComponents == 4
                    //       - NumRegisteredComponentsThisRun == 5 *** we are NOT update component, so count is remained
                    Component = UnregisteredParentComponent;
                    CompIdx--;
                    NumTotalRegisteredComponents--; // because we will try to register the parent again later
                }

                // before we register our component, save it to our transaction buffer so if 'undone' it will return to an unregistered state
                // This should prevent unwanted components hanging around when undoing a copy/paste or duplication action.
                Component->Modify(false);

                Component->RegisterComponentWithWorld(World, Context);
                NumRegisteredComponentsThisRun++;
            }

            NumTotalRegisteredComponents++;
        }

        // see whether we are done
        if (Components.Num() == NumTotalRegisteredComponents)
        {
            // haker: we are DONE with incremental-component-update!
            bHasRegisteredAllComponents = true;
            // finally, call PostRegisterAllComponents
            PostRegisterAllComponents();
            // after all components have been registered the actor is considered fully added; notify the owning world
            World->NotifyPostRegisterAllActorComponents(this);
            return true;
        }

        return false;
    }

    /** returns whether an actor has been initialized for gameplay */
	bool IsActorInitialized() const { return bActorInitialized; }

    /** iterate over components array and call InitializeComponent, which happens once per actor */
    // 008 - Foundation - SpawnActor - AActor::InitializeComponents
    void InitializeComponents()
    {
        // haker: collect all components from Actor and iterate them
        TInlineComponentArray<UActorComponent*> Components;
        GetComponents(Components);

        for (UActorComponent* ActorComp : Components)
        {
            // haker: from our callstack, all components should be registered, so this condition will be 'true'
            if (ActorComp->IsRegistered())
            {
                // haker: Activate() enables the component's tick function
                // - note that only when bAutoActivate is enabled
                // - otherwise, on BeginPlay(), the component's tick function will be registered
                if (ActorComp->bAutoActivate && !ActorComp->IsActive())
                {
                    ActorComp->Activate(true);
                }

                // haker: call InitializeComponent() for each component
                if (ActorComp->bWantsInitializeComponent && !ActorComp->HasBeenInitialized())
                {
                    // broadcast the activation event since Activate occurs too early to fire a callback in a game
                    ActorComp->InitializeComponent();
                }
            }
        }
    }

    /** allow actors to initialize themselves on the C++ side after all of their components have been initialized, only called during gameplay */
    virtual void PostInitializeComponents()
    {
        if (IsValidChecked(this))
        {
            bActorInitialized = true;
            UpdateAllReplicatedComponents();
        }
    }

    /** sets the value of Instigator without causing other side effects to this instance */
    void SetInstigator(APawn* InInstigator)
    {
        Instigator = InInstigator;
    }

    /** ensure that all the components in the Components array are registered */
    virtual void RegisterAllComponents()
    {
        PreRegisterAllComponents();

        // 0 - means register all components
        bool bAllRegistered = IncrementalRegisterComponents(0);
        check(bAllRegistered);

        // clear this flag as it's no longer deferred
        bHasDeferredComponentRegistration = false;
    }

    /** set root component to be the specified component: NewRootComponent's owner should be this actor */
    bool SetRootComponent(USceneComponent* NewRootComponent)
    {
        // only components owned by this actor can be used as a its root component
        if (ensure(NewRootComponent == nullptr || NewRootComponent->GetOwner() == this))
        {
            if (RootComponent != NewRootComponent)
            {
                Modify();

                USceneComponent* OldRootComponent = RootComponent;
                RootComponent = NewRootComponent;

                // notify new root first, as it probably has no delegate on it
                if (NewRootComponent)
                {
                    NewRootComponent->NotifyIsRootComponentChanged(true);
                }

                if (OldRootComponent)
                {
                    OldRootComponent->NotifyIsRootComponentChanged(false);
                }
            }
        }
    }

    /**
     * Construction script, the place to spawn components and do other setup
     * @NOTE: Name used in CreateBlueprint function 
     */
    UFUNCTION(BlueprintImplementableEvent, meta=(BlueprintInternalUseOnly="true", DisplayName="Construction Script"))
    void UserConstructionScript();

    /** runs UserConstructionScript, delays component reregistration until it's complete */
    // 012 - Foundation - ExecuteConstruction * - AActor::ProcessUserConstructionScript
    void ProcessUserConstructionScript()
    {
        // set a flag that this actor is currently running UserConstructionScript
        bRunningUserConstructionScript = true;
        // haker: see UserConstructionScript() BP function briefly
        UserConstructionScript();
        bRunningUserConstructionScript = false;

        // validate component mobility after UCS execution
        // haker: fixup mobility after running UCS call
        for (UActorComponent* Component : GetComponents())
        {
            if (USceneComponent* SceneComponent = Cast<USceneComponent>(Component))
            {
                // a parent component can't be more mobile than its children, so we check for that here and adjust as needed
                // haker: see EComponentMobility (goto 013: ExecuteConstruction)
                if (SceneComponent != RootComponent && SceneComponent->GetAttachParent() != nullptr && SceneComponent->GetAttachParent()->Mobility > SceneComponent->Mobility)
                {
                    if (SceneComponent->IsA<UStaticMeshComponent>())
                    {
                        // SMCs can't be stationary, so always set them (and any children) to be movable
                        // haker: for StaticMeshComponent:
                        // - if AttachParent's Mobility is bigger than SceneComponent's Mobility
                        //   - there is only two cases 'Stationary' or 'Movable'
                        //   - StaticMeshComponent can not be 'Stationary', so update it as 'Movable'
                        SceneComponent->SetMobility(EComponentMobility::Movable);
                    }
                    else
                    {
                        // set the new component (and any children) to be at least as mobile as its parent
                        SceneComponent->SetMobility(SceneComponent->GetAttachParent()->Mobility);
                    }
                }
            }
        }
    }

    /**
     * Called when an instance of this class is placed (in editor) or spawned.
     */
    virtual void OnConstruction(const FTransform& Transform) {}

    /** run any construction script for this Actor; will call OnConstruction */
    // 001 - Foundation - ExecuteConstruction * - AActor::ExecuteConstruction
    // haker: SCS and UCS is related to 'construction'
    // - we can understand ExecuteConstruction() as construction phase
    bool ExecuteConstruction(const FTransform& Transform, const struct FRotationConversionCache* TransformRotationCache, const class FComponentInstanceDataCache* InstanceDataCache, bool bIsDefaultTransform = false, ESpawnActorScaleMethod TransformScaleMethod = ESpawnActorScaleMethod::OverrideRootScale)
    {
        // haker: do you remember what ON_SCOPE_EXIT is done?
        bActorIsBeingConstructed = true;
        ON_SCOPE_EXIT
        {
            bActorIsBeingConstructed = false;
        };

        // haker: before getting into the source code of ExecuteConstruction(), see the parameter types
        // - TransformRotationCache is nullptr
        // - InstanceDataCache is also nullptr:
        //   - what we are interested in is FinishSpawning() in SpawnActor(), which passed it as nullptr
        // - TransformScaleMethod's type, ESpawnActorScaleMethod, will be covered later

        // ensure that any existing native root component gets this new transform
        // we can skip this in the default case as the given transform will be the root component's transform 
        
        // haker: bIsDefaultTransform is passed as 'true' in SpawnActor():
        // - as we covered it in UpdateComponentToWorld(), it is heavy to call UpdateComponentToWorld
        if (RootComponent && !bIsDefaultTransform)
        {
            // haker: we pass this as nullptr, so skip the logic
            if (TransformRotationCache)
            {
                //...
            }
            RootComponent->SetWorldTransform(Transform, /*bSweep=*/false, /*OutSweepHitResult=*/nullptr, ETeleportType::TeleportPhysics);
        }

        // generate the parent blueprint hierarchy for this actor, so we can run all the construction scripts sequentially
        // haker: collect all inherited BlueprintGeneratedClass which has SCS and UCS
        // - see UBlueprintGeneratedClass (goto 002: ExecuteConstruction)
        TArray<const UBlueprintGeneratedClass*> ParentBPClassStack;

        // haker: see UBlueprintGeneratedClass::GetGeneratedClassesHierarchy (goto 009: ExecuteConstruction)
        const bool bErrorFree = UBlueprintGeneratedClass::GetGeneratedClassesHierarchy(GetClass(), ParentBPClassStack);

        // if this actor has a blueprint linage, go ahead and run the construction scripts from least derived to most
        // haker: first see 'else' statement (goto 011: ExecuteConstruction)
        if (ParentBPClassStack.Num() > 0)
        {
            // 013 - Foundation - ExecuteConstruction * - 'if' statement
            if (bErrorFree)
            {
                // get all components owned by the given actor prior to SCS execution
                // note: GetComponents() internally does a NULL check, so we can assume here that all entries are valid

                // haker: overall process of ExecuteConstruction():
                // - Diagram:
                //  [ExecuteConstruction]                                                                            
                //                                                                                                   
                //   0.Collect ParentBPClassStack by UBPC::GetGeneratedClassHierarchy()                              
                //                                                                                                   
                //      AActor◄───────BP0◄──────BP1                                                                  
                //                                                                                                   
                //      *** [BP0, BP1] are collected                                                                 
                //                                                                                                   
                //                                                                                                   
                //   1. Collect NativeSceneComponents(EComponentCreationMethod::Native)                              
                //                                                                                                   
                //                                                                                                   
                //   2. Iterating ParentPBClassStack [BP0 ──► BP1]                                                   
                //       │                                                                                           
                //       └──Execute USimpleConstructionScript::ExecuteScriptOnActor()                                
                //           : we pass collected 'NativeSceneComponents'                                             
                //           │                                                                                       
                //           └── Iterating RootNodes in SCS:                                                         
                //                │                                                                                  
                //                └──Call USCS_Node::ExecuteOnActor()                                                
                //                    │                                                                              
                //                    └──Iterating ChildNodes, call ExecuteOnActor() recursively                     
                //                                                                                                   
                //   3. Collect ActorComponents again (after SCS construction)                                       
                //       │                                                                                           
                //       └──Register Component by USimpleConstructionScript::RegisterInstancedComponent()            
                //                                                                                                   
                //            *** NOTE that Registering USceneComponent call OnRegister()                            
                //                 : All pending Attachments are processed by AttachToComponent() here               
                //                                                                                                   
                //   4. SCS is constructed, then execute UserConstructionScript with ProcessUserConstructionScript() 
                //                                                                                                                   

                // haker: before executing SCS(SimpleConstructionScripts), collect all components (from OwnedComponents in AActor):
                // - GetComponents() validates UObject(UActorComponent), so no need to worry about null object
                TInlineComponentArray<UActorComponent*> PreSCSComponents;
                GetComponents(PreSCSComponents);

                // determine the set of native scene components that SCS nodes can attach to
                // haker: (1) when creation method is 'Native', add components to 'NativeSceneComponents' 
                TInlineComponentArray<USceneComponent*> NativeSceneComponents;
                for (UActorComponent* ActorComponent : PreSCSComponents)
                {
                    if (USceneComponent* SceneComponent = Cast<USceneComponent>(ActorComponent))
                    {
                        // exclude subcomponents of native components as these could unintentially be matched by name during SCS execution and also exclude instance-only components
                        if (SceneComponent->CreationMethod == EComponentCreationMethod::Native && SceneComponent->GetOwner()->IsA<AActor>())
                        {
                            NativeSceneComponents.Add(SceneComponent);
                        }
                    }
                }

                // haker: we iterate Outermost Parent first:
                // - note that ParentBPClassStack is accumulated by iterating parent of BlueprintGeneratedClass
                // - e.g:
                //   class A {};
                //   class B : public A {};
                //   class C : public B {};
                //   
                //   ParentBPClassStack: [C, B, A]
                //   the way of iterating ParentBPClassStack is: A -> B -> C
                // 
                // - (2) iterating ParentBPClassStack, call BP's ExecuteScriptOnActor of SCS
                for (int32 i = ParentBPClassStack.Num() - 1; i >= i--)
                {
                    const UBlueprintGeneratedClass* CurrentBPGClass = ParentBPClassStack[i];
                    check(CurrentBPGClass);

                    // haker: now you should understand BPClass is special Unreal Class which has its own SCS which is the hierarchy of components
                    USimpleConstructionScript* SCS = CurrentBPGClass->SimpleConstructionScript;
                    if (SCS)
                    {
                        // see USimpleConstructionScript::ExecuteScriptOnActor (goto 014: ExecuteConstruction)
                        SCS->ExecuteScriptOnActor(this, NativeSceneComponents, Transform, TransformRotationCache, bIsDefaultTransform, TransformScaleMethod);
                    }
                }

                // ensure that we've called RegisterAllComponents(), in case it was deferred and the SCS could not be fully executed
                if (HasDeferredComponentRegistration() && GetWorld()->bIsWorldInitialized)
                {
                    RegisterAllComponents();
                }

                // once SCS execution has finished, we do a final pass to register any new comments that may have been deferred or were otherwise left unregistered after SCS execution
                // haker: (3) register rest of components who aren't registered yet
                TInlineComponentArray<UActorComponent*> PostSCSComponents;
                GetComponents(PostSCSComponents);
                for (UActorComponent* ActorComponent : PostSCSComponents)
                {
                    // limit registration to components that are known to have been created during SCS execution
                    if (!ActorComponent->IsRegistered() 
                        && ActorComponent->bAutoRegister 
                        && IsValidChecked(ActorComponent) 
                        && GetWorld()->bIsWorldInitialized
                        && (ActorComponent->CreateMethod == EComponentCreationMethod::SimpleConstructionScript || !PreSCSComponents.Contains(ActorComponent)))
                    {
                        USimpleConstructionScript::RegisterInstancedComponent(ActorComponent);
                    }
                }

                // if we passed in cached data, we apply it now, so that the UserConstructionScript can use the updated values
                // haker: other than calling RerunConstructionScript() in Editor, InstanceDataCache is usually nullptr:
                // - the cache is for optimizing RerunConstructionScript() to reduce duplicated function calls
                // - for now, skip InstanceDataCache
                if (InstanceDataCache)
                {
                    InstanceDataCache->ApplyToActor(this, ECacheApplyPhase::PostSimpleConstructionScript);
                }

                // then run the user script, which is responsible for calling its own super, if desired
                // haker: we already had covered:
                // - (4) UserConstructionScript is called after constructing SCS(SimpleConstructionScript)
                {
                    ProcessUserConstructionScript();
                }

                // since re-run construction scripts will never be run and we want to keep dynamic spawning fast, don't spend time determining the UCS modified properties in game worlds
                // haker: note that only when current world is not GameWorld (e.g. Editor World)
                // - we are interested in SpawnActor used in GameWorld
                if (!GetWorld()->IsGameWorld())
                {
                    for (UActorComponent* Component : GetComponents())
                    {
                        if (Component)
                        {
                            Component->DetermineUCSModifiedProperties();
                        }
                    }
                }

                // apply any cached data procedural components
                if (InstanceDataCache)
                {
                    InstanceDataCache->ApplyToActor(this, ECacheApplyPhase::PostUserConstructionScript);
                }
            }
            else
            {
                // disaster recovery mode
                //...
            }
        }
        else
        {
            // 011 - Foundation - ExecuteConstruction * - 'else' statement
            // Then run the user script, which is responsible for calling its own super, if desired
            {
                // see ProcessUserConstructionScript (goto 012: ExecuteConstruction)
                ProcessUserConstructionScript();
            }
            // goto 013: ExecuteConstruction
        }

        OnConstruction(Transform);

        return bErrorFree;
    }

    /** pushes this actor on to the stack of input being handled by a PlayerController */
    // 041 - Foundation - CreateInnerProcessPIEGameInstance - AActor::EnableInput
    virtual void EnableInput(class APlayerController* PlayerController)
    {
        if (PlayerController)
        {
            // if it doesn't exist create it and bind delegates
            // haker: each Actor has the holder for InputComponent
            if (!InputComponent)
            {
                // haker: create new InputComponent and bind input delegates
                InputComponent = NewObject<UInputComponent>(this, UInputSettings::GetDefaultInputComponentClass());
                InputComponent->RegisterComponent();
                InputComponent->bBlockInput = bBlockInput;
                InputComponent->Priority = InputPriority;

                UInputDelegateBinding::BindInputDelegatesWithSubojects(this, InputComponent);
            }
            else
            {
                // Make sure we only have one instance of the InputComponent on the stack
                // haker: pop an InputComponent from PC
                // - see APC::PopInputComponent briefly
                PlayerController->PopInputComponent(InputComponent);
            }

            // haker: push new 'InputComponent' to PC, to get events from LocalPlayer
            // - see PushInputComponent briefly
            // - recommend to read APlayerController::TickPlayerInput() to understand how InputComponent processes input events
            PlayerController->PushInputComponent(InputComponent);
        }
    }

    /** called right before components are initialized, only called during gameplay */
    // 040 - Foundation - CreateInnerProcessPIEGameInstance - AActor::PreInitializeComponents
    virtual void PreInitializeComponents()
    {
        // haker: if this actor is marked as receiving inputs
        // - see AutoReceiveInput(EAutoReceiveInput) briefly
        if (AutoReceiveInput != EAutoReceiveInput::Disabled)
        {
            // haker: AutoReceiveInput has PlayerIndex where we try to get inputs
            const int32 PlayerIndex = int32(AutoReceiveInput.GetValue()) - 1;

            // haker: iterating LocalPlayer in the world, and find Player's PlayerController
            APlayerController* PC = UGameplayStatics::GetPlayerController(this, PlayerIndex);
            if (PC)
            {
                // haker: when we get here, Player is initialized already with its PlayerController
                // - see EnableInput (goto 041: CreateInnerProcessPIEGameInstance)
                EnableInput(PC);
            }
            else
            {
                // haker: our Player's PC is not initialized yet, so register pending list to the Level
                // - see RegisterActorForAutoReceiveInput (goto 042: CreateInnerProcessPIEGameInstance)
                // - note that we pass 'PlayerIndex'
                GetWorld()->PersistLevel->RegisterActorForAutoReceiveInput(this, PlayerIndex);
            }
        }
    }

    /** internal helper to update overlaps during Actor initialization/BeginPlay correctly based on the UpdateOverlapsMethodDuringLevelStreaming and bGenerateOverlapEventsDuringLevelStreaming settings */
    void UpdateInitialOverlaps(bool bFromLevelStreaming)
    {
        if (!bFromLevelStreaming)
        {
            UpdateOverlaps();
        }
        else
        {
            // NOTE: conditionally doing notifies here since loading or streaming in isn't actually conceptually beginning a touch
            //       rather, it was always touching and the mechanics of loading is just an implementation detail
            if (bGenerateOverlapEventsDuringLevelStreaming)
            {
                UpdateOverlaps(bGenerateOverlapEventsDuringLevelStreaming);
            }
            else
            {
                bool bUpdateOverlaps = true;
                const EActorUpdateOverlapsMethd UpdateMethod = GetUpdateOverlapsMethodDuringLevelStreaming();
                switch (UpdateMethod)
                {
                case EActorUpdateOverlapsMethod::OnlyUpdateMovable:
                    bUpdateOverlaps = IsRootComponentMovable();
                    break;
                case EActorUpdateOverlapsMethod::NeverUpdate:
                    bUpdateOverlaps = false;
                    break;
                case EActorUpdateOverlapsMethod::AlwaysUpdate:
                default:
                    bUpdateOverlaps = true;
                    break;
                }

                if (bUpdateOverlaps)
                {
                    UpdateOverlaps(bGenerateOverlapEventsDuringLevelStreaming);
                }
            }
        }
    }

    /** overridable native event for when play begins for this actor */
    // 010 - Foundation - SpawnActor - AActor::BeginPlay
    virtual void BeginPlay()
    {
        // components are done below
        // haker: note that 'bDoComponents' is 'false'
        RegisterAllActorTickFunctions(true, false);

        // haker: iterate Actor's OwnedComponents
        TInlineComponentArray<UActorComponent*> Components;
        GetComponents(Components);
        for (UActorComponent* Component : Components)
        {
            // bHasBegunPlay will be true for the component if the component was renamed and moved to a new outer during initialization
            if (Component->IsRegistered() && !Component->HasBegunPlay())
            {
                // haker: making sure that register all components' tick functions
                Component->RegisterAllComponentTickFunctions(true);

                // see UActorComponent::BeginPlay(goto 011: SpawnActor)
                Component->BeginPlay();
            }
        }

        ReceiveBeginPlay();
        ActorHasBegunPlay = EActorBeginPlayState::HasBegunPlay;
    }

    /** initiate a begin play call on this Actor, will handle calling in the correct order */
    // 009 - Foundation - SpawnActor - AActor::DispatchBeginPlay
    void DispatchBeginPlay(bool bFromLevelStreaming = false)
    {
        UWorld* World = (!HasActorBegunPlay() && IsValidChecked(this) ? GetWorld() : nullptr);
        if (World)
        {
            bActorBeginningPlayFromLevelStreaming = bFromLevelStreaming;
            ActorHasBegunPlay = EActorBeginPlayState::BeginningPlay;

            // see AActor::BeginPlay(goto 010: SpawnActor)
            BeginPlay();

            if (IsValidChecked(this))
            {
                // initialize overlap state
                UpdateInitialOverlaps(bFromLevelStreaming);
            }

            bActorBeginningPlayFromLevelStreaming = false;
        }
    }

    /** move the actor instantly to the specified location and rotation */
    bool SetActorLocationAndRotation(FVector NewLocation, FRotator NewRotation, bool bSweep=false, FHitResult* OutSweepHitResult=nullptr, ETeleportType Teleport = ETeleportType::None)
    {
        if (RootComponent)
        {
            const FVector Delta = NewLocation - GetActorLocation();
            return RootComponent->MoveComponent(Delta, NewRotation, bSweep, OutSweepHitResult, MOVECOMP_NoFlags, Teleport);
        }
        return false;
    }

    /** called after the actor has run its construction: responsible for finishing the actor spawn process */
    // 007 - Foundation - SpawnActor - PostActorConstruction
    void PostActorConstruction()
    {
        // haker: focus on 'bActorsInitialized':
        // - bActorsInitialized indicates the world is already 'BeginPlay'ed
        // - in this scenario, SpawnActor is called at run-time
        //   - so we need to complete initialize components
        UWorld* const World = GetWorld();
        bool const bActorsInitialized = World && World->AreActorsInitialized();

        if (bActorsInitialized)
        {
            PreInitializeComponents();
        }

        // if this is dynamically spawned replicated actor, defer calls to BeginPlay and UpdateOverlaps until replicated properties are deserialized
        // haker: this variable is related to replicated actor stuff, so read the rest of codes as 'bDeferBeginPlayAndUpdateOverlaps' is false
        const bool bDeferBeginPlayAndUpdateOverlaps = (bExchangedRoles && RemoveRole == ROLE_Authority) && !GIsReinstancing;

        if (bActorsInitialized)
        {
            // call InitializeComponent on components
            // haker: we try to initialize all components attached to Actor
            // - see AActor::InitializeComponents(goto 008: SpawnActor)
            InitializeComponents();

            // actor should have all of its components created and registered now, do any collision checking and handling that we need to do
            // haker: PostActorConstruction() handles 'SpawnCollisionHandlingMethod'
            // 1. try to find adjusted location/rotation by UWorld::FindTeleprotSpot()
            // 2. update Actor's location/rotation by SetActorLocationAndRotation()
            // - let's skimming the logic briefly
            if (World)
            {
                switch (SpawnCollisionHandlingMethod)
                {
                case ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn:
                {
                    // Try to find a spawn position
                    FVector AdjustedLocation = GetActorLocation();
                    FRotator AdjustedRotation = GetActorRotation();
                    if (World->FindTeleportSpot(this, AdjustedLocation, AdjustedRotation))
                    {
                        SetActorLocationAndRotation(AdjustedLocation, AdjustedRotation, false, nullptr, ETeleportType::TeleportPhysics);
                    }
                }
                break;
                case ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButDontSpawnIfColliding:
                {
                    // Try to find a spawn position			
                    FVector AdjustedLocation = GetActorLocation();
                    FRotator AdjustedRotation = GetActorRotation();
                    if (World->FindTeleportSpot(this, AdjustedLocation, AdjustedRotation))
                    {
                        SetActorLocationAndRotation(AdjustedLocation, AdjustedRotation, false, nullptr, ETeleportType::TeleportPhysics);
                    }
                    else
                    {
                        UE_LOG(LogSpawn, Warning, TEXT("SpawnActor failed because of collision at the spawn location [%s] for [%s]"), *AdjustedLocation.ToString(), *GetClass()->GetName());
                        Destroy();
                    }
                }
                break;
                case ESpawnActorCollisionHandlingMethod::DontSpawnIfColliding:
                    if (World->EncroachingBlockingGeometry(this, GetActorLocation(), GetActorRotation()))
                    {
                        UE_LOG(LogSpawn, Warning, TEXT("SpawnActor failed because of collision at the spawn location [%s] for [%s]"), *GetActorLocation().ToString(), *GetClass()->GetName());
                        Destroy();
                    }
                    break;
                case ESpawnActorCollisionHandlingMethod::Undefined:
                case ESpawnActorCollisionHandlingMethod::AlwaysSpawn:
                default:
                    // note we use "always spawn" as default, so treat undefined as that
                    // nothing to do here, just proceed as normal
                    break;
                }
            }

            // haker: as we saw above, depending on the result of SpawnActorCollisionHandlingMethod, it could already be destroyed!
            // - so we need to check its validity
            if (IsValidChecked(this))
            {
                // haker: call PostInitializeComponents()
                // - nothing specail, just skim it
                PostInitializeComponents();

                if (IsValidChecked(this))
                {
                    // haker: as we said, we suppose 'bDeferBeginPlayAndUpdateOverlaps' as false
                    // - so, if the world is already 'BeginPlay'ed, bRunBeginPlay will be 'true'
                    bool bRunBeginPlay = !bDeferBeginPlayAndUpdateOverlaps && (BeginPlayCallDepth > 0 || World->HasBegunPlay());
                    if (bRunBeginPlay)
                    {
                        // haker: if the current Actor is attached another Actor (ParentActor), we already check whether the ParentActor is BeginPlay'ed
                        if (AActor* ParentActor = GetParentActor())
                        {
                            // Child Actors cannot run begin play until their parent has run
                            bRunBeginPlay = (ParentActor->HasActorBegunPlay() || ParentActor->IsActorBeginningPlay());
                        }
                    }

                    if (bRunBeginPlay)
                    {
                        // haker: see DispatchBeginPlay(goto 009: SpawnActor)
                        DispatchBeginPlay();
                    }
                }
            }
        }
    }

    /** called to finish the spawning process, generally in the case of deferred spawning */
    // 005 - Foundation - SpawnActor - AActor::FinishSpawning
    void FinishSpawning(const FTransform& UserTransform, bool bIsDefaultTransform = false, const FComponentInstanceDataCache* InstanceDataCache = nullptr, ESpawnActorScaleMethod TransformScaleMethod = ESpawnActorScaleMethod::OverrideRootScale)
    {
        if (ensure(!bHasFinishedSpawning))
        {
            bHasFinishedSpawning = true;

            // haker: FinalRootComponentTransform is passed by UserTransform when RootComponent is not available
            FTransform FinalRootComponentTransform = (RootComponent ? RootComponent->GetComponentTransform() : UserTransform);

            // see if we need to adjust the transform (i.e. in deferred cases where the caller passes in a different transform here
            // then was passed in during the original SpawnActor call)
            // haker: in SpawnActor(), we pass bIsDefaultTransform as true
            // - when you set SpawnActorParameters.bDeferConstruction as 'true', bIsDefaultTransform will be passed as 'false'
            if (RootComponent && !bIsDefaultTransform)
            {
                // haker: UserTransform is cached in GSpawnActorDeferredTransformCache
                FTransform const* const OriginalSpawnTransform = GSpawnActorDeferredTransformCache.Find(this);
                if (OriginalSpawnTransform)
                {
                    GSpawnActorDeferredTransformCache.Remove(this);
                    if (OriginalSpawnTransform->Equals(UserTransform) == false)
                    {
                        // caller passed a different transform!
                        // undo the original spawn transform to get back to the template transform, so we can recompute a good
                        // final transform that takes into account the template's transform
                        
                        // haker: revert UserTransform which we cached before, because we defer FinsihSpawning()
                        FTransform const TemplateTransform = RootComponent->GetComponentTransform() * OriginalSpawnTransform->Inverse();
                        FinalRootComponentTransform = TemplateTransform * UserTransform;
                    }
                }

                // should be fast and relatively rare
                // haker: see ValidateDeferredTransformCache (goto 006: SpawnActor)
                ValidateDeferredTransformCache();
            }

            {
                // 000 - Foundation - ExecuteConstruction * - BEGIN
                // haker: ExecuteConstruction() processes SCS(SimpleConstructionScript) and UCS(UserConstructionScript):
                // - before looking into SpawnActor(), ExecuteConstruction() is one of large function call, so we look into it separately
                // see AActor::ExecuteConstruction (goto 001: ExecuteConstruction)
                ExecuteConstruction(FinalRootComponentTransform, nullptr, InstanceDataCache, bIsDefaultTransform, TransformScaleMethod);
                // 023 - Foundation - ExecuteConstruction * - END
            }

            {
                // haker: see PostActorConstruction (goto 007: SpawnActor)
                PostActorConstruction();
            }
        }
    }

    /** returns how much control the local machine has over this actor */
    ENetRole GetLocalRole() const { return Role; }

    /** 
     * called after the actor is spawned in the world
     * responsible for setting up actor for play
     */
    // 003 - Foundation - SpawnActor - AActor::PostSpawnInitialize
    void PostSpawnInitialize(FTransform const& UserSpawnTransform, AActor* InOwner, APawn* InInstigator, bool bRemoteOwned, bool bNoFail, bool bDeferConstruction, ESpawnActorScaleMethod TransformScaleMethod = ESpawnActorScaleMethod::MultiplyWithRoot)
    {
        // general flow here is like to
        // - Actor sets up the basics
        // - Actor gets PreInitializeComponents()
        // - Actor constructs itself, after which its components should be fully assembled
        // - Actor components get OnComponentCreated
        // - Actor components get InitializeComponent
        // - Actor gets PostInitializeComponents() once everything is set up
        //
        // this should be the same sequence for deferred or non-deferred spawning

        // haker: PostSpawnInitialize() follows similar steps which was done in AActor::PostLoad()
        // Diagram:
        // ┌─────────────────────┐      ┌────────────────────────────────────────────────────────────┐
        // │ AActor Spawn        ├─────►│                                                            │
        // └─────────────────────┘      │  UActorComponent Creation: (1)                             │
        //                              │   │                                                        │
        //                              │   └──For each UActorComponent in AActor's UActorComponents |
        //                              |       |                                                    |
        //                              |       └──UActorComponent::OnComponentCreated()             │
        //                              │                                                            │
        //                              ├────────────────────────────────────────────────────────────┤
        //                              │                                                            │
        //                              │  UActorComponent Register: (2)                             │
        //                              │   │                                                        │
        //                              │   ├──AActor::PreRegisterAllComponents()                    │
        //                              │   │                                                        │
        //                              │   ├──For each UActorComponent in AActor's UActorComponents │
        //                              │   │   │                                                    │
        //                              │   │   └──AActor::RegisterComponent()                       │
        //                              │   │                                                        │
        //                              │   └──AActor::PostRegisterAllComponents()                   │
        //                              │                                                            │
        //                              ├────────────────────────────────────────────────────────────┤
        //                              │  AActor::UserConstructionScript()                          │
        //                              ├────────────────────────────────────────────────────────────┤
        //                              │                                                            │
        //                              │  UActorComponent Initialization:                           │
        //                              │   │                                                        │
        //                              │   ├──AActor::PreInitializeComponents()                     │
        //                              │   │                                                        │
        //                              │   ├──For each UActorComponent in AActor's UActorComponents │
        //                              │   │   │                                                    │
        //                              │   │   ├──UActorComponent::Activate()                       │
        //                              │   │   │                                                    │
        //                              │   │   └──UActorComponent::InitializeComponent()            │
        //                              │   │                                                        │
        //                              │   └──AActor::PostInitializeComponents                      │
        //                              │                                                            │
        //                              └────────────────────────────────────────────────────────────┘

        // it's not safe to call UWorld accessor unction till the world info has been spawned
        UWorld* const World = GetWorld();

        // haker: I skip all network-relevant codes
        //...

        // set instigator
        // haker: 'Instigator' is the UObject who trigger to spawn this Actor
        SetInstigator(InInstigator);

        // set the actor's world transform if it has a native rootcomponent
        // haker: I skip FixupNativeActorComponents which handling the case that root-component doesn't exist
        USceneComponent* const SceneRootComponent = GetRootComponent();
        if (SceneRootComponent)
        {
            // haker: RootComponent contains world-transform of Actor:
            // - update Actor's world transform here
            check(SceneRootComponent->GetOwner() == this);

            // repsect any non-default transform value that the root component may have received from the archetype that's owned by the native CDO, 
            // so the final transform might not always necessarily equate to the passed-in UserSpawnTransform
            const FTransform RootTransform(SceneRootComponent->GetRelativeRotation(), SceneRootComponent->GetRelativeLocation(), SceneRootComponent->GetRelativeScale3D());
            FTransform FinalRootComponentTransform = RootTransform;
            
            // haker: depending on TransformScaleMethod, FinalRootComponentTransform is changed:
            // - just one thing to remember is that only 'OverrideRootScale' force FinalRootComponentTransform to override the value
            switch (TransformScaleMethod)
            {
            case ESpawnActorScaleMethod::OverrideRootScale:
                FinalRootComponentTransform = UserSpawnTransform;
                break;
            case ESpawnActorScaleMethod::MultiplyWithRoot:
            case ESpawnActorScaleMethod::SelectDefaultAtRuntime:
                FinalRootComponentTransform = RootTransform * UserSpawnTransform;
                break;
            }

            // haker: we call SetWorldTransform at the first time on SpawnActor()
            // - ConditionalUpdateComponentToWorld() will be called
            SceneRootComponent->SetWorldTransform(FinalRootComponentTransform, false, nullptr, ETeleportType::ResetPhysics);
        }

        // call OnComponentCreated on all default (native) components
        // haker: (1) iterates Actor's components and call OnComponentCreated() for each component
        // see DispatchOnComponentsCreated (goto 004: SpawnActor)
        DispatchOnComponentsCreated(this);

        // register the actor's default (native) components, but only if we have a native scene root.
        // if we don't, it implies that there could be only non-scene components at the native class level.
        // NOTE: this API will also call PostRegisterAllComponents() on the actor instance. 
        // if deferred, PostRegisterAllComponents() won't be called until the root is set by SCS

        // haker: (2) register components:
        // - it is worth to understand what 'bHasDeferredComponentRegistration' means
        // - 'bHasDeferredComponentRegistration' becomes 'true':
        //    - SceneRootComponent is nullptr and Actor's class is derived from BP
        //    - in this case, SceneRootComponent will be derived from SCS (SimpleConstructionScript)
        //    - so, we have to defer component registration until ExecuteConstruction() is called
        //      - 'ExecuteConstruction()' is called on 'FinishSpawning()'
        bHasDeferredComponentRegistration = (SceneRootComponent == nullptr && Cast<UBlueprintGeneratedClass>(GetClass()) != nullptr);
        if (!bHasDeferredComponentRegistration && GetWorld())
        {
            // haker: we had seen RegisterAllComponents(), but let's see it briefly again
            RegisterAllComponents();
        }

        // execute native and BP construction scripts
        // after this, we can assume all components are created and assembled
        
        // haker: 'bDeferConstruction' is from SpawnActorParameters.bDeferConstruction 
        // - first see else-if-statment when we defer to call FinishSpawning():
        //   - in FinishSpawning(), SCS and UCS are constructed
        if (!bDeferConstruction)
        {
            // haker: see FinishSpawning (goto 005: SpawnActor)
            FinishSpawning(UserSpawnTransform, true);
        }
        else if (SceneRootComponent != nullptr)
        {
            // we have a native root component and are deferred construction, store our original UserSpawnTransform
            // so we can do the proper thing if the user passes in a different transform during FinishSpawning
            // haker: right above, we have to keep UserSpawnTransform in separate stoarge:
            // - the separate storage is 'GSpawnActorDeferredTransformCache'
            GSpawnActorDeferredTransformCache.Emplace(this, UserSpawnTransform);
        }

        // haker: wrap up the steps done in SpawnActor:
        //
        // [PostSpawnInitialize]                                                      
        //  :suppose the World is already BeginPlay'ed                                
        //                                                                            
        //  0.create new AActor with NewObject<AActor> and set appropriate properties 
        //                                                                            
        //  1.Actor's components' creation(UActorComponent::OnComponentCreated)       
        //                                                                            
        //  2.ExecuteConstruction(): construct SCS and UCS                            
        //     │                                                                      
        //     └──all components after construction of SCS and UCS are registered     
        //                                                                            
        //  3.initialize all components in Actor                                      
        //                                                                            
        //  4.deal with SpawnActorCollision:                                          
        //     if the proper location/rotation is failed to find, destroy the self    
        //                                                                            
        //  5.actor and all components in Actor are called by 'BeginPlay()'                                                                              
    }

    /** get the local-to-world transform of the RootComponent: identical to GetTransform() */
    const FTransform& ActorToWorld() const
    {
        return (RootComponent ? RootComponent->GetComponentTransform() : FTransform::Identity);
    }

    /** get the actor-to-world transform */
    const FTransform& GetTransform() const
    {
        return ActorToWorld();
    }

    UFUNCTION(BlueprintImplementableEvent, meta=(DisplayName = "Hit"), Category="Collision")
    void ReceiveHit(class UPrimitiveComponent* MyComp, AActor* Other, class UPrimitiveComponent* OtherComp, bool bSelfMoved, FVector HitLocation, FVector HitNormal, FVector NormalImpulse, const FHitResult& Hit);

    /**
     * event when this actor bumps into a blocking object, or blocks another actor that bumps into it
     * this could happen due to things like Character movement, using SetLocation with 'sweep' enabled, or physics simulation
     * for events when objects overlap (e.g. walking into a trigger) see the 'overlap' event
     * 
     * @note:
     * - for collision during physics simulation to generate hit events, 'Simulation Generates Hit Events' must be enabled
     * - when receiving a hit from another object's movement (bSelfMoved is false), the directions of 'Hit.Normal' and 'Hit.ImpactNormal' will be adjusted to indicate force from the other object aginst this object
     */
    virtual void NotifyHit(class UPrimitiveComponent* MyComp, AActor* Other, class UPrimitiveComponent* OtherComp, bool bSelfMoved, FVector HitLocation, FVector HitNormal, FVector NormalImpulse, const FHitResult& Hit)
    {
        // call BP handler
        ReceiveHit(MyComp, Other, OtherComp, bSelfMoved, HitLocation, HitNormal, NormalImpulse, Hit);
    }

    /** Helper that already assumes the Hit info is reversed, and avoids creating a temp FHitResult if possible. */
    void InternalDispatchBlockingHit(UPrimitiveComponent* MyComp, UPrimitiveComponent* OtherComp, bool bSelfMoved, FHitResult const& Hit)
    {
        if (OtherComp != nullptr)
        {
            AActor* OtherActor = OtherComp->GetOwner();

            // call virtual
            NotifyHit(MyComp, OtherActor, OtherComp, bSelfMoved, Hit.ImpactPoint, Hit.ImpactNormal, FVector(0,0,0), Hit);

            // if we are still ok, call delegate on actor
            OnActorHit.Broadcast(this, OtherActor, FVector(0,0,0), Hit);

            // if component is still alive, call delegate on component
            MyComp->OnComponentHit.Broadcast(MyComp, OtherActor, OtherComp, FVector(0,0,0), Hit);
        }
    }

    /** call ReceiveHit, as well as delegates on Actor and Component */
    void DispatchBlockingHit(UPrimitiveComponent* MyComp, UPrimitiveComponent* OtherComp, bool bSelfMoved, FHitResult const& Hit)
    {
        InternalDispatchBlockingHit(MyComp, OtherComp, bSelfMoved, bSelfMoved ? Hit : FHitResult::GetReversedHit(Hit));
    }

    /** check whether any component of this Actor is overlapping any component of another Actor */
    // 020 - Foundation - UpdateOverlaps - AActor::IsOverlappingActor
    bool IsOverlappingActor(const AActor* Other) const
    {
        // haker: what is OwnedComponents?
        // - all attached components will be inserted into OwnedComponents!
        for (UActorComponent* OwnedComp : OwnedComponents)
        {
            // haker: only PrimitiveComponent has overlapping state
            if (UPrimitiveComponent* PrimComp = Cast<UPrimitiveComponent>(OwnedComp))
            {
                // see UPrimitiveComponent::IsOverlappingActor (goto 021: UpdateOverlaps)
                if ((PrimComp->GetOverlapInfos().Num() > 0) && PrimComp->IsOverlappingActor(Other))
                {
                    // found one
                    return true;
                }
            }
        }
        return false;
    }

    /** get the local-to-world transform of the RootComponent; identicial to GetTransform() */
    const FTransform& ActorToWorld() const
    {
        return (RootComponent ? RootComponent->GetComponentTransform() : FTransform::Identity);
    }

    // 010 - Foundation - UpdateComponentToWorld - AActor::GetTransform()
    const FTransform& GetTransform() const
    {
        // haker: in ActorToWorld(), if RootComponent exists, we use Actor's transform as root component's ComponentToWorld
        return ActorToWorld();
    }

    /** returns if this actor is currently running the UCS(User Construction Script) */
    bool IsRunningUserConstructionScript() const { return bRunningUserConstructionScript; }

    /** check for and resolve any name conflicits prior to instancing an new Blueprint Component */
    // 021 - Foundation - ExecuteConstruction * - AActor::CheckComponentInstanceName
    void CheckComponentInstanceName(const FName InName)
    {
        // if there is Component with this name already (almost certainly because it is an Instance Component), we need to rename it out of the way
        if (!InName.IsNone())
        {
            // haker: we call this function before creating new component, so if we get 'ConflictingObject', it means the name is already used
            UObject* ConflictingObject = FindObjectFast<UObject>(this, InName);

            // haker: see the conditions:
            // 1. ConflictingObject is ActorComponent
            // 2. ConflictingObject is created by 'Instance'
            // -> try to rename ConflictingObject by changing only 'unique number'
            if (ConflicitingObject && ConflictingObject->IsA<UActorComponent>() && CastChecked<UActorComponent>(ConflictingObject)->CreationMethod == EComponentCreationMethod::Instance)
            {
                // try and pick a good name
                FString ConflicitingObjectName = ConflictingObject->GetName();
                int32 CharIndex = ConflicitingObjectName.Len() - 1;
                while (CharIndex >= 0 && FChar::IsDigit(ConflictingObjectName[CharIndex]))
                {
                    --CharIndex;
                }

                // name is only composed of digits not a name conflicit resolution
                // haker: normally it can NOT be happended
                if (CharIndex < 0)
                {
                    return;
                }

                // haker: now CharIndex points number's starting location in FName
                // - using CharIndex, calculate 'Count' of number characters
                int32 Counter = 0;
                if (CharIndex < ConflicitingObjectName.Len() - 1)
                {
                    Counter = FCString::Atoi(*ConflicitingObjectName.RightChop(CharIndex + 1));

                    // haker: now ConflictingObjectName is 'name entry' e.g. ActorComponent
                    ConflicitingObjectName.LeftInline(CharIndex + 1, false);
                }

                // haker: similar to FindFirstFreeName
                FString NewObjectName;
                do
                {
                    NewObjectName = ConflicitingObjectName + FString::FromInt(++Counter);
                } while (FindObjectFast<UObject>(this, *NewObjectName) != nullptr);

                // haker: we are not going to how 'Rename' works (it is a little bit complicated)
                // - as its word implies, it renames ConflictingObject with NewObjectName
                ConflicitingObject->Rename(*NewObjectName, this);
            }
        }
    }

    /** called after instancing a new Blueprint Component from either a template or cooked data */
    // 021 - Foundation - ExecuteConstruction * - PostCreateBlueprintComponent
    void PostCreateBlueprintComponent(UActorComponent* NewActorComp)
    {
        if (NewActorComp)
        {
            // haker: we initially set newly created component as 'UserConstructionScript'
            // - later, we override CreationMethod as 'SimpleConstructionScript' if creating component happens in SCS construction
            NewActorComp->CreationMethod = EComponentCreationMethod::UserConstructionScript;

            // need to do this so component gets saved - Components array is not serialized
            // haker: all components created in SCS are stored in separate container different from OwnedComponents:
            // - it stores in 'BlueprintCreatedComponents'
            BlueprintCreatedComponents.Add(NewActorComp);

            // haker: OwnedComponents contains all components who has owner of the Actor:
            // - where new component is added to OwnedComponent?
            //   [ ] in Lyra project, let's debug it:
            //       - set breakpoints on:
            //         1. AActor::CreateComponentFromTemplate
            //         2. UActorComponent::PostInitProperties()
            // -> new component is added to OwnedComponents while StaticConstructObject() is called by overriding UObject::PostInitProperties
        }
    }

    /** util to create component based on a template */
    // 018 - Foundation - ExecuteConstruction * - AActor::CreateComponentFromTemplate
    UActorComponent* CreateComponentFromTemplate(UActorComponent* Template, const FName InName=NAME_None)
    {
        UActorComponent* NewActorComp = nullptr;
        if (Template != nullptr)
        {
            // haker: in our case, we pass Name as InternalVariableName of USCS_Node
            if (InName == NAME_None)
            {
                // search for a unique name based on our template: our template is going to be our archetype
                // thanks to logic in UBPGC::FindArchetype
                // haker: need to make sure that the name should be unique 
                // see FindFirstFreeName (goto 019: ExecuteConstruction)
                InName = FindFirstFreeName(this, Template->GetFName());
            }
            else
            {
                // resolve any name conflicits
                // haker: if we have a conflict with the arbitrary name, we rename the old-one
                // see AActor::CheckComponentInstanceName (goto 021: ExecuteConstruction)
                CheckComponentInstanceName(InName);
            }

            // haker: now we have unique name to create component with the template:
            // - there is two things to note:
            //   1. we aren't copying the RF_ArchetypeObject flag
            //      - we can guess ComponentTemplate is 'RF_ArchetypeObject'
            //      - using ComponentTemplate, new component is no more archetype object
            //   2. the result is non-transactional by default
            FObjectDuplicationParameters DupActorParameters(Template, this);
            DupActorParameters.DestName = InName;
            DupActorParameters.FlagMask = RF_AllFlags & ~(RF_ArchetypeObject | RF_Transactional | RF_WasLoaded | RF_Public | RF_InheritableComponentTemplate);
            DupeActorParameters.PortFlags = PPF_DuplicateVerbatim; // Skip resetting text IDs

            // haker: creating new component with the template uses 'StaticDuplicateObjectEx':
            // - we are not covering StaticDuplicateObjectEx()
            // - when we deal with unreal object reflection system, we cover this function in the future
            // - the one thing you have to note is that we don't use 'StaticAllocateObject'
            NewActorComp = (UActorComponent*)StaticDuplicateObjectEx(DupActorParameters);

            // handle post-creation tasks:
            // see AActor::PostCreateBlueprintComponent (goto 021: ExecuteConstruction)
            PostCreateBlueprintComponent(NewActorComp);

            // haker: in this function, there are a few things to remember:
            // 1. ComponentTemplate is 'RF_ArchetypeObject'
            //    - template object type is 'ArchetypeObject' and 'ClassDefaultObject'
            //    - creating new component has two types:
            //      1) 'StaticAllocateObject'   - create new instance and initialize
            //          - CDO (Class Default Object), we'll see it again in SpawnActor()
            //      2) 'StaticDuplicateObject'  - duplicate the existing object
            //          - ArchetypeObject
            //          - we can also do this with CDO, but not preferable
            //          - using ArchetypeObject(ComponentTemplate) we can variate different types of templates for same class type
            //            - BUT, with CDO, it is hard to do this
            //
            // 2. all components created from SCS or UCS are stored in 'BlueprintCreatedComponents'
            //
            // - try to understand the difference between CDO and Archetype(ComponentTemplate version):
            //   - Diagram:                             
            //              1:1 (Template)                                            
            //     UClass ────────────────► CDO (Class Default Object)                
            //                                                                        
            //              1:N (Template)                                            
            //     UClass ──────┬─────────► ComponentTemplate0                        
            //                  │                                                     
            //                  │                                                     
            //                  ├─────────► ComponentTemplate1      ***By Archetype   
            //                  │                                                     
            //                  │                                                     
            //                  └─────────► ComponentTemplate2                        
                                                                                 
        }

        return NewActorComp;
    }

    /** returns true if Actor has deferred the RegisterAllComponents() call at spawn time (e.g. pending Blueprint SCS execution to set up a scene root component). */
	bool HasDeferredComponentRegistration() const { return bHasDeferredComponentRegistration; }

    /** destroy the player input component and removes any references to it */
    virtual void DestroyPlayerInputComponent()
    {
        if (InputComponent)
        {
            InputComponent->DestroyComponent();
            InputComponent = nullptr;
        }
    }

    /** searches components array and returns first encountered component of the specified class, native version of GetComponentByClass */
    virtual UActorComponent* FindComponentByClass(const TSubclassOf<UActorComponent> ComponentClass) const
    {
        UActorComponent* FoundComponent = nullptr;
        if (UClass* TargetClass = ComponentClass.Get())
        {
            for (UActorComponent* Component : OwnedComponents)
            {
                if (Component && Component->IsA(TargetClass))
                {
                    FoundComponent = Component;
                    break;
                }
            }
        }
        return FoundComponent;
    }

    // 013 - Foundation - CreateWorld ** - AActor's member variables

    /** 
     * primary actor tick function, which calls TickActor() 
     * - tick functions can be configured to control whether ticking is enabled, at what time during a frame the update occurs, and to set up tick dependencies
     */
    // haker: unreal engine code separates the state(class) and the tick by FTickFunction
    // - we'll cover this in detail later
    // 043 - Foundation - CreateWorld * - AActor::PrimaryActorTick
    // see FActorTickFunction (goto 44)
    struct FActorTickFunction PrimaryActorTick;

    /** all ActorComponents owned by this Actor; stored as a Set as actors may have a large number of components */
    // haker: all components in AActor is stored here
    // see UActorComponent (goto 14)
    TSet<TObjectPtr<UActorComponent>> OwnedComponents;

    /** array of ActorComponents that are created by blueprints and serialized per-instance */
    TArray<TObjectPtr<UActorComponent>> BlueprintCreatedComponents;

    /** whether FinishSpawning has been called for this Actor: if it has not, the Actor is in a malformed state */
    uint8 bHasFinishedSpawning : 1;

    /**
     * indicates that PreInitializeComponents/PostInitializeComponents has been called on this Actor
     * prevents re-initializing of actors spawned during level startup 
     */
    // haker: when PostInitializeComponents is called, bActorInitialized is set to 'true'
    uint8 bActorInitialized : 1;

    /** whether we've tried to register tick functions; reset when they are unregistered */
    // haker: tick function is registered
    uint8 bTickFunctionsRegistered : 1;

    /** whether we've deferred the RegisterAllComponents() call at spawn time; reset when RegisterAllComponents() is called */
    uint8 bHasDeferredComponentRegistration : 1;

    /** enum defining if BeginPlay has started or finished */
    enum class EActorBeginPlayState : uint8
    {
        HasNotBegunPlay,
        BeginningPlay,
        HasBegunPlay,
    };

    /** 
     * indicates that BeginPlay has been called for this actor 
     * set back to HasNotBegunPlay once EndPlay has been called
     */
    // haker: this is VERY INTERESTING!
    // - uint8 bit assignment is working even on middle of uint8's enum type!
    // - ActorHasBegunPlay is updated by BeginPlay() or EndPlay() calls
    EActorBeginPlayState ActorHasBegunPlay : 2;

    /** true if this actor is currently running user construction script (used to defer component registration) */
    uint8 bRunningUserConstructionScript : 1;

    /** set when DispatchBeginPlay() triggers from level streaming, and cleared afterwards. @see IsActorBeginningPlayFromLevelStreaming(). */
    uint8 bActorBeginningPlayFromLevelStreaming:1;

    /** 
     * the component that defines the transform (location, rotation, scale) of this Actor in the world
     * all other components must be attached to this one somehow
     */
    // haker: AActor manages its components(UActorComponent) in a form of scene graph using USceneComponent:
    // [*] see BP's viewport (refer to SCS[Simple Construction Script])
    //
    // Diagram:                                                                                         
    // AActor                                                                                
    //  │                                                                                    
    //  ├──OwnedComponents: TSet<TObjectPtr<UActorComponent>                                 
    //  │    ┌──────────────────────────────────────────────────────────────────────────┐    
    //  │    │ [Component0, Component1, Component2, Component3, Component4, Component5] │    
    //  │    │                                                                          │    
    //  │    └──────────────────────────────────────────────────────Linear Format───────┘    
    //  └──RootComponent: TObjectPtr<USceneComponent>                                        
    //       ┌────────────────────────────┐                                                  
    //       │ Component0 [RootComponent] │                                                  
    //       │  │                         │                                                  
    //       │  ├──Component1             │                                                  
    //       │  │   │                     │                                                  
    //       │  │   ├──Component2         │                                                  
    //       │  │   │                     │                                                  
    //       │  │   └──Component3         │                                                  
    //       │  │       │                 │                                                  
    //       │  │       └──Component4     │                                                  
    //       │  │                         │                                                  
    //       │  └──Component5             │                                                  
    //       │                            │                                                  
    //       └────Hierachrical Format─────┘                                                  
    //                                                                                       
    // see USceneComponent (goto 17)
    TObjectPtr<USceneComponent> RootComponent;

    /** The UChildActorComponent that owns this Actor */
    // haker: we are not covering UChildActorComponent in detail, but let's cover concept of UChildActorComponent
    // - UChildActorComponent supports connection between Actor and Actor
    // - we only have Actor-ActorComponent connection, how to support Actor-Actor connection?
    //   - see Diagram:                                                                                               
    //     ┌──────────┐                         ┌──────────┐                           ┌──────────┐              
    //     │  Actor0  ├────────────────────────►│  Actor1  ├──────────────────────────►│  Actor2  │              
    //     └──────────┘                         └──────────┘                           └──────────┘              
    //                                                                                                           
    //     RootComponent                        RootComponent ◄──────────┐             RootComponent             
    //      │                                    │                       │              │                        
    //      ├───Component1◄──────────┐           ├───Component1     Actor2-Actor1       └───Component1           
    //      │                        │           │                       │                                       
    //      ├───Component2    Actor1-Actor0      └───Component2          └─────────────ParentComponent           
    //      │    │                   │                                                                           
    //      │    └───Component3      └──────────ParentComponent                                                  
    //      │                                                                                                    
    //      └───Component4                                                                                       
    //                                                                                                           
    //                                                                                                           
    //                 ┌──────────────────────────────────────┐                                                  
    //                 │                                      │                                                  
    //                 │      Actor0 ◄───                     │                                                  
    //                 │       │                              │                                                  
    //                 │       └─RootComponent                │                                                  
    //                 │          │                           │                                                  
    //                 │          └─Component1                │                                                  
    //                 │             │                        │                                                  
    //                 │             └─Actor1 ◄───            │                                                  
    //                 │                │                     │                                                  
    //                 │                └─RootComponent       │                                                  
    //                 │                   │                  │                                                  
    //                 │                   └─Actor2 ◄────     │                                                  
    //                 │                                      │                                                  
    //                 │                                      │                                                  
    //                 └──────────────────────────────────────┘                                                  
    TWeakObjectPtr<UChildActorComponent> ParentComponent;

    /**
     * called when this Actor hits (or is hit by) something solid
     * - this could happen due to things like character movement, using SetLocation with 'sweep' enabled, or physics simulation
     * - for events when objects overlap (e.g. walking into a trigger) see the 'Overlap' event
     * @note: for collision during physics simulation to generate hit events, 'Simulation Generates Hit Events'
     */
	FActorHitSignature OnActorHit;

    FActorBeginOverlapSignature OnActorBeginOverlap;
    FActorEndOverlapSignature OnActorEndOverlap;

    /** describes how much control the local machine has over the Actor */
    ENetRole Role;

    /** automatically registers this actor to receive input from a player */
    EAutoReceiveInput::Type AutoReceiveInput;

    /** component that handles input for this actor, if input is enabled */
    TObjectPtr<class UInputComponent> InputComponent;

    /** owner of this Actor, used primarily for replication (bNewUseOwnerRelevancy & bOnlyRelevantToOwner) and visibility (PrimitiveComponent bOwnerNoSee and bOnlyOwnerSee) */
    TObjectPtr<AActor> Owner;

    /** array of all Actors whose Owner is this actor, these are not necessarily spawned by UChildActorComponent */
    TArray<TObjectPtr<AActor>> Children;
};