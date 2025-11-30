
/** thread safe container for actor related global variables */
// haker: 
// - what is TLS(Thread-Local Storage)?
// - why is it thread-safe?
// - understand TLS with TThreadSingleton
// - VERY USEFUL concept to use parallel programming in client-side
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
// 44 - Foundation - CreateWorld - FActorTickFunction
// see FTickFunction (goto 45)
struct FActorTickFunction : public FTickFunction
{
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

// 12 - Foundation - CreateWorld - AActor
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
//     [ ] explain UserConstructionScript vs. SimpleConstructionScript in the editor:
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
//       2. InitializeComponent
//
//  10. AActor::PostInitializeComponents:
//     - post-event when UActorComponents initializations are finished
// 
//  11. AActor::Beginplay:
//     - when level is ticking, AActor calls BeginPlay()
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
    /** Returns this actor's root component. */
    USceneComponent* GetRootComponent() const { return RootComponent; }

    /** 
     * walk up the attachment chain from RootComponent until we encounter a different actor, and return it 
     * if we are not attached to a component in a different actor, returns nullptr;
     */
    AActor* GetAttachParentActor() const
    {
        if (GetRootComponent() && GetRootComponent()->GetAttachParent())
        {
            return  GetRootComponent()->GetAttachParent()->GetOwner();
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

        FActorThreadContext::Get().TestRegisterTickFunctions = this;
    }

    /** when called, will call the virtual call chain to register all of the tick functions for both the actor and optionally all components */
    void RegisterAllActorTickFunctions(bool bRegister, bool bDoComponents)
    {
        if (!IsTemplate())
        {
            // prevent repeated redundant attempts
            if (bTickFunctionsRegistered != bRegister)
            {
                FActorThreadContext& ThreadContext = FActorThreadContext::Get();
                RegisterActorTickFunctions(bRegister);
                bTickFunctionsRegistered = bRegister;
                check(ThreadContext.TestRegisterTickFunctions == this);
                ThreadContext.TestRegisterTickFunctions = nullptr;
            }

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
        }
    }

    /** returns whether an actor has had BeginPlay() called on it (and not subsequently has EndPlay() called) */
    bool HasActorBegunPlay() const { return ActorHasBegunPlay == EActorBeginPlayState::HasBegunPlay; }

    /** returns whether an actor is in the process of beginning play */
    bool IsActorBeginningPlay() const { return ActorHasBegunPlay == EActorBeginPlayState::BeginningPlay; }

    /** finish initializing the component and register tick functions and BeginPlay() if it's the proper time to do so */
    // 65 - Foundation - CreateWorld - AActor::HandleRegisterComponentWithWorld
    void HandleRegisterComponentWithWorld(UActorComponent* Component)
    {
        const bool bOwnerBeginPlayStarted = HasActorBegunPlay() || IsActorBeginningPlay();

        // haker: if component is not initialized, try to initialize the component
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
                Component->BeginPlay();
            }
        }
    }

    /**
     * internal helper function to call a compile-time lambda on all components of a given-type
     * use template parameter bClassIsActorComponent to avoid doing unnecessary IsA checks when the ComponentClass is exactly UActorComponent
     * use template parameter bIncludeFromChildActors to recurse in to ChildActor components and find components of the appropriate type in those actors as well  
     */
    template <class ComponentType, bool bClassIsActorComponent, bool bIncludeFromChildActors, typename Func>
    void ForEachComponent_Internal(TSubclassOf<UActorComponent> ComponentClass, Func InFunc) const
    {
        check(bClassIsActorComponent == false || ComponentClass == UActorComponent::StaticClass());
        check(ComponentClass->IsChildOf(ComponentType::StaticClass()));

        // static check, so that the most common case (bIncludeFromChildActors) doesn't need to allocate an additional array
        if (bIncludeFromChildActors)
        {
            TArray<AActor*, TInlineAllocator<NumInlinedActorComponents>> ChildActors;
            for (UActorComponent* OwnedComponent : OwnedComponents)
            {
                if (OwnedComponent)
                {
                    if (bClassIsActorComponent || OwnedComponent->IsA(ComponentClass))
                    {
                        InFunc(static_cast<ComponentType*>(OwnedComponent));
                    }
                    // haker: explain what ChildActorComponent is
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

    template <class ComponentType, typename Func>
    void ForEachComponent_Internal(TSubclassOf<UActorComponent> ComponentClass, bool bIncludeFromChildActors, Func InFunc) const
    {
        if (ComponentClass == UActorComponent::StaticClass())
        {
            if (bIncludeFromChildActors)
            {
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
    template <class AllocatorType>
    void GetComponents(TArray<UActorComponent*, AllocatorType>& OutComponents, bool bIncludeFromChildActors = false)
    {
        OutComponents.Reset();
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
    bool IncrementalRegisterComponents(int32 NumComponentsToRegister, FRegisterComponentContext* Context = nullptr)
    {
        if (NumComponentsToRegister == 0)
        {
            // 0 - means register all components
            NumComponentsToRegister = MAX_int32;
        }

        UWorld* const World = GetWorld();
        check(World);

        // if we are not a game world, then register tick functions now, if we are a game world we wait until right before BeginPlay()
        // so as to not actually tick until BeginPlay() executes (which could otherwise happen in network games)
        if (!World->IsGameWorld())
        {
            // haker: in our case, we analyze the engine code in editor environment, let's get into this function
            // - note that bDoComponents is false!
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

        int32 NumTotalRegisteredComponents = 0;
        int32 NumRegisteredComponentsThisRun = 0;
        
        TInlineComponentArray<UActorComponent*> Components;
        GetComponents(Components);

        TSet<UActorComponent*> RegisteredParents;
        for (int32 CompIdx = 0; CompIdx < Components.Num() && NumRegisteredComponentsThisRun < NumComponentsToRegister; ++CompIdx)
        {
            UActorComponent* Component = Components[CompIdx];
            if (!Component->IsRegistered() && Component->bAutoRegister && IsValidChecked(Component))
            {
                // ensure that all parents are registered first
                USceneComponent* UnregisteredParentComponent = GetUnregisteredParent(Component);
                if (UnregisteredParentComponent)
                {
                    bool bParentAlreadyHandled = false;
                    RegisteredParents.Add(UnregisteredParentComponent, &bParentAlreadyHandled);
                    if (bParentAlreadyHandled)
                    {
                        UE_LOG(LogActor, Error, TEXT("AActor::IncrementalRegisterComponents parent component '%s' cannot be registered in actor '%s'"), *GetPathNameSafe(UnregisteredParentComponent), *GetPathName());
                        break;
                    }

                    // register parent first, then return to this component on a next iteration
                    // haker: let's think about how we process unregistered parent
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

    // 13 - Foundation - CreateWorld - AActor's member variables

    /** 
     * primary actor tick function, which calls TickActor() 
     * - tick functions can be configured to control whether ticking is enabled, at what time during a frame the update occurs, and to set up tick dependencies
     */
    // haker: unreal engine code separates the state(class) and the tick by FTickFunction
    // - we'll cover this in detail later

    // 43 - Foundation - CreateWorld - AActor::PrimaryActorTick
    // see FActorTickFunction (goto 44)
    struct FActorTickFunction PrimaryActorTick;

    /** all ActorComponents owned by this Actor; stored as a Set as actors may have a large number of components */
    // haker: all components in AActor is stored here
    // see UActorComponent (goto 14)
    // 
    // /** 이 Actor가 소유한 모든 ActorAomponents; actors 집합으로 저장된 배우 구성 요소는 다수일 수 있습니다 */
    // 하커: AActor의 모든 구성 요소는 여기에 저장됩니다
    // 모든 생성된 컴포넌트들은 이곳에 들어가있다.
    TSet<TObjectPtr<UActorComponent>> OwnedComponents;

    /**
     * indicates that PreInitializeComponents/PostInitializeComponents has been called on this Actor
     * prevents re-initializing of actors spawned during level startup 
     */
    // haker: when PostInitializeComponents is called, bActorInitialized is set to 'true'
    // 현재 Initialize단계가 어디까지 진행되었는지를 마킹해주는 비트 플래그
    /**
    * 이 행위자에게 PreInitializeComponents/PostInitializeComponents가 호출되었음을 나타냅니다
    * 레벨 시작 중에 생성된 액터의 initial 재설정을 방지합니다
    */
    // haker: PostInitializeComponents가 호출되면 bActorInitialize가 'true'로 설정됩니다
    uint8 bActorInitialized : 1;

    /** whether we've tried to register tick functions; reset when they are unregistered */
    // haker: tick function is registered
    /** 틱 기능을 등록하려고 했는지 여부; 등록이 취소되면 재설정 */
    // 하커: 틱 기능이 등록되었습니다
    uint8 bTickFunctionsRegistered : 1;

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

    /**
    * 이 배우를 위해 BeginPlay가 호출되었음을 나타냅니다
    * EndPlay가 호출되면 HasNotBegunPlay로 돌아갑니다
    */
    // 하커: 정말 흥미롭네요!
    // - uint8 비트 할당이 uint8의 열거형 중간에서도 작동합니다!
    // - ActorHasBegunPlay는 BeginPlay() 또는 EndPlay() 호출을 통해 업데이트됩니다
    EActorBeginPlayState ActorHasBegunPlay : 2;

    /** 
     * the component that defines the transform (location, rotation, scale) of this Actor in the world
     * all other components must be attached to this one somehow
     */
    // haker: AActor manages its components(UActorComponent) in a form of scene graph using USceneComponent:
    // [ ] see BP's viewport (refer to SCS[Simple Construction Script])
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
    // USceneComponent -> 계층구조를 가능하게 해주는 컴포넌트
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
};