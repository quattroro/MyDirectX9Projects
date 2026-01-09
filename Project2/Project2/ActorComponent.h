
/** tick function that calls UActorComponent::ConditionalTick */
struct FActorComponentTickFunction : public FTickFunction
{
    /** AActor component that is the target of this tick */
    class UActorComponent* Target;
};

DECLARE_DYNAMIC_MULTICAST_SPARSE_DELEGATE_TwoParams(FActorComponentActivatedSignature, UActorComponent, OnComponentActivated, UActorComponent*, Component, bool, bReset);

// 016 - Foundation - CreateWorld ** - EComponentCreationMethod
enum class EComponentCreationMethod : uint8
{
    /** a component that is part of a native class */
    Native,
    /** a component that is created from a template defined in the Component section of Blueprint */
    SimpleConstructionScript,
    /** a dynamically created component, either from the UserConstructionScript or from a Add component node in a Blueprint event graph */
    UserConstructionScript,
    /** a component added to a single Actor instance via the Component section of the Actor's details panel */
    Instance,
};

/** Information about how to update transform when something is moved */
// 004 - Foundation - UpdateComponentToWorld - EUpdateTransformFlags
enum class EUpdateTransformFlags : int32
{
	/** Default options */
	None = 0x0,
	/** Don't update the underlying physics */
    // haker: as the word implies, it is related to SkipPhysicsUpdate, for now, we'll not covered today with this flag
	SkipPhysicsUpdate = 0x1,		
	/** The update is coming as a result of the parent updating (i.e. not called directly) */
    // haker: when we propagate UpdateComponentToWorld from parent to AttachChildren
	PropagateFromParent = 0x2,		
	/** Only update child transform if attached to parent via a socket */
    // haker: it is special purpose flag type, but for now, we don't need to care so much
	OnlyUpdateIfUsingSocket = 0x4	
};

struct FSimpleMemberReference
{
    /**
     * most often the Class that this member is defined in
     * Could be a UPackage, if it is a native delegate signature function (declared globally) 
     */
    TObjectPtr<UObject> MemberParent;

    /** name of the member */
    FName MemberName;

    /** the GUID of the member */
    FGuid MemberGuid;
};

/**
 * base class for component re-register objects, provides helper functions for performing the UnRegister and ReRegister 
 */
class FComponentReregisterContextBase
{
    TSet<FSceneInterface*>* ScenesToUpdateAllPrimitiveSceneInfos = nullptr;

    /** unregister the component and returns the world it was registered to */
    UWorld* Unregister(UActorComponent* InComponent)
    {
        UWorld* World = nullptr;

        if (InComponent->IsRegistered() && InComponent->GetWorld())
        {
            // save the world and set the component's world to NULL to prevent a nested FComponentReregisterContext from reregistering this component
            World = InComponent->GetWorld();
            
            // will set bRegistered to false
            InComponent->ExecuteUnregisterEvents();

            InComponent->WorldPrivate = nullptr;
        }

        return World;
    }
    
    /** reregister the given component on the given scene */
    void Reregister(UActorComponent* InComponent, UWorld* InWorld)
    {
        if (IsValid(InComponent))
        {
            if (InComponent->IsRegistered())
            {
                // the component has been registered already but external code is going to expect the reregister to happen now
                // so unregister and re-register
                InComponent->ExecuteUnregisterEvents();
            }

            InComponent->WorldPrivate = InWorld;

            // will set bRegistered to true
            InComponent->ExecuteRegisterEvents();
        }
    }
};

/**
 * unregisters a component for the lifetime of this class
 * 
 * typically used by constructing the class on the stack:
 * {
 *  FComponentReregisterContext ReregisterContext(this);
 *  // The component is unregistered with the world here as ReregisterContext is constructed.
 *  //...
 *  // The component is registered with the world here as ReregisterContext is destructed.
 * } 
 */
class FComponentReregisterContext : public FComponentReregisterContextBase
{
    /** pointer to component we are unregistering */
    TWeakObjectPtr<UActorComponent> Component;
    /** cache pointer to world from which we were removed */
    TWeakObjectPtr<UWorld> World;

