
/** tick function that calls UActorComponent::ConditionalTick */
struct FActorComponentTickFunction : public FTickFunction
{
    /** AActor component that is the target of this tick */
    class UActorComponent* Target;
};

DECLARE_DYNAMIC_MULTICAST_SPARSE_DELEGATE_TwoParams(FActorComponentActivatedSignature, UActorComponent, OnComponentActivated, UActorComponent*, Component, bool, bReset);

// 16 - Foundation - CreateWorld - EComponentCreationMethod
enum class EComponentCreationMethod : uint8
{
    /** a component that is part of a native class */
    Native,
    /** a component that is created from a template defined in the Component section of Blueprint */
    SimpleConstructionScript,
    /** a dynamically created component, either from the UserConstructionScript or from a Add component node in a Blueprint event graph */
    UserConstructionScript,
};

/**
 * ActorComponent is the base class for components that define reusable behavior that can be added to a different types of Actors
 * ActorComponents that have a transform are known as 'SceneComponents' and those that can be rendered are 'PrimitiveComponents'
 */
// 14 - Foundation - CreateWorld - UActorComponent
// haker: ActorComponent is base class to attach to the AActor:
// - support scene graph (hierarchy): USceneComponent
// - support rendering: UPrimitiveComponent
// see UActorComponent's member variables (goto 15)
class UActorComponent : public UObject
{
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
    // 62 - Foundation - CreateWorld - UActorComponent::SetupActorComponentTickFunction
    bool SetupActorComponentTickFunction(FTickFunction* TickFunction)
    {
        if (TickFunction->bCanEverTick && !IsTemplate())
        {
            AActor* MyOwner = GetOwner();
            if (!MyOwner || !MyOwner->IsTemplate())
            {
                ULevel* ComponentLevel = (MyOwner ? MyOwner->GetLevel() : ToRawPtr(GetWorld()->PersistentLevel));
                // haker: if TickFunction is not registered, just TickState will be updated
                // - we have seen SetTickFunctionEnable(), but let's see one more again briefly
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
    // 61 - Foundation - CreateWorld - UActorComponent::RegisterComponentTickFunctions
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
    // 60 - Foundation - CreateWorld - UActorComponent::RegisterAllComponentTickFunctions
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
    // 53 - Foundation - CreateWorld - SetComponentTickEnabled
    virtual void SetComponentTickEnabled(bool bEnabled)
    {
        // haker: bCanEverTick is the variable to determine whether tick-function is enabled for ticking every frame
        // see UObject::IsTemplate (goto 54)
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
    // 42 - Foundation - CreateWorld - UActorComponent::Activate
    virtual void Activate(bool bReset=false)
    {
        // haker: before we are getting into the details of Activate, see FTickFunction related classes first:
        // see AActor::PrimaryActorTick (goto 43)
        // see UActorComponent::PrimaryComponentTick (goto 51)
        if (bReset || ShouldActivate() == true)
        {
            // 52 - Foundation - CreateWorld - Activate cont.
            // see SetComponentTickEnabled (goto 53)
            SetComponentTickEnabled(true);
            // see SetActiveFlag
            SetActiveFlag(true);

            OnComponentActivated.Broadcast(this, bReset);
        }
    }

    /** called when a component is registered, after Scene is set, but before CreateRenderState_Concurrent or OnCreatePhysicsState are called */
    // 41 - Foundation - CreateWorld - UActorComponent::OnRegister
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
            if (!WorldPrivate->IsGameWorld() 
                // haker: Owner can be nullptr, if a component is contained directly by UWorld
                // e.g. LineBatcher in UWorld
                || Owner == nullptr 
                || Owner->IsActorInitialized)
            {
                // haker: as we covered before, Activate() is usually called on BeginPlay()
                // see Activate (goto 42)
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
    // 40 - Foundation - CreateWorld - UWorld::ExecuteRegisterEvents
    void ExecuteRegisterEvents(FRegisterComponentContext* Context = nullptr)
    {
        if (!bRegistered)
        {
            // see OnRegister (goto 41)
            OnRegister();
        }

        if (FApp::CanEverRender() && !bRenderStateCreated && WorldPrivate->Scene 
            // haker: look ShouldCreateRenderState() 
            && ShouldCreateRenderState())
        {
            // haker:
            // - remember this
            // - this is the place that we add primitive to render world (world's scene == FScene)
            CreateRenderState_Concurrent(Context);
        }

        // haker:
        // we are not going to do deep-dive on physics for a while.
        CreatePhysicsState(/*bAllowDeferral=*/true);

        // haker:
        // - create[render|physics]state means 'creating the reflection of main world's state into each render-world and physics world'
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
    // 39 - Foundation - CreateWorld - UActorComponent::RegisterComponentWithWorld
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
            RegisterAllComponentTickFunctions(true);
        }
        else if (MyOwner == nullptr)
        {
            // haker: here is the InitializeComponent() call which we covered in AActor's initialization stages
            if (!bHasBeenInitialized && bWantsInitializeComponent)
            {
                // see InitializeComponent()
                InitializeComponent();
            }

            // see RegisterAllComponentTickFunctions (goto 60)
            RegisterAllComponentTickFunctions(true);

            // haker: here is the exact case matching UWorld::LineBatcher
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
                    if (ChildComponent->bAutoRegister && !ChildComponent->IsRegistered() && ChildComponent->GetOwner() == MyOwner)
                    {
                        ChildComponent->RegisterComponentWithWorld(InWorld);
                    }
                }
            }
        }
    }

    /** indicates that InitializeComponent has been called, but UninitializeComponent has not yet */
    bool HasBeenInitialized() const { return bHasBeenInitialized; }

    // 15 - Foundation - CreateWorld - UActorComponent's member variables

    /** describes how a component instance will be created */
    // haker: EComponentCreationMethod is just four, I recommend to remember these four methods (not memorizing!)
    // see EComponentCreationMethod (goto 16) 
    EComponentCreationMethod CreationMethod;

    /** main tick function for the component */
    // haker: as we covered FActorTickFunction in AActor, similar functionality is provided
    // 51 - Foundation - CreateWorld - UActorComponent::PrimaryComponentTick
    // see FActorComponentTickFunction
    FActorComponentTickFunction PrimaryComponentTick;

    // haker: the below bit flags is used to describe the UActorComponent's current initialization state
    // - pop QUIZ:
    //   - what happens if we used all of bits of uint8?

    /** indicates if this ActorComponent is currently registered with a scene */
    uint8 bRegistered : 1;

    /** if the render state is currently created for this component */
    uint8 bRenderStateCreated : 1;

    /** does this component automatically register with its owner */
    uint8 bAutoRegister : 1;

    /** whether the component is activated at creation or must be explicitly activated */
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
};