    FComponentReregisterContext(UActorComponent* InComponent, TSet<FSceneInterface*>* InScenesToUpdateAllPrimitiveSceneInfos = nullptr)
        : World(nullptr)
    {
        ScenesToUpdateAllPrimitiveSceneInfos = InScenesToUpdateAllPrimitiveSceneInfos;
        World = UnRegister(InComponent);

        // if we didn't get a scene back NULL the component so we don't try to process it on destruction
        Component = World.IsValid() ? InComponent : nullptr;
    }

    ~FComponentReregisterContext()
    {
        if (Component.IsValid() && World.IsValid())
        {
            Reregister(Component.Get(), World.Get());
        }
    }
};

/**
 * ActorComponent is the base class for components that define reusable behavior that can be added to a different types of Actors
 * ActorComponents that have a transform are known as 'SceneComponents' and those that can be rendered are 'PrimitiveComponents'
 */
// 014 - Foundation - CreateWorld ** - UActorComponent
// haker: ActorComponent is base class to attach to the AActor:
// - support scene graph (hierarchy): USceneComponent
// - support rendering: UPrimitiveComponent
// see UActorComponent's member variables (goto 15)
class UActorComponent : public UObject
{
    virtual void void SendRenderTransform_Concurrent()
    {
        bRenderTransformDirty = false;
    }

    // 001 - Foundation - CreateRenderState * - UActorComponent::CreateRenderState_Concurrent
    virtual void CreateRenderState_Concurrent(FRegisterComponentContext* Context)
    {
        // haker: we just create render state, so mark 'bRenderStateCreated' as true
        bRenderStateCreated = true;

        // haker: dirty flags to notify any changes in world to render-world
        bRenderStateDirty = false;
        bRenderTransformDirty = false;
        //...

        // haker: we can't find any code to create new render state here:
        // - render state is only created for PrimitiveComponent
        // - it is natural to create render state on PrimitiveComponent, cuz PrimitiveComponent is the base class who has geometry data (e.g. vertex buffer/index buffer) to render
    }

    AActor* GetOwner() const
    {
        return OwnerPrivate;
    }

    /** if WorldPrivate isn't set this will determine world from outers */
    UWorld* GetWorld_Uncached() const
    {
        UWorld* ComponentWorld = nullptr;

        AActor* MyOwner = GetOwner();

        // if we don't have a world yet, it may be because we haven't gotten registered yet, but we can try to look at our owner
        if (MyOwner && !MyOwner->HasAnyFlags(RF_ClassDefaultObject))
        {
            ComponentWorld = MyOwner->GetWorld();
        }

        if (ComponentWorld == nullptr)
        {
            // as a fallback check the outer of this component for a world
            // - in some cases components are spawned directly in the world
            // haker: I don't know specific example getting into this case
            ComponentWorld = Cast<UWorld>(GetOuter());
        }

        return ComponentWorld;
    }

    virtual UWorld* GetWorld() const override final { return (WorldPrivate ? WorldPrivate : GetWorld_Uncached()); }

    /**
     * set up a tick function a component in the standard way
     * tick after the actor; don't tick if the actor is static, or if the actor is a template or if this is a 'NeverTick' component
     * tick while paused if only if my owner does
     */
    // 062 - Foundation - CreateWorld * - UActorComponent::SetupActorComponentTickFunction
    // haker: this TickFunction is for ActorComponent:
    // - ActorComponent's TickFunction run after Actor's TickFunction
    // - do NOT tick
    //   1. Actor is static
    //   2. Actor is template(==CDO)
    //   3. NeverTick is 'true'
    // - ActorComponent's can be PAUSED, only if owner does
    bool SetupActorComponentTickFunction(FTickFunction* TickFunction)
    {
        if (TickFunction->bCanEverTick && !IsTemplate())
        {
            AActor* MyOwner = GetOwner();
            // haker: MyOwner (AActor) also should NOT be Template()
            if (!MyOwner || !MyOwner->IsTemplate())
            {
                ULevel* ComponentLevel = (MyOwner ? MyOwner->GetLevel() : ToRawPtr(GetWorld()->PersistentLevel));
                // haker: if TickFunction is not registered, just TickState will be updated
                // - we have seen SetTickFunctionEnable(), but let's see one more again briefly
                // see IsTickFunctionEnabled()
                // see SetTickFunctionEnable() - we already had seen!
                TickFunction->SetTickFunctionEnable(TickFunction->bStartsWithTickEnabled || TickFunction->IsTickFunctionEnabled());

                // haker: we need ULevel where the component resides in, cuz FTickFunction is resides in TickTaskLevel!
                // see RegisterTickFunction (goto 63)
                TickFunction->RegisterTickFunction(ComponentLevel);
                return true;
            }
        }
        return false;

        // haker: SetupActorComponentTickFunction makes sure that TickFunction is registered
    }

    /** virtual call chain to register all tick functions */
    // 061 - Foundation - CreateWorld * - UActorComponent::RegisterComponentTickFunctions
    // haker: the function name is 'TickFunctions'
    // - you can override this function to add multiple TickFunctions (maybe we will have a chance to see this in the future)
    virtual void RegisterComponentTickFunctions(bool bRegister)
    {
        if (bRegister)
        {
            // see SetupActorComponentTickFunction (goto 62)
            if (SetupActorComponentTickFunction(&PrimaryComponentTick))
            {
                PrimaryComponentTick.Target = this;
            }
        }
        else
        {
            if (PrimaryComponentTick.IsTickFunctionRegistered())
            {
                // see UnregisterTickFunction (goto 64)
                PrimaryComponentTick.UnRegisterTickFunction();
            }
        }
    }

    /** when called, will call virtual call chain to register all of the tick functions */
    // 060 - Foundation - CreateWorld * - UActorComponent::RegisterAllComponentTickFunctions
    void RegisterAllComponentTickFunctions(bool bRegister)
    {
        // components don't have tick functions until they are registered with the world
        // haker: bRegistered becomes true when OnRegister() is called (== registered with the world)
        if (bRegistered)
        {
            // prevent repeated redundant attempts
            if (bTickFunctionsRegistered != bRegister)
            {
                // see UActorComponent::RegisterComponentTickFunctions (goto 61)
                RegisterComponentTickFunctions(bRegister);
                bTickFunctionsRegistered = bRegister;
            }
            
            if (bAsyncPhysicsTickEnabled)
            {
                // haker: skip AsyncPhysicsTickEnabled():
                // - this is the TickFunction as we covered that multiple tick functions can be registered
                RegisterAsyncPhysicsTickEnabled(bRegister);
            }
        }

        // search (goto 60)
    }

    /** called when a component is created (not loaded); this can happen in the editor or during gameplay */
    virtual void OnComponentCreated()
    {
        ensure(!bHasBeenCreated);
        bHasBeenCreated = true;
    }

    /** recalculate the value of our component to world transform */
    // haker: the specific example is all classes inherited from USceneComponent
    // - we are going to defer looking this function later
    virtual void UpdateComponentToWorld(EUpdateTransformFlags UpdateTransformFlags = EUpdateTransformFlags::None, ETeleportType Teleport = ETeleportType::None) {}

    /** set this component's tick functions to be enabled or disabled; only has an effect if the function is registered */
    // 053 - Foundation - CreateWorld * - SetComponentTickEnabled
    virtual void SetComponentTickEnabled(bool bEnabled)
    {
        // haker: bCanEverTick is the variable to determine whether tick-function is enabled for ticking (registered)
        // see UObject::IsTemplate (goto 54)
        // - if current ActorComponent is CDO, it doesn't make sense to enable TickFunction
        // IsTemplate -> 이게 무엇인가.
        // 현재 오브젝트 뿐만 아니라 outer들도 전부다 ArchetypeObejct 또는 DefaultObject인지 확인한다. -> 이것들은 실제 사용되는 오브젝트가 아닌 더미로 만들어진 DefaultObject란 뜻이다.
        if (PrimaryComponentTick.bCanEverTick && !IsTemplate())
        {
            // see SetTickFunctionEnabled (goto 55)
            PrimaryComponentTick.SetTickFunctionEnable(bEnabled);
        }
    }

    /** 
     * set the value of bIsActive without causing other side effects to this instance 
     * - Activate(), Deactivate() and SetActive() are preferred in most cases cuz, they respect virtual behavior
     */
    void SetActiveFlag(const bool bNewIsActive)
    {
        bIsActive = bNewIsActive;
    }

    /** activates the SceneComponent, should be overriden by native child classes */
    // 042 - Foundation - CreateWorld ** - UActorComponent::Activate
    virtual void Activate(bool bReset=false)
    {
        // haker: before we are getting into the details of Activate, see FTickFunction related classes first:
        // see AActor::PrimaryActorTick (goto 043)
        // see UActorComponent::PrimaryComponentTick (goto 051)
        if (bReset || ShouldActivate() == true)
        {
            // 052 - Foundation - CreateWorld * - Activate cont.
            // see SetComponentTickEnabled (goto 053)
            SetComponentTickEnabled(true);
            // see SetActiveFlag
            // - bIsActive is the indicator of Activate()
            SetActiveFlag(true);

            OnComponentActivated.Broadcast(this, bReset);
        }
    }

    /** called when a component is registered, after Scene is set, but before CreateRenderState_Concurrent or OnCreatePhysicsState are called */
    // 041 - Foundation - CreateWorld ** - UActorComponent::OnRegister
    virtual void OnRegister()
    {
        bRegistered = true;

        // haker: 
        // as the comment describes, this function is called before Create[Render|Phyiscs]State:
        // - it infers we should update all component transforms correctly
        // - if a component is USceneComponent, it should update all children's transforms
        // see UActorComponent::UpdateComponentToWorld
        UpdateComponentToWorld();

        // haker: when bAutoActivate is enabled, it try to activate on registeration stage
        if (bAutoActivate)
        {
            AActor* Owner = GetOwner();

            // haker: Owner == nullptr which is exact condition we matched for LineBatcher
            // - (!WorldPrivate->IsGameWorld()) means not tyically calling normal game-play
            if (!WorldPrivate->IsGameWorld() 
                // haker: Owner can be nullptr, if a component is contained directly by UWorld
                // e.g. LineBatcher in UWorld
                || Owner == nullptr 
                || Owner->IsActorInitialized)
            {
                // see Activate (goto 42)
                // haker: usually we are not in here!
                Activate(true);
            }
        }
    }

    /** return true; if CreateRenderState() should be called */
    virtual bool ShouldCreateRenderState() const
    {
        // haker:
        // if you look at UStaticMeshComponent::ShouldCreateRenderState(), it will return true:
        // - BUT, if static mesh is compiling, we could defer creating render state until the static mesh becomes ready
        return false;
    }

    /** calls OnRegister, CreateRenderState_Concurrent and OnCreatePhysicsState */
    // 040 - Foundation - CreateWorld ** - UWorld::ExecuteRegisterEvents
    // AActor::RegisterComponent() 이 함수에서 호출되었다
    void ExecuteRegisterEvents(FRegisterComponentContext* Context = nullptr)
    {
        if (!bRegistered)
        {
            // see OnRegister (goto 41)
            // 000 - Foundation - AttachToComponent - BEGIN
            // haker: try to remember how we can reach ExecuteRegisterEvents():
            // - RegisterComponentWithWorld() -> ExecuteRegisterEvents() -> OnRegister()
            // - what does ExecuteRegisterEvents() do?
            // see USceneComponent::OnRegister (goto 001: AttachToComponent)
            OnRegister();
            // 022 - Foundation - AttachToComponent - END
        }

        if (FApp::CanEverRender() && !bRenderStateCreated && WorldPrivate->Scene 
            // haker: look ShouldCreateRenderState() 
            && ShouldCreateRenderState())
        {
            // haker:
            // - remember this
            // - this is the place that we add primitive to render world (world's scene == FScene)

            // 000 - Foundation - CreateRenderState * - BEGIN
            // haker: we make our new render object reflecting world object
            // - FRegisterComponentContext Context is passed as 'nullptr'
            // - see UActorComponent::CreateRenderState_Concurrent (goto 001: CreateRenderState)
            // - see UPrimitiveComponent::CreateRenderState_Concurrent(goto 002: CreateRenderState)
            // 나의 분신을 여기에 만들어 준다.
            CreateRenderState_Concurrent(Context);
            // 015 - Foundation - CreateRenderState * - END
        }

        // haker:
        // we are not going to do deep-dive on physics for a while.
        // 나의 분신을 여기에 만들어 준다.
        CreatePhysicsState(/*bAllowDeferral=*/true);

        // haker:
        // - create[render|physics]state means 'creating the reflection of main world's state into each render-world and physics world'
    }

    /** calls OnUnregister, DestroyRenderState_Concurrent and OnDestroyPhysicsState */
    void ExecuteUnregisterEvents()
    {
        DestroyPhysicsState();

        if (bRenderStateCreated)
        {
            DestroyRenderState_Concurrent();
        }

        if (bRegistered)
        {
            OnUnregister();
        }
    }

    /** 
     * initialize the component
     * - occurs at level startup or actor spawn
     * - this is before BeginPlay (Actor or Component)
     * - requires component to be registered, and bWantsInitializeComponent to be true
     */
    virtual void InitializeComponent()
    {
        bHasBeenInitialized = true;
    }

    /** returns true if instances of this component are created by either the user or simple construction script */
    bool IsCreatedByConstructionScript() const
    {
        return ((CreationMethod == EComponentCreationMethod::SimpleConstructionScript) || (CreationMethod == EComponentCreationMethod::UserConstructionScript));
    }

    /** registers a component with a specific world, which creates any visual/physical state */
    // 039 - Foundation - CreateWorld ** - UActorComponent::RegisterComponentWithWorld
    void RegisterComponentWithWorld(UWorld* InWorld, FRegisterComponentContext* Context = nullptr)
    {
        // if the component was already registered, do nothing
        if (IsRegistered())
        {
            return;
        }

        // haker: it is natural to early-out cuz there is no world to register 
        if (InWorld == nullptr)
        {
            return;
        }

        // if not registered, should not have a scene
        // haker: it means that we can know whether it is registered or not by its existance of WorldPrivate
        check(WorldPrivate == nullptr);

        // haker: UWorld::LineBatcher(ULineBatchComponent)'s owner is nullptr
        AActor* MyOwner = GetOwner();
        check(MyOwner == nullptr || MyOwner->OwnsComponent(this));

        if (!HasBeenCreated)
        {
            // haker: do you remember OnComponentCreated() event covered in AActor?
            // - it is called when ActorComponent is being created
            OnComponentCreated();
        }

        WorldPrivate = InWorld;

        // see ExectueRegisterEvents (goto 40)
        ExecuteRegisterEvents(Context);

        // if not in a game world register ticks now, otherwise defer until BeginPlay
        // if no owner we won't trigger BeginPlay() either so register now in that case as well
        if (!InWorld->IsGameWorld())
        {
            // haker: 
            // - if the world is not game world, we didn't call InitializeComponent()
            //   - see the condition below (MyOwner == nullptr) 
            // - we are debugging with PIE, so we are not in here
            //   - the PIE returns true on IsGameWorld()
            RegisterAllComponentTickFunctions(true);
        }
        else if (MyOwner == nullptr)
        {
            // haker: here is the InitializeComponent() call which we covered in AActor's initialization stages
            // 만약 내가 레지스터 렌더링까지 다 반영된 이후에 무언가 처리를 하고싶으면 InitializeComponent()이걸 오버라이드 해서 처리하면 가능하다 (이런 팁 필요)
            if (!bHasBeenInitialized && bWantsInitializeComponent)
            {
                // see InitializeComponent()
                InitializeComponent();
            }

            // see RegisterAllComponentTickFunctions (goto 60)
            RegisterAllComponentTickFunctions(true);
        }
        else
        {
            // haker: this is **the NORMAL case** we expect
            // see HandleRegisterComponentWithWorld (goto 65)
            MyOwner->HandleRegisterComponentWithWorld(this);
        }

        // if this is a blueprint created component and it has component children they can miss getting registered in some scenarios
        if (IsCreatedByConstructionScript())
        {
            // haker: explain what is SCS(Simple Construction Script) and CCS(Custom Construction Script) with examples (or with editor)
            // - Children will be collected from SCS and CCS, and they have outer object as this UActorComponent
            TArray<UObject*> Children;
            GetObjectsWithOuter(this, Children, true, RF_NoFlags, EInternalObjectFlags::Garbage);

            // haker: iterating Children, and try to RegisterComponentWithWorld (recursive calls)
            for (UObject* Child : Children)
            {
                if (UActorComponent* ChildComponent = Cast<UActorComponent>(Child))
                {
                    // haker: when RegisterComponentWithWorld is called, bRegister will be true
                    if (ChildComponent->bAutoRegister && !ChildComponent->IsRegistered() && ChildComponent->GetOwner() == MyOwner)
                    {
                        // maybe some ActorComponents from SCS or CCS not registered yet, if so, register components
                        ChildComponent->RegisterComponentWithWorld(InWorld);
                    }
                }
            }
        }
    }

    /** return true if the component requires end of frame updates to happen from the game thread */
    virtual bool RequiresGameThreadEndOfFrameUpdates() const
    {
        return false;
    }

    // 003 - Foundation - SceneRenderer * - UActorComponent::MarkForNeededEndOfFrameUpdate
    void MarkForNeededEndOfFrameUpdate()
    {
        UWorld* ComponentWorld = GetWorld();
        if (ComponentWorld)
        {
            // haker: see UWorld::MarkActorComponentForNeededEndOfFrameUpdate (goto 004: SceneRenderer)
            ComponentWorld->MarkActorComponentForNeededEndOfFrameUpdate(this, RequiresGameThreadEndOfFrameUpdates());
        }
    }

    /**
     * uses the bRender[State|Transform|Instances]Dirty to perform any necessary work on this component
     * do NOT call this directly, call MarkRender[State|DynamicData|Instances]Dirty instead
     */
    // 006 - Foundation - SceneRenderer * - UActorComponent::DoDeferredRenderUpdates_Concurrent
    void DoDeferredRenderUpdates_Concurrent()
    {
        // haker: as we expected, depending on bRenderStateDirty, bRenderTransformDirty, we process all pending calls (RecreateRenderState_Concurrent and SendRenderTransform_Concurrent)
        if (bRenderStateDirty)
        {
            RecreateRenderState_Concurrent();
        }
        else
        {
            if (bRenderTransformDirty)
            {
                // update the component's transform if the actor has been moved since it was last updated
                // haker: see UPrimitiveComponent::SendRenderTransform_Concurrent (goto 007: SceneRenderer)
                SendRenderTransform_Concurrent();
            }

            //...
        }
    } 

    // 001 - Foundation - SceneRenderer * - UActorComponent::MarkRenderTransformDirty
    // haker: see where MarkRenderTransformDirty() is called (goto 002: SceneRenderer)
    void MarkRenderTransformDirty()
    {
        // haker: world's transform update(change) can be applied after bRenderStateCreated:
        // - in UActorComponent::CreateRenderState_Concurrent(), we update 'bRenderStateCreated' as true
        if (IsRegistered() && bRenderStateCreated)
        {
            // haker: mark 'bRenderTransformDirty' as true, and register this component to do EOFUpdate(EndOfFrameUpdate)
            // - see UActorComponent::MarkForNeededEndOfFrameUpdate (goto 003: SceneRenderer)
            bRenderTransformDirty = true;
            MarkForNeededEndOfFrameUpdate();
        }
        else if (!IsUnreachable())
        {
            // we don't have a world, do it right now
            DoDeferredRenderUpdates_Concurrent();
        }
    }

    /** indicates that InitializeComponent has been called, but UninitializeComponent has not yet */
    bool HasBeenInitialized() const { return bHasBeenInitialized; }

    /** Tracks whether the component has been added to one of the world's end of frame update lists */
    uint32 GetMarkedForEndOfFrameUpdateState() const { return MarkedForEndOfFrameUpdateState; }

    /** returns whether replication is enabled or not */
    bool GetIsReplicated() const
    {
        return bReplicates;
    }

    /** register this component, creating any rendering/physics state; will also add itself to the outer Actor's components array, if not already present */
    void RegisterComponent()
    {
        Actor* MyOwner = GetOwner();
        UWorld* MyOwnerWorld = (MyOwner ? MyOwner->GetWOrld() : nullptr);
        if (ensure(MyOwnerWorld))
        {
            RegisterComponentWithWorld(MyOwnerWorld);
        }
    }

    /** initializes the list of properties that are modified by the UserConstructionScript */
    void DetermineUCSModifiedProperties()
    {
        if (CreationMethod == EComponentCreationMethod::SimpleConstructionScript)
        {
            TArray<FSimpleMemberReference> UCSModifiedProperties;

            class FComponentPrioertySkipper : public FArchive
            {
                FComponentPropertySkipper()
                    : FArchive()
                {
                    this->SetIsSaving(true);
                    
                    // include properties that would normally skip tagged serialization (e.g. bulk serialization of array properties)
                    ArPortFlags |= PPF_ForceTaggedSerialization;
                }

                virtual bool ShouldSkipProperty(const FProperty* InProperty) const override
                {
                    static const FName MD_SkipUCSModifiedProperties(TEXT("SkipUCSModifiedProperties"));
                    return (InProperty->HasAnyPropertyFlags(CPF_Transient)
                        || !InProperty->HasAnyPropertyFlags(CPF_Edit | CPF_Interp)
                        || InProperty->IsA<FMulticastDelegateProperty>());
                }
            } PropertySkipper;

            UClass* ComponentClass = GetClass();
            UObject* ComponentArchetype = GetArchetype();

            for (TFieldIterator<FProperty> It(ComponentClass); It; ++It)
            {
                FProperty* Property = *It;
                if (Property->ShouldSerializeValue(PropertySkipper))
                {
                    for (int32 Idx=0; Idx<Property->ArrayDim; ++Idx)
                    {
                        uint8* DataPtr = Property->ContainerPtrToValuePtr<uint8>((uint8*)this, Idx);
                        uint8* DefaultValue = Property->ContainerPtrToValuePtrForDefaults<uint8>(ComponentCalss, (uint8*)ComponentArchetype, Idx);
                        if (!Property->Identical(DataPtr, DefaultValue, PPF_DeepCompareInstances))
                        {
                            UCSModifiedProperties.Add(FSimpleMemberReference());
                            FMemberReference::FillSimpleMemberReference<FProperty>(Property, UCSModifiedProperties.Last());
                            break;
                        }
                    }
                }
            }

            FRWScopeLock Lock(AllUCSModifiedPropertiesLock, SLT_Write);
            if (UCSModifiedProperties.Num() > 0)
            {
                AllUCSModifiedProperties.Add(this, MoveTemp(UCSModifiedProperties));
            }
            else
            {
                AllUCSModifiedProperties.Remove(this);
            }
        }
    }

    void ClearUCSModifiedProperties()
    {
        FRWScopeLock Lock(AllUCSModifiedPropertiesLock, SLT_Write);
        AllUCSModifiedProperties.Remove(this);
    }

    /** check whether the component class allows reregistration during ReregisterAllComponents */
    bool AllowReregistration() const { return bAllowReregistration; }

    void ReregisterComponent()
    {
        if (AllowReregisteration())
        {
            if (!IsRegistered())
            {
                return;
            }

            FComponentReregisterContext(this);
        }
    }

    void PostApplyToComponent()
    {
        if (IsRegistered())
        {
            ReregisterComponent();
        }
    }

    /** 
     * Blueprint implementable event for when the component is beginning play, called before its owning actor's BeginPlay
     * or when the component is dynamically created if the Actor has already BegunPlay. 
     */
    UFUNCTION(BlueprintImplementableEvent, meta=(DisplayName = "Begin Play"))
    ENGINE_API void ReceiveBeginPlay();

    /**
     * begin play for the component
     * - called when the owning Actor begins play or when the component is created if the Actor has already begun play
     * - Actor BeginPlay() normally right after PostInitializeComponents but can be delayed for networked or child actors
     * - requires component to be registered and initialized 
     */
    // 011 - Foundation - SpawnActor - UActorComponent::BeginPlay
    virtual void BeginPlay()
    {
        // haker: if it is compiled by BP, call BP's BeginPlay BP node
        if (GetClass()->HasAnyClassFlags(CLASS_CompiledFromBlueprint) || !GetClass()->HasAnyClassFlags(CLASS_Native))
        {
            ReceiveBeginPlay();
        }

        // haker: mark current component is BeginPlay'ed
        bHasBegunPlay = true;
    }

    // 015 - Foundation - CreateWorld ** - UActorComponent's member variables

    /** describes how a component instance will be created */
    // haker: EComponentCreationMethod is just four, I recommend to remember these four methods (not memorizing!)
    // see EComponentCreationMethod (goto 16) 
    EComponentCreationMethod CreationMethod;

    /** main tick function for the component */
    // haker: as we covered FActorTickFunction in AActor, similar functionality is provided
    // 051 - Foundation - CreateWorld * - UActorComponent::PrimaryComponentTick
    // see FActorComponentTickFunction
    FActorComponentTickFunction PrimaryComponentTick;

    /** used for fast removal of end of frame update */
    int32 MarkedForEndOfFrameUpdateArrayIndex;

    // haker: the below bit flags is used to describe the UActorComponent's current initialization state
    // - pop QUIZ:
    //   - what happens if we used all of bits of uint8?
    //     - it will allocate another uint8 after being used up all 8 bits for uint8 (1 byte)

    /** indicates if this ActorComponent is currently registered with a scene */
    uint8 bRegistered : 1;

    /** if the render state is currently created for this component */
    // haker: whether an ActorComponent is created in RenderWorld(FScene)
    uint8 bRenderStateCreated : 1;

    /** does this component automatically register with its owner */
    uint8 bAutoRegister : 1;

    /** whether the component is activated at creation or must be explicitly activated */
    // haker: understand difference between stages (Register vs. Activate)
    // - Register is 'register tickfunction' to TickTaskLevel
    // - Activate is 'enable tickfunction (including cooling-down)'
    uint8 bAutoActivate : 1;

    /** whether the component is currently active */
    uint8 bIsActive : 1;

    /** if true, we call the virtual InitializeComponent */
    uint8 bWantsInitializeComponent : 1;

    // haker:
    // the below bit flag indicates the initialization sequence of UActorComponent
    // OnCreatedComponent() -> InitializeComponent() -> BeginPlay()

    /** indicates that OnCreatedComponent has been called, but OnDestroyedComponent has not yet */
    uint8 bHasBeenCreated : 1;

    /** indicates that InitializeComponent has been called, but UninitializeComponent has not yet */
    uint8 bHasBeenInitialized : 1;

    /** whether we've tried to register tick functions; reset when they are unregistered */
    uint8 bTickFunctionsRegistered : 1;

    /** Is this component in need of its whole state being sent to the renderer? */
	uint8 bRenderStateDirty:1;

    /** Is this component's transform in need of sending to the renderer? */
    uint8 bRenderTransformDirty : 1;

    /** tracks whether the component has been added to one of the world's end of frame update lists */
    // haker: it has 2bits covering 3 values in EComponentMarkedForEndOfFrameUpdateState
    uint8 MarkedForEndOfFrameUpdateState : 2;

    /** track whether the component has been added to the world's pre and end of frame sync list */
    uint8 bMarkedForPreEndOfFrameSync : 1;

    /** if this component currently replicating? should the network code consider it for replication? owning Actor must be replicating first! */
    uint8 bReplicates : 1;

    /** check whether the component class allows reregistration during ReregisterAllComponents */
    uint8 bAllowReregistration : 1;

    /** cached pointer to owning actor */
    // haker: UActorComponent should have its owner as AActor
    mutable AActor* OwnerPrivate;

    /**
     * activation system 
     */
    /** called when the component has been activated, with parameter indicating if it was from a reset */
    FActorComponentActivatedSignature OnComponentActivated;

    /**
     * pointer to the world that this component is currently registered with
     * this is only non-NULL when the component is registered 
     */
    // haker: AActor resides in ULevel -> ULevel resides in UWorld : UActorComponent resides in UWorld~ :)
    UWorld* WorldPrivate;

    static FRWLock AllUCSModifiedPropertiesLock;
    static TMap<UActorComponent*, TArray<FSimpleMemberReference>> AllUCSModifiedProperties;
};
