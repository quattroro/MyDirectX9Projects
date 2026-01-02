
/** overlap info consisting of the primitive and the body that is overlapping */
struct FOverlapInfo
{
    FOverlapInfo(UPrimitiveComponent* InComponent, int32 InBodyIndex)
        : bSweepFrom(false)
    {
        OverlapInfo.HitObjectHandle = FActorInstanceHandle(InComponent ? InComponent->GetOwner() : nullptr);
        OverlapInfo.Component = InComponent;
        OverlapInfo.Item = InBodyIndex;
    }

    int32 GetBodyIndex() const { return OverlapInfo.Item; }

    bool bFromSweep;

    /**
     * information for both sweep and overlap queries:
     * - different parts are valid depending on bFromSweep
     * 1. bFromSweep is true, then FHitResult is completely valid just like a regular sweep result
     * 2. bFromSweep is false, only FHitResult::Component, FHitResult::Actor, FHitResult::Item are valid as this is really just an FOverlapResult 
     */
    FHitResult OverlapInfo;
};

typedef TArrayView<const FOverlapInfo> TOverlapArrayView;
typedef TArray<FOverlapInfo, TInlineAllocator<3>> TInlineOverlapInfoArray;
typedef TArray<const FOverlapInfo*, TInlineAllocator<8>> TInlineOverlapPointerArray;

/** the space for the transform */
// 009 - Foundation - UpdateComponentToWorld - ERelativeTransformSpace
// haker: you can set target space with enum conveniently
// [ ] explain each enum element's base space
enum ERelativeTransformSpace : int8
{
    /** World space transform */
    RTS_World,
    /** Actor space transform. */
    RTS_Actor,
    /** Component space transform. */
    RTS_Component,
    /** Parent bone space transform */
    RTS_ParentBoneSpace,
};

struct ALightWeightInstanceManager : public AActor
{
    /** returns the specific class that this clas manages */
    UClass* GetRepresentedClass() const
    {
        return IsValid(RepresentedClass) ? RepresentedClass : nullptr;
    }

    /** returns the class to use when spawning a new actor */
    virtual UClass* GetActorClassToSpawn(const FActorInstanceHandle& Handle) const
    {
        return RepresentedClass;
    }

    /** check if we already have an actor for this handle, if an actor already exists we set the Actor variable on Handle and return true */
    bool FindActorForHandle(const FActorInstanceHandle& Handle) const
    {
        if (Handle.Actor.IsValid())
        {
            return true;
        }

        const TObjectPtr<AActor>* FoundActor = Actors.Find(Handle.GetInstanceIndex());
        Handle.Actor = FoundActor ? *FoundActor : nullptr;
        return Handle.Actor != nullptr;
    }

    /** return the transform of the instance specified by Handle */
    FTransform GetTransform(const FActorInstanceHandle& Handle) const
    {
        if (FindActorForHandle(Handle))
        {
            return Handle.Actor->GetActorTransform();
        }

        if (ensure(IsIndexValid(Handle.GetInstanceIndex())))
        {
            // return in world space
            return InstanceTransforms[Handle.GetInstanceIndex()] * GetActorTransform();
        }

        return FTransform();
    }

    /** LWI grid size to use for this manager */
    virtual int32 GetGridSize() const
    {
        return LWIGridSize;
    }

    /** helper that converts a position (world space) into a coordinate for the LWI grid */
    FInt32Vector3 ConvertPositionToCoord(const FVector& InPosition) const
    {
        float GridSize = GetGridSize();
        if (GridSize > 0)
        {
            return FInt32Vector3(
                FMath::FloorToInt(InPosition.X / GridSize),
                FMath::FloorToInt(InPosition.Y / GridSize),
                FMath::FloorToInt(InPosition.Z / GridSize)
            );
        }
        return FInt32Vector3::ZeroValue;
    }

    virtual void SetSpawnParameters(FActorSpawnParameters& SpawnParams)
    {
        SpawnParams.OverrideLevel = GetLevel();
        SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        SpawnParams.ObjectFlags = RF_Transactional;
    }

    /** called after actor construction but before other systems see this actor spawn */
    virtual void PreSpawnInitialization(const FActorInstanceHandle& Handle, AActor* SpawnedActor)
    {
        Handle.Actor = SpawnedActor;
        Actors.Add(Handle.GetInstanceIndex(), SpawnedActor);
    }

    /** create an actor to replace the instance specified by handle */
    AActor* ConvertInstanceToActor(const FActorInstanceHandle& Handle)
    {
        // we shouldn't calling this on indices that already have an actor representing them
        if (Actors.Contains(Handle.GetInstanceIndex()) && Actors[Handle.GetInstanceIndex()] != nullptr)
        {
            return Actors[Handle.GetInstanceIndex()];
        }

        // if we're pointing at an invalid index we can't return an actor
        if (ValidIndices.Num() <= Handle.GetInstanceIndex() || ValidIndices[Handle.GetInstanceIndex()] == false)
        {
            return nullptr;
        }

        // if we have previously spawned an actor for this index, don't spawn again even though we have no current actor entry
        // this may occur if an FActorInstanceHandle was cached before a spawned actor was destroyed and fetched after
        if (DestroyedActorIndices.Contains(Handle.GetInstanceIndex()))
        {
            return nullptr;
        } 

        AActor* NewActor = nullptr;
        // only spawn actors on the server so they are replicated to the clients
        // otherwise, we'll end up with multiple actors
        if (HasAuthority())
        {
            FActorSpawnParameters SpawnParams;
            SetSpawnParameters(SpawnParams);
            SpawnParams.CustomPreSpawnInitialization = [this, &Handle](AActor* SpawnedActor)
            {
                PreSpawnInitialization(Handle, SpawnedActor);
            };

            NewActor = GetLevel()->GetWorld()->SpawnActor<AActor>(GetActorClassToSpawn(Handle), GetTransform(Handle), SpawnParams);
            check(NewActor);

            NewActor->OnDestroyed.AddUniqueDynamic(this, &ALightWeightInstanceManager::OnSpawnedActorDestroyed);

            // haker: nothing implement here, but you can override it
            PostActorSpawn(Handle);
        }

        return NewActor;
    }

    /** return the actor associated with Handle if one exists */
    AActor* FetchActorFromHandle(const FActorInstanceHandle& Handle)
    {
        // make sure the handle doesn't have an actor already
        if (ensure(!Handle.Actor.IsValid()))
        {
            // check if we already have an actor already
            TObjectPtr<AActor>* FoundActor = nullptr;
            if ((FoundActor = Actors.Find(Handle.GetInstanceIndex())) == nullptr)
            {
                // spawn a new actor
                ConvertInstanceToActor(Handle);
            }
            else
            {
                Handle.Actor = *FoundActor;
            }
        }

        // unless we are on the server or the actor has been spawned, this ensure will fail
        // commented out until there is more robust replication
        // ensure(Handle.Actor.IsValid());
        return Handle.Actor.Get();
    }

    /**
     * per-instance data
     * stored in separate arrays to make ticking more efficiently when we need to update everything 
     */
    TArray<FTransform> InstanceTransforms;

    TSubclassOf<AActor> RepresentedClass;

    // keep track of which instances are currently represented by an actor
    TMap<int32, TObjectPtr<AActor>> Actors;

    /** handy way to check indices quickly so we don't need to iterate through the free indices list */
    TArray<bool> ValidIndices;

    /** list of indices that we are no longer using */
    TArray<int32> FreeIndices;

    TArray<int32> DestroyedActorIndices;
};

struct FLightWeightInstanceSubsystem
{
    static TSharedPtr<FLightWeightInstanceSubsystem, ESPMode::ThreadSafe> LWISubsystem;
    static FCriticalSection GetFunctionCS;

    static FLightWeightInstanceSubsystem& Get()
    {
        if (!LWISubsystem)
        {
            FScopeLock Lock(&GetFunctionCS);
            if (!LWISubsystem)
            {
                LWISubsystem = MakeShareable(new FLightWeightInstanceSubsystem());
            }
        }
        return *LWISubsystem;
    }

    /** returns the instance manager that handles the given handle */
    ALightWeightInstanceManager* FindLightWeightInstanceManager(const FActorInstanceHandle& Handle) const
    {
        if (Handle.Manager.IsValid())
        {
            return Handle.Manager.Get();
        }

        if (Handle.Actor.IsValid())
        {
            FReadScopeLock Lock(LWIManagerRWLock);
            // see if we already have a match
            for (ALightWeightInstanceManager* LWIInstance : LWIInstanceManagers)
            {
                if (Handle.Actor->GetClass() == LWIInstance->GetRepresentedClass())
                {
                    const FInt32Vector3 GridCoord = LWIInstance->ConvertPositionToCoord(Handle.GetLocation());
                    const FInt32Vector3 ManagerGridCoord = LWIInstance->ConvertPositionToCoord(LWInstance->GetActorLocation());
                    if (ManagerGridCoord == GridCoord)
                    {
                        return LWIInstance;
                    }
                }
            }
        }

        return nullptr;
    }

    /** returns the actor specified by Handle; this may require loading and creating the actor object */
    AActor* FetchActor(const FActorInstanceHandle& Handle)
    {
        if (Handle.Actor.IsValid())
        {
            return Handle.Actor.Get();
        }

        if (ALightWeightInstanceManager* LWIManager = FindLightWeightInstanceManager(Handle))
        {
            return LWIManager->FetchActorFromHandle(Handle);
        }

        return nullptr;
    }

    /** mutex to make sure we don't change the LWIInstanceManagers array while reading/writing it */
    mutable FRWLock LWIManagersRWLock;
};

TSharedPtr<FLightWeightInstanceSubsystem> FLightWeightInstanceSubsystem::LWISubsystem;
FCriticalSection FLightWeightInstanceSubsystem::GetFunctionCS;

/** handle to a unique object; this may specify a full weight actor or it may only specify the light weight instance that represents the same object */
struct FActorInstanceHandle
{
    bool IsActorValid() const
    {
        return Actor.IsValid();
    }

    /** return the index used internally by the manager */
    int32 GetInstanceIndex() const { return InstanceIndex; }

    FVector GetLocation() const
    {
        if (IsActorValid())
        {
            return Actor->GetActorLocation();
        }

        if (Manager.IsValid())
        {
            return Manager->GetLocation(*this);
        }

        return FVector();
    }

    AActor* GetManagingActor() const
    {
        if (IsActorValid())
        {
            return Actor.Get();
        }

        return Manager.Get();
    }

    /** returns the actor specified by this handle: this may require loading and creating the actor object */
    AActor* FetchActor() const
    {
        if (IsActorValid())
        {
            return Actor.Get();
        }

        return FLightWeightInstanceSubsystem::Get().FetchActor(*this);
    }

    template <typename T>
    bool DoesRepresent() const
    {
        if (const UClass* RepresentClass = GetRepresentClass())
        {
            if constexpr(TIsInterface<T>::Value)
            {
                return RepresentClass->ImplementInterface(T::UClassType::StaticClass());
            }
            else
            {
                return RepresentedClass->IsChildOf(T::StaticClass());
            }
        }
        return false;
    }

    template <typename T>
    T* FetchActor() const
    {
        if (DoesRepresent<T>())
        {
            return CastChecked<T>(FetchActor(), ECastCheckType::NullAllowed);
        }
        return nullptr;
    }

    /** this is cached here for convenience */
    TWeakObjectPtr<AActor> Actor;

    /** identifies the light weight instance manage to use */
    TWeakObjectPtr<ALightWeightInstanceManager> Manager;

    /** identifies the instance within the manager */
    int32 InstanceIndex;

    /** unique identifier for instance represented by the handle */
    uint32 InstanceUID;
};

/** structure containing information about one hit of an overlap test */
// 010 - Foundation - UpdateOverlaps - FOverlapResult
struct FOverlapResult
{
    /** utility to return the Actor that owns the Component that was hit */
    AActor* GetActor() const
    {
        return OverlapObjectHandle.FetchActor();
    }

    // haker: I am not going to see this in detail:
    // - it is related to 'FLightWeightInstanceSubsystem'
    // - FLightWeightInstanceSubsystem is the system to optimize Actors:
    //   - before interacting with Actor, it will be remained as dummy (as instanced static mesh component -> more precisely, HISMC(Hierarchical Instanced Static Mesh Component))
    //   - mid-far distance objects performance (CPU/GPU) optimization
    // - if you are interested in this, refer to 'FLightWeightInstanceSubsystem'
    FActorInstanceHandle OverlapObjectHandle;

    /** PrimitiveComponent that the check hit */
    // haker: overlapped primitive component
    TWeakObjectPtr<class UPrimitiveComponent> Component;

    /**
     * this is the index of the overlapping item
     * For DestructibleComponents, this is the ChunkInfo index
     * For SkeletalMeshComponents, this is the Body Index or INDEX_NONE for single body 
     */
    // haker: you can think of extra-data int32 field
    int32 ItemIndex;
};

/** structure that defines parameters passed into collision function */
// 012 - Foundation - UpdateOverlaps - FCollisionQueryParams
// see FCollisionQueryParams member variables (goto 013 : UpdateOverlaps)
struct FCollisionQueryParams
{
    void AddIgnoredActor(const AActor* InIgnoreActor)
    {
        if (InIgnoreActor)
        {
            IgnoreActors.Add(InIgnoreActor->GetUniqueID());
        }
    }

    void AddIgnoredActors(const TArray<AActor*>& InIngoreActors)
    {
        for (int32 Idx = 0; Idx < InIgnoreActors.Num(); ++Idx)
        {
            AddIgnoredActor(InIgnoreActors[Idx]);
        }
    }

    void Internal_AddIgnoredComponent(const UPrimitiveComponent* InIgnoreComponent)
    {
        if (InIgnoreComponent && IsQueryCollisionEnabled(InIgnoreComponent))
        {
            IgnoreComponents.Add(InIgnoreComponent->GetUniqueID());
            bComponentListUnique = false;
        }
    }

    void AddIgnoredComponents(const TArray<UPrimitiveComponent*>& InIgnoreComponents)
    {
        for (const UPrimitiveComponent* IgnoreComponent : InIgnoreComponents)
        {
            Internal_AddIgnoredComponent(IgnoreComponent);
        }
    }

    FCollisionQueryParams(const AActor* InIgnoreActor)
    {
        AddIgnoredActor(InIgnoreActor);
    }

    /** whether to ignore blocking results */
    // haker: if we are doing overlap checking, bIgnoreBlocks will be 'false'
    bool bIgnoreBlocks;

    // haker: IgnoreComponentsArrayType and IgnoreActorsArrayType's uint32 is same as UObjectBase::InternalIndex
    // - refer to GetUniqueID()
    // - we'll see how we insert it soon

    /** TArray typedef of components to ignore */
    typedef TArray<uint32, TInlineAllocator<8>> IgnoreComponentsArrayType;

    /** TArray typedef of actors to ignore */
    typedef TArray<uint32, TInlineAllocator<4>> IgnoreActorsArrayType;

    // haker: we can specify ignored actors or components functioning like 'filtering'
    // - it is useful to use filtering self-intersection components or actors

    /** set of components to ignore during the trace */
    mutable IgnoreComponentsArrayType IgnoreComponents;

    /** set of actors to ignore during the trace */
    IgnoreActorsArrayType IgnoreActors;
};

/** structure when performing a collision query using a component's geometry */
// 011 - Foundation - UpdateOverlaps - FComponentQueryParams
// see FCollisionQueryParam (goto 012: UpdateOverlaps)
struct FComponentQueryParams : public FCollisionQueryParams
{
};

/** container for indicating a set of collision channels that this object will collide with */
// 023 - Foundation - UpdateOverlaps - FCollisionResponseContainer
// haker: what is channel in FCollisionResponseContainer?
//                                                                                    
//     UPrimitiveComponent                                                            
//       │                                                                            
//       └───BodyInstance: FBodyInstance                                              
//            :reflected state in physics-world for PrimitiveComponent                
//              │                                                                     
//              └───ResponseToChannels: FCollisionResponseContainer                   
//                   :┌─────────┬─────────┬─────────┬─────────┬─────────┬──────┐      
//                    │Channel_0│Channel_1│Channel_2│Channel_3│Channel_4│...   │      
//                    └────▲────┴─────────┴─────────┴─────────┴─────────┴──────┘      
//                         │                                                          
//                         │                                                          
//                     uint8: ECollisionResponse                                      
//                       1.Ignore                                                     
//                       2.Overlap                                                    
//                       3.Block                                                      
//                                                                                    
//                                                                                    
//                                                                                    
//    Case 1)                                                                         
//                        Component1: Channel_0[Overlap]                              
//                       ┌──────────────────────┐                                     
//                       │                      │                                     
//       ┌───────────────┼─────┐     Component1 │                                     
//       │               │*****│▲───────────────┼───────────SUCCESS!!!                
//       │ Component0    └─────┼────────────────┘                                     
//       │                     │                                                      
//       └─────────────────────┘                                                      
//        Component0: Channel_0[Overlap]                                              
//                                                                                    
//                                                                                    
//                                                                                    
//                                                                                    
//    Case 2)                                                                         
//                        Component1: Channel_1[Overlap]                              
//                       ┌──────────────────────┐                                     
//                       │                      │                                     
//       ┌───────────────┼─────┐     Component1 │                                     
//       │               │     │▲───────────────┼───────────FAIL!!!                   
//       │ Component0    └─────┼────────────────┘                                     
//       │                     │                                                      
//       └─────────────────────┘                                                      
//        Component0: Channel_0[Overlap]                                              
//                                                                                    
//                                                                                    
//                                                                                    
//                                                                                    
//    Case 3)                                                                         
//                        Component1: Channel_0[Ignore]                               
//                       ┌──────────────────────┐                                     
//                       │                      │                                     
//       ┌───────────────┼─────┐     Component1 │                                     
//       │               │     │▲───────────────┼───────────FAIL!!!                   
//       │ Component0    └─────┼────────────────┘                                     
//       │                     │                                                      
//       └─────────────────────┘                                                      
//        Component0: Channel_0[Overlap]                                                                                                                          
struct FCollisionResponseContainer
{
    union
    {
        struct
        {
            //Reserved Engine Trace Channels
            uint8 WorldStatic;			// 0
            uint8 WorldDynamic;			// 1
            uint8 Pawn;					// 2
            uint8 Visibility;			// 3
            uint8 Camera;				// 4
            uint8 PhysicsBody;			// 5
            uint8 Vehicle;				// 6
            uint8 Destructible;			// 7

            // Unspecified Engine Trace Channels
            uint8 EngineTraceChannel1;   // 8
            uint8 EngineTraceChannel2;   // 9
            uint8 EngineTraceChannel3;   // 10
            uint8 EngineTraceChannel4;   // 11
            uint8 EngineTraceChannel5;   // 12
            uint8 EngineTraceChannel6;   // 13

            // Unspecified Game Trace Channels
            uint8 GameTraceChannel1;     // 14
            uint8 GameTraceChannel2;     // 15
            uint8 GameTraceChannel3;     // 16
            uint8 GameTraceChannel4;     // 17
            uint8 GameTraceChannel5;     // 18
            uint8 GameTraceChannel6;     // 19
            uint8 GameTraceChannel7;     // 20
            uint8 GameTraceChannel8;     // 21
            uint8 GameTraceChannel9;     // 22
            uint8 GameTraceChannel10;    // 23
            uint8 GameTraceChannel11;    // 24 
            uint8 GameTraceChannel12;    // 25
            uint8 GameTraceChannel13;    // 26
            uint8 GameTraceChannel14;    // 27
            uint8 GameTraceChannel15;    // 28
            uint8 GameTraceChannel16;    // 29 
            uint8 GameTraceChannel17;    // 30
            uint8 GameTraceChannel18;    // 31
        };
        uint8 EnumArray[32];
    };
};

/** structure that defines response container for the query; advanced option */
// 022 - Foundation - UpdateOverlaps - FCollisionResponseParams
// haker: see FCollisionResponseContainer (goto 023)
struct FCollisionResponseParams
{
    /** collision response container for trace filtering; if you'd like to ignore certain channel for this trace, use this struct */
    FCollisionResponseContainer CollisionResponse;
};

/** structure containing information about one hit of a trace, such as point of impact and surface normal at that point */
struct FHitResult
{
    /**
     * get a copy of the HitResult with relevant information reversed
     * for example when receiving a hit from another object, we reverse the normals 
     */
    static FHitResult GetReversedHit(const FHitResult& Hit)
    {
        FHitResult Result(Hit);
        Result.Normal = -Result.Normal;
        Result.ImpactNormal = -Result.ImpactNormal;

        int32 TempItem = Result.Item;
        Result.Item = Result.MyItem;
        Result.MyItem = TempItem;

        FName TempBoneName = Result.BoneName;
        Result.BoneName = Result.MyBoneName;
        Result.MyBoneName = TempBoneName;
        return Result;
    }

    /**
     * normal of the hit in world space, for the object that was hit by the sweep, if any
     * - e.g. if a sphere hits a flat plane, this is a normalized vector pointing out from the plane
     * - in the case of impact with a corner or edge of a surface, usually the "most opposing" normal (opposed to the query direction) is chosen  
     */
    FVector_NetQuantizeNormal ImpactNormal;

    /**
     * normal of the hit in world space, for the object that was swept
     * - equal to ImpactNormal for line tests
     * - this is computed for capsules and spheres, otherwise it will be the same as ImpactNormal
     * e.g. for a sphere test, this is a normalized vector pointing in towards the center of the sphere at the point of impact 
     */
    FVector_NetQuantizeNormal Normal;

    /** if the hit result is from a collision this will have extra info about the item that hit the second item */
    int32 MyItem;

    /** extra data about item that was hit (hit primitive specific) */
    int32 Item;

    /** name of bone we hit (for skeletal meshes) */
    FName BoneName;

    /** name of the _my_ bone which took part in hit event (in case of two skeletal meshes colliding) */
    FName MyBoneName;

    /** PrimitiveComponent hit by the trace */
    TWeakObjectPtr<UPrimitiveComponent> Component;

    /** handle to the object hit by the trace */
    FActorInstanceHandle HitObjectHandle;
};

/**
 * enum that controls the scoping behavior of FScopedMovementUpdate.
 * note that EScopedUpdate::ImmediateUpdates is not allowed within outer scopes that defer updates,
 * and any attempt to do so will change the new inner scope to use deferred updates instead.
 */
namespace EScopedUpdate
{
	enum Type
	{
		ImmediateUpdates,
		DeferredUpdates	
	};
}

/**
 * FScopedMovementUpdate creates a new movement scope, within which propagation of moves may be deferred until the end of the outermost scope that does not defer updates
 * Moves within this scope will avoid updates such as UpdateBounds(), OnUpdateTransform(), UpdatePhysicsVolume(), UpdateChildTransforms() etc until the move is committed (which happens when the last deferred scope goes out of context)
 * 
 * @note: non-deferred scopes are not allowed within outer scopes that defer updates, and any attempt to use one will change the inner scope to use deferred updates 
 */
class FScopedMovementUpdate : public FNoncopyable
{
    typedef TArray<FHitResult, TInlineAllocator<2>> TScopedBlockingHitArray;
    typedef TArray<FOverlapInfo, TInlineAllocator<3>> TScopedOverlapInfoArray;

    enum class EHasMovedTransformOption
	{
		eTestTransform,
		eIgnoreTransform
	};

    enum class EOverlapState
	{
		eUseParent,
		eUnknown,
		eIncludesOverlaps,
		eForceUpdate,
	};

    FScopedMovementUpdate( class USceneComponent* Component, EScopedUpdate::Type ScopeBehavior, bool bRequireOverlapsEventFlagToQueueOverlaps )
        : Owner(Component)
        , OuterDeferredScope(nullptr)
        , CurrentOverlapState(EOverlapState::eUseParent)
        , TeleportType(ETeleportType::None)
        , FinalOverlapCandidatesIndex(INDEX_NONE)
        , bDeferUpdates(ScopeBehavior == EScopedUpdate::DeferredUpdates)
        , bHasMoved(false)
        , bRequireOverlapsEventFlag(bRequireOverlapsEventFlagToQueueOverlaps)
    {
        if (IsValid(Component))
        {
            OuterDeferredScope = Component->GetCurentScopedMovement();
            InitialTransform = Component->GetComponentToWorld();
            InitialRelativeLocation = Component->GetRelativeLocation();
            InitialRelativeRotation = Component->GetRelativeRotation();
            InitialRelativeScale = Component->GetRelativeScale3D();

            if (ScopeBehavior == EScopedUpdate::ImmediateUpdates)
            {
                // we only allow ScopeUpdateImmediately if there is no outer scope, or if the outer scope is also ScopeUpdateImmediately
                if (OuterDeferredScope && OuterDeferredScope->bDeferUpdates)
                {
                    bDeferUpdates = true;
                }
            }

            if (bDeferUpdates)
            {
                Component->BeginScopedMovementUpdate(*this);
            }
        }
        else
        {
            Owner = nullptr;
        }
    }

    ~FScopedMovementUpdate()
    {
        if (bDeferUpdates && IsValid(Owner))
        {
            Owner->EndScopedMovementUpdate(*this);
        }
        Owner = nullptr;
    }

    bool IsDeferringUpdates() const
    {
        return bDeferUpdates;
    }

    /** returns true if the Component's trasnform differs that at the start of the scoped update */
    bool IsTransformDirty() const
    {
        if (Owner)
        {
            return !InitialTransform.Equals(Owner->GetComponentToWorld(), UE_SMALL_NUMBER);
        }
        return false;
    }

    /** Returns whether movement has occurred at all during this scope, optionally checking if the transform is different (since changing scale does not go through a move). RevertMove() sets this back to false. */
    bool HasMoved(EHasMovedTransformOption CheckTransform) const
    {
        return bHasMoved || (CheckTransform == EHasMovedTransformOption::eTestTransform && IsTransformDirty());
    }

    void SetHasTeleported(ETeleportType InTeleportType)
    {
        // Request an initialization. Teleport type can only go higher - i.e. if we have requested a reset, then a teleport will still reset fully
        TeleportType = ((InTeleportType > TeleportType) ? InTeleportType : TeleportType); 
    }

    /** notify this scope that the given inner scope completed its update (i.e. is going out of scope) only occurs for deferred updates */
    void OnInnerScopeComplete(const FScopedMovementUpdate& InnerScope)
    {
        if (IsValid(Owner))
        {
            checkSlow(IsDeferringUpdates());
            checkSlow(InnerScope.IsDeferringUpdates());
            checkSlow(InnerScope.OuterDeferredScope == this);

            // combine with the next item on the stack
            if (InnerScope.HasMoved(EHasMovedTransformOption::eTestTransform))
            {
                bHasMoved = true;

                if (InnerScope.CurrentOverlapState == EOverlapState::eUseParent)
                {
                    // unchanged, use our own
                }
                else
                {
                    // bubble up from inner scope
                    CurrentOverlapState = InnerScope.CurrentOverlapState;
                    if (InnerScope.FinalOverlapCandidateIndex == INDEX_NONE)
                    {
                        FinalOverlapCandidatesIndex = INDEX_NONE;
                    }
                    else
                    {
                        FinalOverlapCandidateIndex = PendingOverlaps.Num() + InnerScope.FinalOverlapCandidateIndex;
                    }
                    PendingOverlaps.Append(InnerScope.GetPendingOverlaps());
                }

                if (InnerScope.Teleport > TeleportType)
                {
                    SetHasTeleported(InnerScope.TeleportType);
                }
            }
            else
            {
                // don't want to invalidate a parent scope when nothing changed in the child.
                checkSlow(InnerScope.CurrentOverlapState == EOverlapState::eUseParent);
            }

            BlockingHits.Append(InnerScope.GetPendingBlockingHit());
        }
    }

    /** add overlaps to the queued overlaps array; this is intended for use only by SceneComponent and its derived classes whenever movement is performed */
    void AppendOverlapsAfterMove(const TOverlapArrayView& NewPendingOverlaps, bool bSweep, bool bIncludesOverlapsAtEnd)
    {
        bHasMoved = true;
        const bool bWasForcing = (CurrentOverlapState == EOverlapState::eForceUpdate);

        if (bIncludesOverlapsAtEnd)
        {
            CurrentOverlapState = EOverlapState::eIncludeOverlaps;
            if (NewPendingOverlaps.Num())
            {
                FinalOverlapCandidatesIndex = PendingOverlaps.Num();
                PendingOverlaps.Append(NewPendingOverlaps.GetData(), NewPendingOverlaps.Num());
            }
            else
            {
                // no new pending overlaps means we're not overlapping anything at the end location
                FinalOverlapCandidatesIndex = INDEX_NONE;
            }
        }
        else
        {
            // we don't know about the final overlaps in the case of a teleport
            CurrentOverlapState = EOverlapState::eUnknown;
            FinalOverlapCandidatesIndex = INDEX_NONE;
            PendingOverlaps.Append(NewPendingOverlaps.GetData(), NewPendingOverlaps.Num());
        }

        if (bWasForcing)
        {
            CurrentOverlapState = EOverlapState::eForceUpdate;
        }
    }

    USceneComponent* Owner;
    FScopedMovementUpdate* OuterDeferredScope;

    EOverlatState CurrentOverlapState;
    ETeleportType TeleportType;

    FTransform InitialTransform;
    FVector InitialRelativeLocation;
    FRotator InitialRelativeRotation;
    FVector InitialRelativeScale;

    /** if not INDEX_NONE, overlaps at this index and beyond in PendingOverlaps are at the final destination */
    int32 FinalOverlapCandidateIndex;
    /** all overlaps encountered during the scope of moves */
    TScopedOverlapInfoArray PendingOverlaps;
    /** all blocking hits encountered during the scope of moves */
    TScopedBlockingHitArray BlockingHits;

    uint8 bDeferUpdates : 1;
    uint8 bHasMoved : 1;
};

EUpdateTransformFlags SkipPhysicsToEnum(bool bSkipPhysics){ return bSkipPhysics ? EUpdateTransformFlags::SkipPhysicsUpdate : EUpdateTransformFlags::None; }

DECLARE_EVENT_ThreeParams(USceneComponent, FTransformUpdated, USceneComponent* /*UpdatedComponent*/, EUpdateTransformFlags /*UpdateTransformFlags*/, ETeleportType /*Teleport*/);
DECLARE_DYNAMIC_MULTICAST_SPARSE_DELEGATE_TwoParams(FIsRootComponentChanged, USceneComponent, IsRootComponentChanged, USceneComponent*, UpdatedComponent, bool, bIsRootComponent);

/** rules for attaching components - needs to be kept synced to EDetachmentRule */
// 004 - Foundation - AttachToComponent - EAttachmentRule
// haker: let's understand the difference between 'KeepRelative' and 'KeepWorld'
// Diagram:
//                                                                                                                                                                                      
//         A                                                                                                                                                                            
//        ┌──────┐                                                                                                                                                                      
//        │      │                                                                                                                                                                      
//        │  x   │                               Attach B to A                                                                                                                          
//        │      │                               1.KeepRelative: you should set RelativeLocation, RelativeRotation, RelativeScale                                                       
//        └──────┘     B                                                                                                                                                                
//  Y                  ┌──────┐                ─────────────────────────────►                                                                                                           
//   ▲                 │      │                                                   Suppose we set RelativeLocation as  (5,0) only then:                                                  
//   │                 │  x   │                                                                                                                                                         
//   │                 │      │                                                                                                                                                         
//   │                 └──────┘                                                               A     B                                                                                   
//   └──────►X                                                                               ┌──────┬──────┐                                                                            
//                                                                                           │      │      │                                                                            
//   'x' is the origin (or pivot)                                                            │  x   │  x   │                                                                            
//                                                                                           │      │      │                                                                            
//                                                                                           └──────┴──────┘                                                                            
//                                                                                     Y                                                                                                
//                                                                                      ▲                  *** ComponentToWorld should be updated!!!                                    
//                                                                                      │                                                                                               
//                                                                                      │                                                                                               
//                                                                                      │                                                                                               
//                                                                                      └──────►X                                                                                       
//                                                                                                                                                                                      
//                                                                                                                                                                                      
//                                                                                                                                                                                      
//                                                                                                                                                                                      
//                                               Attach B to A                                                                                                                          
//                                               2. KeepWorld: you don't need to set any RelativeLocation, RelativeRotation, RelativeScale                                              
//                                                                                                                                                                                      
//                                             ─────────────────────────────►                                                                                                           
//                                                                                            A                                                                                         
//                                                                                           ┌──────┐                                                                                   
//                                                                                           │      │    *** ComponentToWorld remains same!, but RelativeLocation will be updated!!!    
//                                                                                           │  x   │                                                                                   
//                                                                                           │      │                                                                                   
//                                                                                           └──────┘     B                                                                             
//                                                                                     Y                  ┌──────┐                                                                      
//                                                                                      ▲                 │      │                                                                      
//                                                                                      │                 │  x   │                                                                      
//                                                                                      │                 │      │                                                                      
//                                                                                      │                 └──────┘                                                                      
//                                                                                      └──────►X                                                                                       
//                                                                                                                                                                                      
// haker: what are use-cases for KeepRelative and KeepWorld in games?
// 1. KeepRelative:
//    - you can image normal attachments like armor, weapon and etc.
// 2. KeepWorld:
//    - increasing Actor count is heavy in terms of performance espcially if your game is network-game
//    - to reduce actors, we can create spatial-actor having all components which are originally subsided by other multiple actors:
//                                                                                                                                                                                                                                                                                                                  
//          ActorA                                                                                               ActorA                                          
//           ┌──────────────────────────────┐                                                                     ┌─────────────────────────────────────┐        
//           │                              │                                                                     │                                     │        
//           │     ComponentC               │                                                                     │     ComponentC                      │        
//           │    ┌──────┐                  │                                                                     │    ┌──────┐                         │        
//           │    │      │      ComponentB  │                                                                     │    │      │      ComponentB         │        
//           │    │  x   │     ┌──────┐     │                                                                     │    │  x   │     ┌──────┐            │        
//           │    │      │     │      │     │                                                                     │    │      │     │      │            │        
//           │    └──────┘     │  x   │     │                                                                     │    └──────┘     │  x   │            │        
//           │                 │      │     │                                                                     │                 │      │            │        
//           │                 └──────┘     │                                                                     │                 └──────┘            │        
//           │            ComponentA        │                                                                     │            ComponentA               │        
//           │           ┌──────┐           │                                                                     │           ┌──────┐                  │        
//           │           │      │           │                  Reduce Actor count by merging it                   │           │      │                  │        
//           │           │  x   │           │                  ────────────────────────────►                      │           │  x   │                  │        
//           │           │      │           │                  When we attach ActorB's Component to ActorA        │           │      │                  │        
//           │           └──────┘           │                  *** we should keep world tranform!                 │           └──────┘                  │        
//           │                              │                      so, we use 'KeepWorld' option                  │                                     │        
//           └──────────────────────────────┘                                                                     │                                     │        
//                                                                                                                │                                     │        
//                  ActorB                                                                                        │                                     │        
//                  ┌─────────────────────────────┐                                                               │                                     │        
//                  │                             │                                                               │                                     │        
//                  │    ┌──────┐     ┌──────┐    │                                                               │           ┌──────┐     ┌──────┐     │        
//                  │    │      │     │      │    │                                                               │           │      │     │      │     │        
//                  │    │  x   │     │  x   │    │                                                               │           │  x   │     │  x   │     │        
//                  │    │      │     │      │    │                                                               │           │      │     │      │     │        
//     Y            │    └──────┘     └──────┘    │                                                          Y    │           └──────┘     └──────┘     │        
//      ▲           │  ComponentD    ComponentE   │                                                           ▲   │         ComponentD    ComponentE    │        
//      │           └─────────────────────────────┘                                                           │   └─────────────────────────────────────┘        
//      │                                                                                                     │                                                  
//      │                                                                                                     │                                                  
//      └──────►X                                                                                             └──────►X                                          
//      'x' is the origin (or pivot)                                                                          'x' is the origin (or pivot)                       
                                                                                                                                                               
enum class EAttachmentRule : uint8
{
    /** keep current relative transform as the relative transform to the new parent */
    KeepRelative,

    /** automatically calculates the relative transform such that the attached component maintains the same world transform */
    KeepWorld,

    /** snaps transform to the attach point */
    // haker: it just follow AttachParent's location, rotation or scale -> not used that much (cuz, usually you specified its values by yourself)
    // - IMO, SnapToTarget on rotation is useful~
    SnapToTarget,
};

/** rules for attaching components */
// 003 - Foundation - AttachToComponent - FAttachmentTransformRules
struct FAttachmentTransformRules
{
    /** the rule to apply to location when attaching */
    // haker: see EAttachmentRule (goto 004: AttachToComponent)
    EAttachmentRule LocationRule;

    /** the rule to apply to rotation when attaching */
    EAttachmentRule RotationRule;

    /** the rule to apply to scale when attaching */
    EAttachmentRule ScaleRule;

    /** whether to weld simulated bodies together when attaching */
    // haker: what is the meaning of 'weld'?
    // - in the dictionary, 'weld' means 'join together (metal pieces or parts) by heating the surfaces to the point of emlting using blow torch'
    // - firstly, you need to understand what a 'rigid body' is and what a 'shape' is in physics engine:
    //                                                  
    //   RigidBody                                      
    //   ┌─────────────────────────────────┐            
    //   │                                 │            
    //   │      Shape0                     │            
    //   │      ┌───────────┐              │            
    //   │      │           │              │            
    //   │      └───────┬───┤              │            
    //   │              │   │    Shape2    │            
    //   │              │   ├─────────┐    │            
    //   │              │   │         │    │            
    //   │              │   ├─────────┘    │            
    //   │              │   │              │            
    //   │              └───┘              │            
    //   │               Shape1            │            
    //   │                                 │            
    //   └─────────────────────────────────┘            
    //                                                  
    //   - RigidBody is the unit of physics simulation  
    //   - Shape is the unit of physics properties:     
    //      1. ShapeType(Sphere,Box,Capsule,...)        
    //      2. Mass                                     
    //      3. ...etc.                                  
    //
    // - secondly, understand what is the simulation unit(rigid-body) is:
    //                                                                                                         
    //   Case 1) Each Shape has its own Rigid-Body               Case 2) All shapes owned by one Rigid-Body     
    //                                                                                                          
    //        Shape0:RigidBody0                                       RigidBody                                 
    //        ┌───────────┐                                           ┌─────────────────────────────────┐       
    //        │           │                                           │      Shape0                     │       
    //        └───────┬───┤                                           │      ┌───────────┐              │       
    //                │   │    Shape2:RigidBody2                      │      │           │              │       
    //                │   ├─────────┐                                 │      └───────┬───┤              │       
    //                │   │         │                                 │              │   │    Shape2    │       
    //                │   ├─────────┘                                 │              │   ├─────────┐    │       
    //                │   │                                           │              │   │         │    │       
    //                └───┘                                           │              │   ├─────────┘    │       
    //                 Shape1:RigidBody1                              │              │   │              │       
    //                                                                │              └───┘              │       
    //                  │                                             │               Shape1            │       
    //                  │                                             └─────────────────────────────────┘       
    //                  │ Gravity is applied                                                                    
    //                  │                                                              │                        
    //                  │                                                              │                        
    //                  ▼                                                              │ Gravity is applied     
    //                                                                                 │                        
    //                                                                                 │                        
    //               Shape1:RigidBody1                                                 ▼                        
    //                     ┌───┐                                                                                
    //                     │   │                                              Shape0                            
    //                     │   │                                              ┌───────────┐                     
    //    Shape0:RigidBody0│   │ Shape2:RigidBody2                            │           │                     
    //    ┌───────────┐    │   │ ┌─────────┐                                  └───────┬───┤                     
    //    │           │    │   │ │         │                                          │   │    Shape2           
    // ───┴───────────┴────┴───┴─┴─────────┴─                                         │   ├─────────┐           
    //                                                                                │   │         │           
    //                                                                                │   ├─────────┘           
    //                                                                                │   │                     
    //                                                                ────────────────┴───┴─────────────────    
    //                                                                                 Shape1                                                        
    //
    // - thirdly, how a shape and a rigid-body are mapped into in UE4/UE5
    //   - Shape -> Geometry(Shape)
    //   - Rigid-Body -> FBodyInstance
    //
    // - lastly, what is 'welding'?
    //   - normally, each primitive component has its own BodyInstance:
    //     - skeletal mesh component has multiple BodyInstances, cuz usually we assign BodyInstance to each bone
    //   - scene-graph consists of SceneComponents which means it could have multiple BodyInstances:
    //     - physics stability could result in artifacts like jittering
    //     - we can increase the phyiscs stability by reducing simulating units(rigid-body=BodyInstance)
    //   - 'welding' means merging BodyInstances(multiple rigid bodies) into one BodyInstance (the most ancestor)
    //   - Diagram:
    //                                                                                                                                    
    //     Component0:BodyInstance0                                                    Component0:BodyInstance0                                  
    //     ┌───────────┐                                                               ┌───────────┐                                             
    //     │           │                                                               │           │                                             
    //     └───────┬───┤                                                               └───────┬───┤                                             
    //             │   │    Component2:BodyInstance2                                           │   │    Component2:WeldParent=BodyInstance0      
    //             │   ├─────────┐                                                             │   ├─────────┐                                   
    //             │   │         │                   Conceptually,                             │   │         │                                   
    //             │   ├─────────┘                    Merge AllComponent's RididBodies         │   ├─────────┘                                   
    //             │   │                              into Component0(RootComponent)           │   │                                             
    //             └───┘                             ─────────────────────────────►            └───┘                                             
    //              Component1:BodyInstance1                                                    Component1:WeldParent=BodyInstance0              
    //                                                                                                                                           
    //                                                                                                                                           
    //                                                                                                                                           
    //            Component0:AttachChildren[Component1]                                       Component0:AttachChildren[Component1]              
    //                ▲                                                                           ▲      WeldChildren[Component1,Component2]     
    //                │                                                                           │                                              
    //            Component1:AttachChildren[Component2]                                       Component1:AttachChildren[Component2]              
    //                ▲                                                                           ▲                                              
    //                │                                                                           │                                              
    //            Component2                                                                  Component2                                         
    //                                                                                                                                           
    //
    // haker: please when you're trying to use 'Welding', you should have a solid understanding about physics engine!
    // - otherwise, just stick to fake physics using transform and ticking
    bool bWeldSimulatedBodies;
};

/** describes how often this component is allowed to move */
// haker: try to understand Static, Stationary and Movable with examples
namespace EComponentMobility
{
    enum Type : int32
    {
        /**
         * static objects cannot be moved or changed in game
         * - allow baked lighting
         * - fastest rendering 
         */
        Static,

        /**
         * a stationary light will only have its shadowing and bounced lighting from static geometry baked by Lightmass, all other lighting will be dynamic
         * - it can change color and intensity in game
         * - can't move
         * - allow partial baked lighting
         * - dynamic shadows 
         */
        // haker: you can think of 'Stationary' as fixed location, but orientation can be changed
        // - representative example is directional light, point light, ...
        Stationary,

        /**
         * movable objects can be moved and changed in game
         * - totally dynamic
         * - can cast dynamic shadows
         * - slowest rendering 
         */
        Movable,
    };
}

/** rules for detaching components - needs to be kept synced to EAttachmentRule */
enum class EDetachmentRule : uint8
{
    /** keep current relative transform */
    KeepRelative,

    /** automatically calculates the relative transform such that the detached component maintains the same world transform */
    KeepWorld,
};

/** rules for detaching components */
// 009 - Foundation - AttachToComponent - FDetachmentTransformRules
// haker: FDetachmentTransformRules should be consistent with FAttachmentTransformRules
struct FDetachmentTransformRules
{
    /** the rule to apply to location when detaching */
    EDetachmentRule LocationRule;

    /** the rule to apply to rotation when detaching */
    EDetachmentRule RotationRule;

    /** the rule to apply to scale when detaching */
    EDetachmentRule ScaleRule;

    /** whether to call Modify() on the components concerned when detaching */
    // haker: you can think whether a detachment calls Modify and apply detachment modification on Undo/Redo transactionbuffer
    bool bCallModify;
};

/**
 * a SceneComponent has a transform and supports attachment, but has no rendering or collision capabilities
 * useful as a 'dummy' component in hierarchy to offset others 
 */
// 017 - Foundation - CreateWorld ** - USceneComponent
// haker: as we covered, USceneComponent supports scene-graph:
// - what is scene-graph for?
//   - supports hierarichy:
//     - representative example is 'transforms'
// see USceneComponent's member variables (goto 18)
class USceneComponent : public UActorComponent
{
    /**
     * gets the literal value of bVisible
     * this exists so subclasses don't need to have direct access to the bVisible property so it can be made private later 
     * IsVisible and IsVisibleInEditor are preferred in most cases because they respect virtual behavior
     */
    bool GetVisibleFlag() const
    {
        return bVisible;
    }

    /** returns true if this component is visible in the current context */
    virtual bool IsVisible() const
    {
        // if hidden in game, nothing to do
        if (bHiddenInGame)
        {
            return false;
        }

        // haker: GetVisibleFlag() returns 'bVisible' in SceneComponent 
        return (GetVisibleFlag() && CachedLevelCollection->IsVisible());
    }

    // 003 - Foundation - CreateRenderState * - USceneComponent::ShouldRender
    bool ShouldRender() const
    {
        // haker: note that I modify this logic heavily, removing unncessary codes:
        // - I remove editor viewport related codes

        AActor* Owner = GetOwner();
        UWorld* World = GetWorld();

        // haker: if we attached to another Actor (parent Actor), we confirm whether parent Actor should be rendered
        if (Owner)
        {
            if (UChildActorComponent* ParentComponent = Owner->GetParentComponent())
            {
                if (!ParentComponent->ShouldRender())
                {
                    // haker: if parent Actor is not going to be rendered, just early-out
                    return false;
                }
            }
        }

        // haker: see UWorld::UsesGameHiddenFlags breifly
        // - UsesGameHiddenFlags indicates whether the world is game-world or not
        const bool bInGameWorld = World && World->UsesGameHiddenFlags();

        // haker: see USceneComponent::IsVisible()
        // - we also consider the consistency with component's owner (AActor's visibility)
        const bool bShowInGame = IsVisible() && (!Owner || !Owner->IsHidden());
        return (bInGameWorld && bShowInGame);
    }

    const TArray<TObjectPtr<USceneComponent>>& GetAttachChildren() const
    {
        return AttachChildren;
    }

    /** appends all descendants (recursively) of this scene component to the list of Children: NOTE: it does NOT clear the list first */
    void AppendDescendants(TArray<USceneComponent*>& Children) const
    {
        const TArray<USceneComponent*>& AttachedChildren = GetAttachChildren();
        Children.Reserve(Children.Num() + AttachedChildren.Num());

        for (USceneComponent* Child : AttachedChildren)
        {
            if (Child)
            {
                Children.Add(Child);
            }
        }

        for (USceneComponent* Child : AttachedChildren)
        {
            if (Child)
            {
                Child->AppendDescendants(Children);
            }
        }
    }

    /** gets all components that are attached to this component, possibly recursively */
    void GetChildrenComponents(bool bIncludeAllDescendants, TArray<USceneComponent*>& Children) const
    {
        Children.Reset();

        if (bIncludeAllDescendants)
        {
            AppendDescendants(Children);
        }
        else
        {
            const TArray<USceneComponent*>& AttachedChildren = GetAttachChildren();
            Children.Reserve(AttachedChildren.Num());
            for (USceneComponent* Child : AttachedChildren)
            {
                if (Child)
                {
                    Children.Add(Child);
                }
            }
        }
    }

    /** get the SceneComponent we are attached to */
    USceneComponent* GetAttachParent() const
    {
        return AttachParent;
    }

    const FTransform& GetComponentTransform() const
    {
        return ComponentToWorld;
    }

    FName GetAttachSocketName() const
    {
        return AttachSocketName;
    }

    /**
     * gets the literal value of RelativeLocation
     * Note this may be an absolute location if this is a root component (not attached to anything)
     * when IsUsingAbsoluteLocation returns true
     * 
     * this exists so subclasses don't need to have direct access to RelativeLocation property so it
     * can be made private later 
     */
    FVector GetRelativeLocation() const
    {
        return RelativeLocation;
    }

    FVector GetRelativeScale3D() const
    {
        return RelativeScale3D;
    }

    /** get world-space socket transform */
    // 008 - Foundation - UpdateComponentToWorld - GetSocketTransform
    virtual FTransform GetSocketTransform(FName InSocketName, ERelativeTransformSpace TransformSpace=RTS_World) const
    {
        // haker: let's see parameter's type:
        // see ERelativeTransformSpace (goto 009: UpdateComponentToWorld)
        switch (TransformSpace)
        {
            case RTS_Actor:
            {
                // haker: we are going to cover FTransform::GetRelativeTransform
                // see AActor::GetTransform (goto 010: UpdateComponentToWorld)
                // - GetComponentTransform() == ComponentToWorld (component space -> global space)
                // - GetOwner()->GetTransform() == ActorTransform
                //
                // you can think of GetRelativeTransform as divider(/)
                // GetComponentTransform() / GetOwner()->GetTransform()
                // (component -> actor -> world) / (actor -> world) = (component -> actor)
                return GetComponentTransform().GetRelativeTransform(GetOwner()->GetTransform());
            }
            case RTS_Component:
            case RTS_ParentBoneSpace:
            {
                // haker: this case will be overriden by USkeletalMeshComponent
                // - we'll see this soon
                return FTransform::Identity;
            }
            default:
            {
                // haker: RTS_World (global space)
                return GetComponentTransform();
            }
        }
    }

    /** utility function to handle calculating transform with a parent */
    // 007 - Foundation - UpdateComponentToWorld - CalcNewComponentToWorld_GeneralCase
    FTransform CalcNewComponentToWorld_GeneralCase(const FTransform& NewRelativeTransform, const USceneComponent* Parent, FName SocketName) const
    {
        // haker: we are passing Parent as AttachParent()
        if (Parent != nullptr)
        {
            // haker: we are going to see two override functions of GetSocketTransform:
            // - there are several variations, but after you understand two version of GetSocketTransform, we can do it yourself for rest of variations
            // 1. see USceneComponent::GetSocketTransform
            //    (goto 008: UpdateComponentToWorld)
            // 2. see USkinnedMeshComponent::GetSocketTransform
            //    (goto 011: UpdateComponentToWorld)
            const FTransform ParentToWorld = Parent->GetSocketTransform(SocketName);

            // haker: we get parent's world transform by socket and multiply it with NewRelativeTransform
            FTransform NewCompToWorld = NewRelativeTransform * ParentToWorld;
            return NewCompToWorld;
        }
        else
        {
            return NewRelativeTransform;
        }
    }

    /**
     * calculate the new ComponentToWorld transform this component
     * Parent is optional and can be used for computing ComponentToWorld based on arbitrary USceneComponent
     * If Parent is not passed in we use the component's AttachParent
     */
    // 006 - Foundation - UpdateComponentToWorld - CalcNewComponentToWorld
    // haker: the one-thing to note about CalcNewComponentToWorld:
    // - Parent is optional, but usually nullptr:
    //   - by parent = nullptr, parent will defaully be retrieved from GetAttachParent()
    // - lastly, it converts transform from local to global (considering all its parents)
    FTransform CalcNewComponentToWorld(const FTransform& NewRelativeTransform, const USceneComponent* Parent=nullptr, FName SocketName=NAME_None) const
    {
        // haker: if Parent==nullptr, it will try to retrieve AttachParent and AttackSocketName
        SocketName = Parent ? SocketName : GetAttachSocketName();
        Parent = Parent ? Parent : GetAttachParent();
        if (Parent)
        {
            // haker: we skip exceptional cases for dealing with absolute location/rotation/scale
            // see CalcNewComponentToWorld_GeneralCase (goto 007)
            return CalcNewComponentToWorld_GeneralCase(NewRelativeTransform, Parent, SocketName);
        }
        else
        {
            return NewRelativeTransform;
        }
    }

    /** update the bounds of the component */
    // 019 - Foundation - UpdateComponentToWorld - USceneComponent::UpdateBounds
    virtual void UpdateBounds()
    {
        // if use parent bound if attach parent exists, and the flag is set
        // since parents tick first before child, this should work correctly
        if (bUseAttachParentBound && GetAttachParent() != nullptr)
        {
            Bounds = GetAttachParent()->Bounds;
        }
        else
        {
            // calculate new bounds
            const UWorld* const World = GetWorld();
            const bool bIsGameWorld = World && World->IsGameWorld();
            if (!bComputeBoundsOnceForGame || !bIsGameWorld || !bComputedBoundsOnceForGame)
            {
                // haker: to calculate Bounds, call calcBounds() which is virtual method
                // - whenever our ComponentToWorld is updated, Bounds should be updated, cuz it has its position
                Bounds = CalcBounds(GetComponentTransform());
            }
        }
    }

    /** native callback when this component is moved */
    virtual void OnUpdateTransform(EUpdateTransformFlags UpdateTransformFlags, ETeleportType Teleport = ETeleportType::None)
    {

    }

    /** update transforms of any components attached to this one */
    // 020 - Foundation - UpdateComponentToWorld - UpdateChildTransforms
    void UpdateChildTransforms(EUpdateTransformFlags UpdateTransformFlags=EUpdateTransformFlags::None, ETeleportType Teleport=ETeleportType::None)
    {
        if (AttachChildren.Num() > 0)
        {
            const bool bOnlyUpdateIfUsingSocket = !!(UpdateTransformFlags & EUpdateTransformFlags::OnlyUpdateIfUsingSocket);
            const EUpdateTransformFlags UpdateTransformNoSocketSkip = ~EUpdateTransformFlags::OnlyUpdateIfUsingSocket & UpdateTransformFlags;

            // haker: final UpdateTransformFlag:
            // 1. strip EUpdateTransformFlags::OnlyUpdateIfUsingSocket off
            // 2. append EUpdateTransformFlags::PropagateFromParent
            const EUpdateTransformFlags UpdateTransformFlagsFromParent = UpdateTransformNoSocketSkip | EUpdateTransformFlags::PropagateFromParent;

            for (USceneComponent* ChildComp : GetAttachChildren())
            {
                if (ChildComp != nullptr)
                {
                    // update child if it's never been updated
                    // haker: if ChildComp is not updated ever-yet, just call UpdateComponentToWorld with UpdateTransformFlagsFromParent
                    if (!ChildComp->bComponentToWorldUpdated)
                    {
                        ChildComp->UpdateComponentToWorld(UpdateTransformFlagsFromParent, Teleport);
                    }
                    else
                    {
                        // if we're updating child only if it's using a socket, skip if that's not the case
                        // haker: when EUpdateTransformFlags::OnlyUpdateIfUsingSocket is enabled, check whether socket name exists or not
                        if (bOnlyUpdateIfUsingSocket && (ChildComp->AttachSocketName == NAME_None))
                        {
                            continue;
                        }

                        // haker: note that we recursively call UpdateComponentToWorld for AttachChildren
                        ChildComp->UpdateComponentToWorld(UpdateTransformFlagsFromParent, Teleport);
                    }
                }
            }
        }
    }

    // 018 - Foundation - UpdateComponentToWorld - PropagateTransformUpdate
    void PropagateTransformUpdate(bool bTransformChanged, EUpdateTransformFlags UpdateTransformFlags=EUpdateTransformFlags::None, ETeleportType Teleport=ETeleportType::None)
    {
        // haker: we skip DeferringMovementUpdates() which is the trick to optimize(reduce) overlapping calculation
        if (IsDeferringMovementUpdates())
        {
            //...
            return;
        }

        // haker: we propagate transform-update by iterating AttachedChildren (scene-graph)
        const TArray<USceneComponent*>& AttachedChildren = GetAttachChildren();
        FPlatformMisc::Prefetch(AttachChildren.GetData());

        // haker: note that what bTransformChanged differs in PropgateTransformUpdate:
        // - overal process is same:
        //   1. UpdateBounds
        //   2. if bRegistered==True, MarkRenderTransformDirty()
        //   3. UpdateChildTransforms
        //      - recursive call on UpdateComponentToWorld() in AttachChildren
        // - the only difference is:
        //   - call OnUpdateTransform() and TransformUpdated.Broadcast()
        if (bTransformChanged)
        {
            // see USceneComponent::UpdateBounds (goto 019: UpdateComponentToWorld) 
            // haker: various UpdateBounds from all classes inherited from USceneComponent:
            // - UPrimitiveComponent, UStaticMeshComponent, USkeletalMeshComponent, ...
            UpdateBounds();

            // if registered, tell subsystems about the change in transform
            if (bRegistered)
            {
                // call OnUpdateTransform if this components wants it
                if (bWantsOnUpdateTransform)
                {
                    OnUpdateTransform(UpdateTransformFlags, Teleport);
                }
                TransformUpdated.Broadcast(this, UpdateTransformFlags, Teleport);

                // flag render transform as dirty
                // haker: for now, we just understand this function's functionality:
                // - it notify render-world(FScene) to update reflected object's transform to be synced with GameWorld
                
                // 002 - Foundation - SceneRenderer * - MarkRenderTransformDirty is used in PropagateTransformUpdate
                // haker: when SceneComponent's transform is changed, we mark it to apply to Scene(RenderWorld)
                MarkRenderTransformDirty();
            }

            // now go and update children
            // - do NOT pass skip physics to children
            //   - this is only used when physics updates us, 
            // - but in that case we really need to update the attached children since they are kinematic
            if (AttachedChildren.Num() > 0)
            {
                // haker: strip EUpdateTransformFlags::SkipPhysicsUpdate off from UpdateTransformFlags
                // see UpdateChildTransforms (goto 020: UpdateComponentToWorld)
                EUpdateTransformFlags ChildrenFlagNoPhysics = ~EUpdateTransformFlags::SkipPhysicsUpdate & UpdateTransformFlags;
                UpdateChildTransforms(ChildrenFlagNoPhysics, Teleport);
            }
        }
        else
        {
            // haker: almost same as above:
            // - bTransformChanged is the variable whether we call OnUpdateTransform and TransformUpdated.Broadcast()
            // - it does **NOT** stop UpdateChildTransforms!
            UpdateBounds();

            if (AttachedChildren.Num() > 0)
            {
                UpdateChildTransforms();
            }

            if (bRegistered)
            {
                MarkRenderTransformDirty();
            }
        }
    }

    // 003 - Foundation - UpdateComponentToWorld - UpdateComponentToWorldWithParent
    void UpdateComponentToWorldWithParent(USceneComponent* Parent, FName SocketName, EUpdateTransformFlags UpdateTransformFlags, const FQuat& RelativeRotationQuat, ETeleportType Teleport = ETeleportType::None)
    {
        // haker: before look through the logic, we see parameters' types:
        // 1. EUpdateTransformFlags (goto 004)
        //    - initially, UpdateComponentToWorldWithParent is called usually EUpdateTransformFlags::None is set
        // 2. ETeleport (goto 005 : UpdateComponentToWorld)
        //    - also note that its initial value is ETeleport::None

        // if our parent hasn't been updated before, we'll need walk up our parent attach hierarchy
        // haker: there is no guarantee that developer calls UpdateComponentToWorldWithParent on root of scene graph(SceneComponent):
        // - so make sure successive parent's ComponentToWorld is up-to-date wherever you call UpdateComponentToWorld()
        //                                                  ┌───Component4                                                    
        //                                                  │                                                                 
        //                                 ┌──Component2◄───┤                                                                 
        //                                 │                │  ┌──────────┐                                                   
        //                                 │                └──┤Component5│◄─────Component6                                   
        // Component0◄───────Component1◄───┤                   └──────────┘                                                   
        //                                 │                       ▲                                                          
        //                                 │                       │                                                          
        //                                 └──Component3           │                                                          
        //                                                         │                                                          
        //                                                         │                                                          
        //                                                         │                                                          
        //                                                   When Component5's UpdateComponentToWorldWithParent is called     
        //                                                    - Recursively make sure Parent's UpdateComponentToWorld is up-to-date
        if (Parent && !Parent->bComponentToWorldUpdated)
        {
            // haker: also NOTE that before calling Parent->UpdateComponentToWorld, we check whether bComponentToWorldUpdate is 'true' or not:
            // - prevent dup calls on UpdateComponentToWorld()
            Parent->UpdateComponentToWorld();

            //                                                                                                                         
            //                                                      ┌───Component4                                                     
            //                                                      │                                                                  
            //                                     ┌──Component2◄───┤                                                                  
            //                                     │                │  ┌──────────┐                                                    
            //                                     │                └──┤Component5│◄─────Component6                                    
            //     Component0◄───────Component1◄───┤                   └──────────┘                                                    
            //                                     │                                                                                   
            //                                     │                                                                                   
            //                                     └──Component3                                                                       
            //                                                                                                                         
            // CASE 1)                                                                                                                 
            //     Component0's bComponentToWorldUpdated : False                                                                       
            //     Component1's bComponentToWorldUpdated : False                                                                       
            //     Component2's bComponentToWorldUpdated : False                                                                       
            //                                                                                                                         
            //     [Component0, Component1, Component2, Component3, Component4, Component5, Component6]                                
            //                                                                                                                         
            // CASE 2)                                                                                                                 
            //     Component0's bComponentToWorldUpdated : True                                                                        
            //     Component1's bComponentToWorldUpdated : True                                                                        
            //     Component2's bComponentToWorldUpdated : False                                                                       
            //                                                                                                                         
            //     [Component2, Component4, Component5, Component6]                                                                    
            //                                                                                                                         
            // CASE 3)                                                                                                                 
            //     Component0's bComponentToWorldUpdated : False                                                                       
            //     Component1's bComponentToWorldUpdated : True                                                                        
            //     Component2's bComponentToWorldUpdated : False                                                                       
            //                                                                                                                         
            //     [???]                                              
            
            // updating the parent may (depending on if we were already attached to parent) result in our being updated, so just return
            // haker: can you catch why we try to get early-out after calling Parent->UpdateComponentToWorld()?
            if (bComponentToWorldUpdated)
            {
                return;
            }
        }

        // haker: NOW, we understand what bComponentToWorldUpdated is for
        // - also note that bComponentToWorldUpdated is only used initially:
        //   - after bComponentToWorldUpdated is set to true, before destroying the component, it remains bComponentToWorldUpdated as 'true'
        bComponentToWorldUpdated = true;

        FTransform NewTransform(NoInit);
        {
            // haker: note that RelativeRotationQuat is the cached converted value from FRotator to FQuat
            const FTransform RelativeTransform(RelativeRotationQuat, GetRelativeLocation(), GetRelativeScale3D());

            // see CalcNewComponentToWorld (goto 006)
            NewTransform = CalcNewComponentToWorld(RelativeTransform, Parent, SocketName);
        }

        // if transform has changed
        // haker: bHasChanged is determined by comparing NewTransform and Component's ComponentToWorld
        // - by 'bHasChanged', we can choose whether we propgate lightly or heavily
        bool bHasChanged;
        {
            bHasChanged = !GetComponentTransform().Equals(NewTransform, UE_SMALL_NUMBER);
        }

        // we propgate here based on more than just the transform changing, as other components may depend on the teleport flag
        // to detect transforms out of the component direct hierarchy (such as the actor transform)
        if (bHasChanged || 
            // haker: if Teleport is not None, we should update transform, so we consider it bHasChanged as 'true'
            Teleport != ETeleportType::None)
        {
            // update transform
            ComponentToWorld = NewTransform;

            // haker: see the below version of PropagateTransformUpdate which we pass bTransformChanged as 'false'
            // see PropagateTransformUpdate (goto 018: UpdateComponentToWorld)
            PropagateTransformUpdate(true, UpdateTransformFlags, Teleport);
        }
        else
        {
            PropagateTransformUpdate(false);
        }

        // haker: even though ComponentToWorld is not changed, we recursively call UpdateChildTransforms()
    }

    // 002 - Foundation - UpdateComponentToWorld - UpdateComponentToWorld
    // haker: note that USceneComponent's UpdateComponentToWorld is 'final'
    virtual void UpdateComponentToWorld(EUpdateTransformFlags UpdateTransformFlags = EUpdateTransformFlags::None, ETeleport Teleport=ETeleport::None) override final
    {
        // see UpdateComponentToWorldWithParent (goto 003)
        UpdateComponentToWorldWithParent(GetAttachParent(), GetAttachSocketName(), UpdateTransformFlags, RelativeRotationCache.RotatorToQuat(GetRelativeRotation()), Teleport);
    }

    /** call UpdateComponentToWorld if bComponentToWorldUpdated is false */
    // 001 - Foundation - UpdateComponentToWorld - ConditionalUpdateComponentToWorld
    void ConditionalUpdateComponentToWorld()
    {
        // haker: bComponentToWorldUpdated can be known later in UpdateComponentToWorld()
        // - preventing infinite-looping from recursive call while updating children's ComponentToWorld
        // see bComponentToWorldUpdated bit flag member variable breifly
        if (!bComponentToWorldUpdated)
        {
            // see UpdateComponentToWorld (goto 002)
            UpdateComponentToWorld();
        }
    }

    /** return location of the component in world space */
    FVector GetComponentLocation() const
    {
        return GetComponentTransform().GetLocation();
    }

    /** return rotation quaternion of the component, in world space */
    FQuat GetComponentQuat() const
    {
        return GetComponentTransform().GetRotation();
    }

    FVector& GetRelativeLocation_DirectMutable()
    {
        return RelativeLocation;
    }

    /**
     * sets the value of RelativeLocation without causing other side effects to this instance
     * you should not use this method. the standard SetRelativeLocation variants should be used 
     */
    void SetRelativeLocation_Direct(const FVector NewRelativeLocation)
    {
        GetRelativeLocation_DirectMutable() = NewRelativeLocation;
    }

    FRotator& GetRelativeRotation_DirectMutable()
    {
        return RelativeRotation;
    }

    void SetRelativeRotation_Direct(const FRotator NewRelativeRotation)
    {
        GetRelativeRotation_DirectMutable() = NewRelativeRotation;
    }

    /** internal helper, for use from MoveComponent(). special codepath since the normal setters call MoveComponent */
    // 003 - Foundation - UpdateOverlaps - InternalSetWorldLocationAndRotation
    bool InternalSetWorldLocationAndRotation(FVector NewLocation, const FQuat& RotationQuat, bool bNoPhysics=false, ETeleportType Teleport=ETeleportType::None)
    {
        FQuat NewRotationQuat(RotationQuat);

        // if attached to something, transform into local space
        // haker: parameters (NewLocation and RotationQuat) are based on Relative Space (not global space)
        if (GetAttachParent() != nullptr)
        {
            // haker: we covered how GetSocketTransform() works
            FTransform const ParentToWorld = GetAttachParent()->GetSocketTransform(GetAttachSocketName());

            // in order to support mirroring you'll have to use FTransform.GetRelativeTransform because negative scale should flip the rotation
            // haker: how negative scaling works?
            //                                                                           
            //                                                               
            //        ┌────────────┐                       ┌────────────┐    
            //        │            │                       │            │    
            //        │            │                       │            │    
            //        │            │      scale(-1,1)      │            │    
            //   y    │   +        │    ──────────────►    │        +   │    
            //   ▲    │            │                       │            │    
            //   │    └────────────┘                       └────────────┘    
            //   │                                                           
            //   └────►x                                   ┌────────────┐    
            //                            scale(-1,-1)     │            │    
            //                           ─────────────►    │        +   │    
            //                                             │            │    
            //                                             │            │    
            //                                             │            │    
            //                                             └────────────┘    
            //
            // - negative scaling is frequently used for variation of static meshes:
            //   - apply negative scaling with one model, we can achieve geometry variations                                                   
                                                               
            if (FTransform::AnyHasNegativeScale(GetRelativeScale3D(), ParentToWorld.GetScale3D()))
            {
                // haker: properly applying negative scaling, we use GetRelativeTransform()
                FTransform const WorldTransform = FTransform(RotationQuat, NewLocation, GetRelativeScale3D() * ParentToWorld.GetScale3D());
                FTransform const RelativeTransform = WorldTransform.GetRelativeTransform(ParentToWorld);

                NewLocation = RelativeTransform.GetLocation();
                NewRotationQuat = RelativeTransform.GetRotation();
            }
            else
            {
                // haker: otherwise, we use normal linear algebra inverse transform
                NewLocation = ParentToWorld.InverseTransformPosition(NewLocation);
                
                // Quat multiplication works reverse way, make sure you do Parent(-1) * World = Local, not World * Parent(-1) = Local (the way matrix does)
                // haker: note that the order of quat and matrix to apply rotation is opposite!
                // 1. quaternion: Parent(-1) <----- World (right-to-left)
                // 2. matrix: World -----> Parent(-1) (left-to-right)
                NewRotationQuat = ParentToWorld.GetRotation().Inverse() * NewRotationQuat;
            }
        }

        // haker: now check any difference on location or rotation
        const FRotator NewRelativeRotation = RelativeRotationCache.QuatToRotator_ReadOnly(NewRotationQuat);
        bool bDiffLocation = !NewLocation.Equals(GetRelativeLocation());
        bool bDiffRotation = !NewRelativeRotation.Equals(GetRelativeRotation());
        if (bDiffLocation || bDiffRotation)
        {
            // haker: see SetRelativeLocation_Direct and understand what _Direct means
            SetRelativeLocation_Direct(NewLocation);

            // here it is important to compute the quaternion from the rotator and not the opposite
            // in some cases, similar quaternions generate the same rotator, which create issues
            // when the component is loaded, the rotator is used to generate the quaternion, which
            // is then used to compute the ComponentToWorld matrix. when running a blueprint script,
            // it is required to generate that same ComponentToWorld otherwise the FComponentInstanceDataCache
            // might fail to apply to the relevant component. in order to have the exact same transform
            // we must enforce the quaternion to come from the rotator (as in load)
            
            // haker: the comment explains the importance of direction to cache FQuat from FRotator
            // - FQuat is used for calculation NOT for caching
            // - FRotator is for caching data
            // - FQuat cache should be always applied whenever FRotator is changed -> it makes RelativeRotationCache remain up-to-date
            if (bDiffRotation)
            {
                SetRelativeRotation_Direct(NewRelativeRotation);
                RelativeRotationCache.RotatorToQuat(NewRelativeRotation);
            }

            // haker: we alerady covered it:
            // - from here, we can acknowledge that whenever updating location/rotation, it always propagate and recalculate its AttachChildren's ComponentToWorld
            // - meaningless updating location/rotation is performance-hurting
            UpdateComponentToWorldWithParent(GetAttachParent(), GetAttachSocketName(), SkipPhysicsToEnum(bNoPhysics), RelativeRotationCache.GetCacheQuat(), Teleport);

            return true;
        }

        return false;
    }

    /** if true, we can use the old computed overlaps */
    bool ShouldSkipUpdateOverlaps() const
    {
        return bSkipUpdateOverlaps;
    }

    /** replace current PhysicsVolume to input NewVolume */
    // 013 - Foundation - UpdateOverlaps - SetPhysicsVolume
    void SetPhysicsVolume(APhysicsVolume* NewVolume, bool bTriggerNotifiers)
    {
        // owner can be NULL
        // the notifier can be triggered with NULL Actor
        // still the delegate should be still called
        if (bTriggerNotifiers)
        {
            APhysicsVolume* OldPhysicsVolume = PhysicsVolume.Get();
            if (NewVolume != OldPhysicsVolume)
            {
                // haker: only update when PhysicsVolume is newly updated
                AActor* A = GetOwner();
                if (OldPhysicsVolume)
                {
                    // haker: old volume notification (leave)
                    OldPhysicsVolume->ActorLeavingVolume(A);
                }

                // haker: broadcast volume changed event and then update volume
                PhysicsVolumeChangedDelegate.Broadcast(NewVolume);
                PhysicsVolume = NewVolume;

                if (IsValid(NewVolume))
                {
                    // haker: new volume notification (enter)
                    NewVolume->ActorEnteredVolume(A);
                }
            }
        }
        else
        {
            PhysicsVolume = NewVolume;
        }
    }

    /** returns the channel that this component belongs to when it moves */
    virtual ECollisionChannel GetCollisionObjectType() const
    {
        return ECC_WorldDynamic;
    }

    /** updates the PhysicsVolume of this SceneComponent, if bShouldUpdatePhysicsVolume is true */
    // 006 - Foundation - UpdateOverlaps - UpdatePhysicsVolume
    virtual void UpdatePhysicsVolume(bool bTriggerNotifiers)
    {
        if (bShouldUpdatePhysicsVolume && IsValid(this))
        {
            if (UWorld* MyWorld = GetWorld())
            {
                // see APhysicsVolume (goto 007: UpdateOverlap)
                APhysicsVolume* NewVolume = MyWorld->GetDefaultPhysicsVolume();
                // avoid doing anything if there are no other physics volumes in the world
                const int32 NumVolumes = MyWorld->GetNonDefaultPhysicsVolumeCount();
                if (NumVolumes > 0)
                {
                    // avoid a full overlap query if we can do some quick bounds tests against the volumes
                    static int32 MaxVolumesToCheck = 20;
                    bool bAnyPotentialOverlap = true;

                    // only check volumes manually if there are fewer than our limit, otherwise skip ahead to the query
                    // haker: ComponentOverlapMultiByChannel() gives overlap results:
                    // - ComponentOverlapMultiByChannel() is expensive
                    // - if physics volume count is less than 20(MaxVolumesToCheck), Bound checking is more efficient than querying overlap to physics engine
                    if (NumVolumes <= MaxVolumesToCheck)
                    {
                        // haker: first mark it as 'false' cuz, we start to test intersections naively
                        bAnyPotentialOverlap = false;

                        // haker: see GetNonDefaultPhysicsVolumeIterator()
                        for (auto VolumeIter = MyWorld->GetNonDefaultPhysicsVolumeIterator(); VolumeIter; ++VolumeIter)
                        {
                            APhysicsVolume* Volume = VolumeIter->Get();
                            if (Volume != nullptr)
                            {
                                // haker: it is preferrable way to get transform data from root-component in AActor
                                // - review AActor::ActorToWorld()
                                const USceneComponent* VolumeRoot = Volume->GetRootComponent();
                                if (VolumeRoot)
                                {
                                    // haker: sphere intersection check is cheaper than box intersection check
                                    // see picture in https://forums.unrealengine.com/t/bound-extents-of-sphere-change/289320/2
                                    if (FBoxSphereBounds::SphereIntersect(VolumeRoot->Bounds, Bounds))
                                    {
                                        if (FBoxSphereBounds::BoxesIntersect(VolumeRoot->Bounds, Bounds))
                                        {
                                            // haker: mark it to calculate overlap testing further
                                            bAnyPotentialOverlap = true;
                                            break;
                                        }
                                    }
                                }
                            }
                        }
                    }

                    // haker: note that if bAnyPotentialOverlap is 'false', we can get optimization benefit above
                    if (bAnyPotentialOverlap)
                    {
                        // check for all volumes that overlap the component
                        // haker: expensive calculation occurs!
                        // see FOverlapResult (goto 010: UpdateOverlaps)
                        TArray<FOverlapResult> Hits;

                        // see FComponentQueryParams (goto 011: UpdateOverlaps)
                        // haker: GetOwner() is registered as ignored actor in FComponentQueryParams
                        FComponentQueryParams Params(SCENE_QUERY_STAT(UpdatePhysicsVolume), GetOwner());
                        Params.bIgnoreBlocks = true; // only care about overlaps

                        const UPrimitiveComponent* SelfAsPrimitive = Cast<UPrimitiveComponent>(this);

                        // haker: overlap results will be accumulated in Hits: TArray<FOverlapResult>
                        if (SelfAsPrimitive)
                        {
                            MyWorld->ComponentOverlapMultiByChannel(Hits, SelfAsPrimitive, GetComponentLocation(), GetComponentQuat(), GetCollisionObjectType(), Params);
                        }
                        else
                        {
                            // haker: we make dummy sphere geometry to overlap test as 'dot sphere'
                            MyWorld->OverlapMultiByChannel(Hits, GetComponentLocation(), FQuat::Identity, GetCollisionObjectType(), FCollisionShape::MakeSphere(0.f), Params);
                        }

                        // haker: iterating Hits, get the Actor and cast it as APhysicsVolume
                        for (const FOverlapResult& Link : Hits)
                        {
                            APhysicsVolume* const V = Link.OverlapObjectHandle.FetchActor<APhysicsVolume>();
                            // haker: if 'V' priority is higher than 'New Volume', update it
                            if (V && (V->Priority > NewVolume->Priority))
                            {
                                // haker: I skipped unnecessary exception handling logic
                                NewVolume = V;
                            }
                        }
                    }
                }

                // haker: update NewVolume:
                // see SetPhysicsVolume (goto 013: UpdateOverlaps)
                SetPhysicsVolume(NewVolume, bTriggerNotifiers);
            }
        }
    }

    /** return the form of collision for this component */
    virtual ECollisionEnabled::Type GetCollisionEnabled() const
    {
        return ECollisionEnabled::NoCollision;
    }

    /** utility to see if there is any query collision enabled on this component */
    bool IsQueryCollisionEnabled() const
    {
        return CollisionEnabledHasQuery(GetCollisionEnabled());
    }

    /** internal helper for UpdateOverlaps */
    // 005 - Foundation - UpdateOverlaps - USceneComponent::UpdateOverlapsImpl
    virtual bool UpdateOverlapsImpl(const TOverlapArrayView* PendingOverlaps=nullptr, bool bDoNotifies=true, const TOverlapArrayView* OverlapsAtEndLocation=nullptr)
    {
        bool bCanSkipUpdateOverlaps = true;

        // SceneComponent has no physical representation, so no overlaps to test for
        // but, we need to test down the attachment chain since there might be PrimitiveComponents below
        
        // haker: SceneComponent has no state like UPrimitiveComponent::OverlappingComponents
        TInlineComponentArray<USceneComponent*> AttachedChildren;
        AttachedChildren.Append(GetAttachChildren());

        // haker: iterate AttachedChildren recursively call UpdateOverlaps():
        // - child component could be PrimitiveComponent
        for (USceneComponent* ChildComponent : AttachedChildren)
        {
            if (ChildComponent)
            {
                // do not pass on OverlapsAtEndLocation, it only applied to this component
                // haker: we don't pass any pending overlaps (begin/end)
                bCanSkipUpdateOverlaps &= ChildComponent->UpdateOverlaps(nullptr, bDoNotifies);
            }
        }

        if (bShouldUpdatePhysicsVolume)
        {
            // haker: note that there is another virtual call of UPrimitiveComponent::UpdatePhysicsVolume()
            // see UpdatePhysicsVolume (goto 006: UpdateOverlaps)
            UpdatePhysicsVolume(bDoNotifies);

            // haker: if we update physics volume, can't skip update overlap.. why?
            // - for now, I didn't understand why
            // - as you can see above, while iterating AttachedChildren with UpdateOverlaps(), we don't check with bSkipUpdateOverlaps
            // - if we set it as 'false', next time we recursively call UpdateOverlaps(), we call it again
            bCanSkipUpdateOverlaps = false;
        }

        return bCanSkipUpdateOverlaps;
    }
    
    /** queries world and updates overlap tracking state for this component */
    // 004 - Foundation - UpdateOverlaps - USceneComponent::UpdateOverlaps
    bool UpdateOverlaps(const TOverlapArrayView* PendingOverlaps=nullptr, bool bDoNotifies=true, const TOverlapArrayView* OverlapsAtEndLocation=nullptr)
    {
        // haker: skip the DeferringMovementUpdates()
        if (IsDeferringMovementUpdates())
        {
            GetCurrentScopedMovement()->ForceOverlapUpdate();
        }
        else if (!ShouldSkipUpdateOverlaps())
        {
            // haker: UpdateOverlapsImpl() is 'virtual' function to override:
            // - bSkipUpdateOverlaps is the variable whether we can call UpdateOverlaps():
            //   - ClearSkipUpdateOverlaps() can reset 'bSkipUpdateOverlaps'
            // 1. see USceneComponent::UpdateOverlapsImpl (goto 005: UpdateOverlaps)
            // 2. see UPrimitiveComponent::UpdateOverlapsImpl (goto 014: UpdateOverlaps)
            bSkipUpdateOverlaps = UpdateOverlapsImpl(PendingOverlaps, bDoNotifies, OverlapsAtEndLocation);
        }
        return bSkipUpdateOverlaps;
    }

    /** override this method for custom behavior for MoveComponent */
    // 002 - Foundation - UpdateOverlaps - MoveComponentImpl
    virtual bool MoveComponentImpl(const FVector& Delta, const FQuat& NewRotation, bool bSweep, FHitResult* Hit = NULL, EMoveComponentFlags MoveFlags = MOVECOMP_NoFlags, ETeleportType Teleport = ETeleportType::None)
    {
        // fill in optional output param; SceneComponent doesn't sweep, so this is just an empty result
        if (OutHit)
        {
            *OutHit = FHitResult(1.f);
        }

        // haker: if bComponentToWorldUpdated is 'false' (still not initialized yet), update ComponentToWorld (recursively to all AttachChildren and its AttachParent)
        ConditionalUpdateComponentToWorld();

        // early out for zero case
        if (Delta.IsZero())
        {
            // skip if no vector or rotation
            if (NewRotation.Equals(GetComponentTransform().GetRotation(), SCENECOMPONENT_QUAT_TOLERANCE))
            {
                // haker: only delta(translation) and orientation(rotation) diff is nearly zero, don't apply MoveComponent()
                return true;
            }
        }

        // just teleport, sweep is supported for PrimitiveComponents; this will update child components as well
        // haker: see InternalSetWorldLocationAndRotation (goto 003: USceneComponent::InternalSetWorldLocationAndRotation)
        const bool bMoved = InternalSetWorldLocationAndRotation(GetComponentLocation() + Delta, NewRotation, false, Teleport);
    
        // only update overlaps if not deferring update within a scope
        // haker: IsDeferringMovementUpdates() is important to reduce duplicated calls on UpdateOverlaps
        // - for simplicity, we'll skip this for now
        if (bMoved && !IsDeferringMovementUpdates())
        {
            // need to update overlap detection in case PrimitiveComponents are attached
            // see UpdateOverlaps (goto 004: UpdateOverlaps)
            UpdateOverlaps();
        }

        return true;
    }

    /**
     * tries to move the component by a movement vector(Delta) and sets rotation to NewRotation
     * assume that the component's current location is valid and that the component does fit in its current location
     * dispatches blocking hit notifications (if bSweep is true), and calls UpdateOverlaps() after movement to update overlap state
     */
    // 001 - Foundation - UpdateOverlaps - MoveComponent
    // haker: MoveComponent() applies movement vector(delta) and sets orientation by FQuat(NewRotation)
    // 1. dispatches blocking hit notification (bSweep == true)
    // 2. call UpdateOverlaps() after movement to update component's overlap state
    bool MoveComponent(const FVector& Delta, const FQuat& NewRotation, bool bSweep, FHitResult* Hit, EMoveComponentFlags MoveFlags, ETeleportType Teleport)
    {
        // see MoveComponentImpl(goto 002: UpdateOverlaps)
        return MoveComponentImpl(Delta, NewRotation, bSweep, Hit, MoveFlags, Teleport);
    }

    /** set the location and rotation of the component relative to its parent */
    void SetRelativeLocationAndRotation(FVector NewLocation, const FQuat& NewRotation, bSweep=false, FHitResult* OutSweepHitResult=nullptr, ETeleportType Teleport=ETeleportType::None)
    {
        // 000 - Foundation - UpdateComponentToWorld - BEGIN
        // see ConditionalUpdateComponentToWorld (goto 001)
        ConditionalUpdateComponentToWorld();
        // 021 - Foundation - UpdateComponentToWorld - END

        // 000 - Foundation - UpdateOverlaps - BEGIN
        const FTransform DesiredRelTransform(NewRotation, NewLocation);
        // haker: see USceneComponent::CalcNewComponentToWorld briefly
        // - DesiredDelta is the delta of translation between DesiredTransform and ComponentToWorld
        const FTransform DesiredWorldTransform = CalcNewComponentToWorld(DesriedRelTransform);
        const FVector DesiredDelta = FTransform::SubtractTranslation(DesiredWorldTransform, GetComponentTransform());
        // see MovementComponent (goto 001: UpdateOverlaps)
        MoveComponent(DesiredDelta, DesiredWorldTransform.GetRotation(), bSweep, OutSweepHitResult, MOVECOMP_NoFlags, Teleport);
        // 029 - Foundation - UpdateOverlaps - END
    }

    /** set the non-uniform scale of the component relative to its parent */
    void SetRelativeScale3D(FVector NewScale3D)
    {
        if (NewScale3D != GetRelativeScale3D())
        {
            SetRelativeScale3D_Direct(NewScale3D);

            if (UNLIKELY(NeedsInitialization() || OwnerNeedsInitialization()))
            {
                // if we're in the component or actor constructor, don't do anything else
                return;
            }

            UpdateComponentToWorld();

            if (IsRegistered())
            {
                if (!IsDeferringMovementUpdates())
                {
                    UpdateOverlaps();
                }
                else
                {
                    // invalidate cached overlap state at this location
                    TArray<FOverlapInfo> EmptyOverlaps;
                    GetCurrentScopedMovement()->AppendOverlapsAfterMove(EmptyOverlaps, false, false);
                }
            }
        }
    }

    /** set the transform of the component relative to its parent */
    void SetRelativeTransform(const FTransform& NewTransform, bool bSweep=false, FHitResult* OutSweepHitResult=nullptr, ETeleportType Teleport=ETeleportType::None)
    {
        SetRelativeLocationAndRotation(NewTransform.GetTranslation(), NewTransform.GetRotation(), bSweep, OutSweepHitResult, Teleport);
        SetRelativeScale3D(NewTransform.GetScale3D());
    }

    /** set the transform of the component in world space */
    void SetWorldTransform(const FTransform& NewTransform, bool bSweep=false, FHitResult* OutSweepHitResult=nullptr, ETeleportType Teleport=ETeleportType::None)
    {
        // if attached to something, transform into local space
        if (GetAttachParent() != nullptr)
        {
            const FTransform ParentToWorld = GetAttachParent()->GetSocketTransform(GetAttachSocketName());
            FTransform RelativeTM = NewTransform.GetRelativeTransform(ParentToWorld);

            SetRelativeTransform(RelativeTM, bSweep, OutSweepHitResult, Teleport);
        }
        else
        {
            SetRelativeTransform(NewTransform, bSweep, OutSweepHitResult, Teleport);
        }
    }

    /** return true if movement is currently within the scope of an FScopedMovementUpdate */
    bool IsDeferringMovementUpdate() const
    {
        if (ScopedMovementStack.Num() > 0)
        {
            return true;
        }
        return false;
    }

    /** returns the current scoped movement update, or NULL if there is none */
    FScopedMovementUpdate* GetCurrentScopedMovement() const
    {
        if (ScopedMovementStack.Num() > 0)
        {
            return ScopedMovementStack.Last();
        }
        return nullptr;
    }

    bool IsDeferringMovementUpdates(const FScopedMovementUpdate& ScopedUpdate) const
    {
        return ScopedUpdate.IsDeferringUpdates();
    }

    /** returns the form of collision for this component */
    virtual ECollisionEnabled::Type GetCollisionEnabled() const
    {
        return ECollisionEnabled::NoCollision;
    }

    /** Returns the response that this component has to a specific collision channel. */
	virtual ECollisionResponse GetCollisionResponseToChannel(ECollisionChannel Channel) const
    {
        return ECR_Ignore;
    }

    /** compares the CollisionObjectType of each component against the Response of the other, to see what kind of response we should generate */
    ECollisionResponse GetCollisionResponseToComponent(USceneComponent* OtherComponent) const
    {
        // ignore if no component, or either component has no collision
        if (OtherComponent == nullptr || GetCollisionEnabled() == ECollisionEnabled::NoCollision || OtherComponent->GetCollisionEnabled() == ECollisionEnabled::NoCollision)
        {
            return ECR_Ignore;
        }

        ECollisionResponse OutResponse;
        ECollisionChannel MyCollisionObjectType = GetCollisionObjectType();
        ECollisionChannel OtherCollisionObjectType = OtherComponent->GetCollisionObjectType();

        /**
         * we decide minimum of behavior of both will decide the resulting response
         * if A wants to block B, but B wants to touch A, touch will be the result of this collision
         * however if A is static, then we don't care about B's response to A (static) but A's response to B overrides the verdict
         * vice versa, if B is static, we don't care about A's response to static but B's response to A will override the 
         * to make this work, if MyCollisionObjectType is static, set OtherResponse to be ECR_Block, so to be ignored at the end 
         */
        ECollisionResponse MyResponse = GetCollisionResponseToChannel(OtherCollisionObjectType);
        ECollisionResponse OtherResponse = OtherComponent->GetCollisionResponseToChannel(MyCollisionObjectType);
        OutResponse = FMath::Min<ECollisionResponse>(MyResponse, OtherResponse);

        return OutResponse;
    }

    void BeginScopedMovementUpdate(FScopedMovementUpdate& ScopedUpdate)
    {
        checkSlow(IsInGameThread());
        checkSlow(IsDeferringMovementUpdates(ScopedUpdate));
        ScopedMovementStack.Push(&ScopedUpdate);
    }

    void EndScopedMovementUpdate(FScopedMovementUpdate& ScopedUpdate)
    {
        // process top of the stack
        FScopedMovementUpdate* CurrentScopedUpdate = ScopedMovementStack.Pop(false);
        {
            if (ScopedMovementStack.Num() == 0)
            {
                // this was the last item on the stack, time to apply the updates if ncessary
                const bool bTransformChanged = CurrentScopedUpdate->IsTransformDirty();
                if (bTransformChanged)
                {
                    // pass teleport flag if set
                    PropagateTransformUpdate(true, EUpdateTransformFlags::None, CurrentScopedUpdate->TeleportType);
                }

                // we may have moved somewhere and then moved back to the start, we still need to update overlaps if we touched things along the way
                // if no movement and no change in transform, nothing changed
                if (bTransformChanged || CurrentScopeUpdate->bHasMoved)
                {
                    UPrimitiveComponent* PrimitiveThis = Cast<UPrimitiveComponent>(this);
                    if (PrimitiveThis)
                    {
                        // @note: UpdateOverlaps filters events to only consider overlaps where bGenerateOverlapEvents is true for both components, so it's ok if we queued up other overlaps
                        TInlineOverlapInfoArray EndOverlaps;
                        const TOverlapArrayView PendingOverlap(CurrentScopedUpdate->GetPendingOverlaps());
                        const TOptional<TOverlapArrayView> EndOverlapsOptional = CurrentScopedUpdate->GetOverlapsAtEnd(*PrimitiveThis, EndOverlaps, bTransformChanged);
                        UpdateOverlaps(&PendingOverlaps, true, EndOverlapsOptional.IsSet() ? &(EndOverlapsOptional.GetValue()) : nullptr);
                    }
                    else
                    {
                        UpdateOverlaps(nullptr, true, nullptr);
                    }
                }

                // dispatch all deferred blocking hits
                if (CurrentScopedUpdate->BlockingHits.Num() > 0)
                {
                    AActor* const Owner = GetOwner();
                    if (Owner)
                    {
                        // if we have blocking hits, we must be a primitive component
                        UPrimitiveComponent* PrimitiveThis = CastChecked<UPrimitiveComponent>(this);
                        for (const FHitResult& Hit : CurrentScopedUpdate->BlockingHits)
                        {
                            // overlaps may have caused us to be destroyed, as could other queued blocking hits.
                            if (!IsValid(PrimitiveThis))
                            {
                                break;
                            }

                            // collision response may change (due to overlaps or multiple blocking hits), make sure it's still considered blocking.
                            if (PrimitiveThis->GetCollisionResponseToComponent(Hit.GetComponent()) == ECR_Block)
                            {
                                PrimitiveThis->DispatchBlockingHit(*Owner, Hit);
                            }						
                        }
                    }
                }
            }
            else
            {
                // combine with next item on the stack
                FScopedMovementUpdate* OuterScopedUpdate = ScopedMovementStack.Last();
                OuterScopedUpdate->OnInnerScopeComplete(*CurrentScopedUpdate);
            }
        }
    }

    /** compares the CollisionObjectType of each component against the Response of the other, to see what kind of response we should generate */
    ECollisionResponse GetCollisionResponseToComponent(USceneComponent* OtherComponent) const
    {
        // ignore if no component, or either component has no collision
        if (OtherComponent == nullptr || GetCollisionEnabled() == ECollisionEnabled::NoCollision || OtherComponent->GetCollisionEnabled() == ECollisionEnabled::NoCollision)
        {
            return ECR_Ignore;
        }

        ECollisionResponse OutResponse;
        ECollisionChannel MyCollisionObjectType = GetCollisionObjectType();
        ECollisionChannel OtherCollisionObjectType = OtherComponent->GetCollisionObjectType();

        /**
         * we decide minimum of behavior of both will decide the resulting response
         * if A wants to block B, but B wants to touch A, touch will be the result of this collision
         * However if A is static, then we don't care about B's response to A(static) but A's response to B overrides the verdict
         * Vice versa, if B is static, we don't care about A's response to static but B's response to A will override the verdict
         * To make this work, if MyCollisionObjectType is static, set OtherResponse to be ECR_Block, so to be ignored at the end
         */
        ECollisionResponse MyResponse = GetCollisionResponseToChannel(OtherCollisionObjectType);
        ECollisionResponse OtherResponse = OtherComponent->GetCollisionResponseToChannel(MyCollisionObjectType);

        OutResponse = FMath::Min<ECollisionResponse>(MyResponse, OtherResponse);

        return OutResponse;
    }

    /** gets whether or not the cached PhysicsVolume this component overlaps should be updated whent he component is moved */
    bool GetShouldUpdatePhysicsVolume() const
    {
        return bShouldUpdatePhysicsVolume;
    }

    /** return the ULevel that this Component is part of */
    ULevel* GetComponentLevel() const
    {
        // for model components Level is outer object
        AActor* MyOwner = GetOwner();
        return (MyOwner ? MyOwner->GetLevel() : GetTypeOuter<ULevel>());
    }

    void SetAttachParent(USceneComponent* NewAttachParent)
    {
        AttachParent = NewAttachParent;
    }

    void SetAttachSocketName(FName NewSocketName)
    {
        AttachSocketName = NewSocketName;
    }

    /** walk up the attachment chain to see if this component is attached to the supplied component: If TestComp == this, return false */
    // 007 - Foundation - AttachToComponent - USceneComponent::IsAttachedTo
    bool IsAttachedTo(const USceneComponent* TestComp) const
    {
        if (TestComp != nullptr)
        {
            for (const USceneComponent* Comp=this->GetAttachParent(); Comp != nullptr; Comp = Comp->GetAttachParent())
            {
                if (TestComp == Comp)
                {
                    return true;
                }
            }
            return false;
        }
    }

    /**
     * initialize desired AttachParent and SocketName to be attached to when the component is registered
     * generally intended to be called from its Owning Actor's constructor and should be preferred over AttachToComponent when a component is not registered 
     */
    // 006 - Foundation - AttachToComponent - USceneComponent::SetupAttachment
    void SetupAttachment(USceneComponent* InParent, FName InSocketName=NAME_None)
    {
        if (InParent != AttachParent || InSocketName != AttachSocketName)
        {
            // haker: note that SetupAttachment() should be called before registering component!
            // - you can think of SetupAttachment() as deferring AttachToComponent to OnRegister()
            if (ensureMsgf(!bRegistered, TEXT("SetupAttachment() should only be used to initialize AttachParent and AttachSocketName for a future AttachToComponent")))
            {
                if (ensureMsgf(InParent != this, TEXT("Cannot attach a component to itself")))
                {
                    // haker: see IsAttachedTo (goto 007)
                    if (ensureMsgf(InParent == nullptr || !InParent->IsAttachedTo(this), TEXT("Setting up attachment would create a cycle")))
                    {
                        if (ensureMsgf(AttachParent == nullptr || !AttachParent->AttachChildren.Contains(this), TEXT("SetupAttachment cannot be used once a component has already has AttachTo used to connect it to a parent")))
                        {
                            // haker: see SetAttachParent and SetAttachSocketName briefly
                            SetAttachParent(InParent);
                            SetAttachSocketName(InSocketName);
                        }
                    }
                }
            }
        }
    }

    /** see if the owning Actor is currently running the UCS */
    // 008 - Foundation - AttachToComponent - IsOwnerRunningUserConstructionScript
    bool IsOwnerRunningUserConstructionScript() const
    {
        AActor* MyOwner = GetOwner();
        // haker: see IsRunningUserConstructionScript() briefly
        return (MyOwner && MyOwner->IsRunningUserConstructionScript());
    }

    /**
     * called after a child scene component is attached/detached from this component
     * NOTE: do not change the attachment state of the child during this call 
     */
    virtual void OnChildAttached(USceneComponent* ChildComponent) {}
    virtual void OnChildDetached(USceneComponent* ChildComponent) {}

    /** called when AttachParent changes, to allow the scene to update its attachment state */
    virtual void OnAttachmentChanged() {}

    /**
     * called when AttachChildren is modified
     * other systems may leverage this to get notificatios for when the value is changed 
     */
    void ModifiedAttachChildren()
    {
        //...
    }

    /** detach this component from whatever it is attached to. automatically unwelds components that are welded together (see WeldTo) */
    // 010 - Foundation - AttachToComponent - USceneComponent::DetachFromComponent
    // haker: as we saw, if weld() is called, unweld() is called in DetachFromComponent()
    virtual void DetachFromComponent(const FDetachmentTransformRules& DetachmentRules)
    {
        // haker: when we don't attach any parent, there is nothing to detach
        if (GetAttachParent() != nullptr)
        {
            AActor* Owner = GetOwner();

            if (UPrimitiveComponent* PrimComp = Cast<UPrimitiveComponent>(this))
            {
                // haker: note that only primitive component is supported by Weld/Unweld:
                // - primitive component is the component who has FBodyInstance(Rigid Body: state of physics world)
                // - we are not going to look into the detail of FBodyInstance:
                //   - let's see how scene-tree is maintined by FBodyInstance for weld/unweld
                // see UPrimitiveComponent::UnWeldFromParent (goto 011: AttachToComponent)
                PrimComp->UnWeldFromParent();
            }

            // due to replication order the ensure below is only valid on server OR if neither the parent nor child are replicated
            // haker: checks replication's problematic unsynchronization:
            // 1. when component need to be replicated, it should be replicated in server
            // 2. or the client who has authority, update its children (detach/attach)
            //    - all attached components should have consistency with its parent in terms of replication:
            //      - parent component(replicated: true)
            //      - child components to be attached should have 'replicated' as 'true'
            if ((Owner && Owner->GetLocalRole() == ROLE_Authority() || !(GetIsReplicated() || GetAttachParent()->GetIsReplicated())))
            {
                ensureMsgf(!bRegistered || GetAttachParent()->GetAttachChildren().Contains(this), TEXT("attempt to detach SceneComponent while not attached"));
            }

            // haker: when DetachmentRules.bCallModify, save changes to undo-transaction buffer
            if (DetachmentRules.bCallModify && !HasAnyFlags(RF_Transient))
            {
                Modify();

                // Attachment is persisted on the child so modify both actors for Undo/Redo but not mark the Parent package dirty
                GetAttachParent()->Modify(/*bAlwaysMarkDirty=*/false);
            }

            // no longer required to tick after the attachment
            // haker: from there, we can assume AttachChildren's tick function is executed after its parent is executed!
            // - let's defer to see this again on AttachToComponent()
            PrimaryComponentTick.RemovePrerequisites(GetAttachParent(), GetAttachParent()->PrimaryComponentTick);

            // haker: now we remove the current component from AttachParent's AttachChildren
            GetAttachParent()->AttachChildren.Remove(this);
            // haker: ClientAttachedChildren is used for replication of AttachChildren:
            // - I list it up on networking part to explain further
            GetAttachParent()->ClientAttachedChildren.Remove(this);

            // haker: notify events (this component is detached)
            GetAttachParent()->OnChildDetached(this);
            GetAttachParent()->ModifiedAttachChildren();

            // haker: set its parent as nullptr, and socket name is 'NAME_None'
            SetAttachParent(nullptr);
            SetAttachSocketName(NAME_None);

            OnAttachmentChanged();

            // if desired, update RelativeLocation and RelativeRotation to maintain current world position after detachment
            // haker: when we set as 'KeepWorld', update this's Relative[Location|Rotation|Scale] as world's transform
            switch (DetachmentRules.LocationRule)
            {
            case EDetachmentRule::KeepRelative:
                break;
            case EDetachmentRule::KeepWorld:
                // or GetComponentLocation, but worried about custom location
                SetRelativeLocation_Direct(GetComponentTransform().GetTranslation());
                break;
            }

            switch (DetachmentRules.RotationRule)
            {
            case EDetachmentRule::KeepRelative:
                break;
            case EDetachmentRule::KeepWorld:
                SetRelativeRotation_Direct(GetComponentRotation());
                break;
            }

            switch (DetachmentRules.ScaleRule)
            {
            case EDetachmentRule::KeepRelative:
                break;
            case EDetachmentRule::KeepWorld:
                SetRelativeScale3D_Direct(GetComponentScale());
                break;
            }

            // calculate transform with new attachment condition
            // haker: we already covered this: iterating its children and update ComponentToWorld as well as its Overlaps, Bounds, etc...
            UpdateComponentToWorld();

            // update overlaps, in case location changed or overlap state depends on attachment
            // haker: note that bDisableDetachmentUpdateOverlaps is marked as false, when DetachFromComponent() is called inside AttachToComponent()
            if (IsRegistered() && !bDisableDetachmentUpdateOverlaps)
            {
                UpdateOverlaps();
            }
        }
    }

    /** clears the skip update overlaps flag: this should be called any time a change to state would prevent the result of UpdateOverlaps: for example attachment, changing collision settings, etc... */
    void ClearSkipUpdateOverlaps()
    {
        if (ShouldSkipUpdateOverlaps())
        {
            bSkipUpdateOverlaps = false;
            if (GetAttachParent())
            {
                GetAttachParent()->ClearSkipUpdateOverlaps();
            }
        }
    }

    /**
     * attach this component to another scene component, optionally at a named socket:
     * - It is valid to call this on components whether or not they have been Registered, however from constructor or when not registered it is preferable to use SetupAttachment
     */
    // 002 - Foundation - AttachToComponent - USceneComponent::AttachToComponent
    bool AttachToComponent(USceneComponent* Parent, const FAttachmentTransformRules& AttachmentRules, FName InSocketName=NAME_None)
    {
        // see FAttachmentTransformRules (goto 003: AttachToComponent)

        // see FUObjectThreadContext (goto 005: FUObjectThreadContext)
        FUObjectThreadContext& ThreadContext = FUObjectThreadContext::Get();
        if (ThreadContext.IsInConstructor > 0)
        {
            // validate that the use of AttachTo in the constructor is just setting up the attachment and not expecting to be able to do anything else
            check(!AttachmentRules.bWeldSimulatedBodies);
            check(AttachmentRules.LocationRule == EAttachmentRule::KeepRelative);
            // haker: when we call AttachToComponent in the constructor of UObject), we can defer AttachToComponent on OnRegister() call as we saw:
            // - we do cheap operation by SetupAttachment()
            // see USceneComponent::SetupAttachment (goto 006: AttachToComponent)
            SetupAttachment(Parent, SocketName);
            //...
            return true;
        }

        if (Parent != nullptr)
        {
            // haker: the underlying statments are valiating whether we can call AttachToComponent() or not
            // - do NOT need to memorize, just skimming it multiple times

            const int32 LastAttachIndex = Parent->AttachChildren.Find(TObjectPtr<USceneComponent>(this));
            const bool bSameAttachParentAndSocket = (Parent == GetAttachParent() && SocketName == GetAttachSocketName());
            if (bSameAttachParentAndSocket && LastAttachIndex != INDEX_NONE)
            {
                // already attached!
                return true;
            }

            if (Parent == this)
            {
                // AttachToSelfWarning: cannot be attached to itself
                return false;
            }

            AActor* MyActor = GetOwner();
            AActor* TheirActor = Parent->GetOwner();

            if (MyActor == TheirActor && MyActor && MyActor->GetRootComponent() == this)
            {
                // AttachToSelfRootWarning: root component cannot be attached to other components in the same actor
                return false;
            }

            // haker: note that IsAttachTo() checks all its parents
            if (Parent->IsAttachedTo(this))
            {
                // AttachCycleWarning: would for cycle!
                return false;
            }

            // haker: you can filter whether component is allowed to attach to parent in SocketName
            if (!Parent->CanAttachAsChild(this, SocketName))
            {
                return false;
            }

            // don't allow components with static mobility to be attached to non-static paraents (except during UCS)
            // see IsOwnerRunningUserConstructionScript (goto 008: AttachToComponent)
            // haker: when running UCS, component and component's parent mobilities are different, failed to AttachToComponent
            if (!IsOwnerRunningUserConstructionScript() && Mobility == EComponentMobility::Static && Parent->Mobility != EComponentMobility::Static)
            {
                // NoStaticToDynamicWarning
                return false;
            }

            // if our template type doesn't match
            // haker: if component and component's parent has same template type, it is allowed to attach!
            if (Parent->IsTemplate() != IsTemplate())
            {
                return false;
            }

            // don't call UpdateOverlaps() when detaching, since we are going to do it anyway after we reattach below
            // aside from a perf benfit this also maintains correct behavior when we don't have KeepWorldPosition set

            // haker: when we have to detach component, keep the overlap state:
            // - it is performance-beneficial
            // - we are going to attach component again, so the overlap state will be up-to-date
            //   - even though attachment is set as EAttachmentRule::KeepRelative, it still works correctly
            const bool bSavedDisableDetachmentUpdateOverlaps = bDisableDetachmentUpdateOverlaps;
            bDisableDetachmentUpdateOverlaps = true;

            if (!ShouldSkipUpdateOverlaps())
            {
                // haker: clear its parent bSkipUpateOverlaps flags
                // - before we detach/attach we clear bSkipUpdateOverlaps flags
                Parent->ClearSkipUpdateOverlaps();
            }

            // haker: see FDetachmentTransformRules (goto 009: AttachToComponent)
            FDetachmentTransformRules DetachmentRules(AttachmentRules, true);

            // make sure we are detached
            if (bSameAttachParentAndSocket 
                && !IsRegistered() 
                && AttachmentRules.LocationRule == EAttachmentRule::KeepRelative 
                && AttachmentRules.RotationRule == EAttachmentRule::KeepRelative 
                && AttachmentRules.ScaleRule == EAttachmentRule::KeepRelative 
                && LastAttachIndex == INDEX_NONE)
            {
                // no sense detaching from what we are about to attach to during registration, as long as relative position is being maintained
                // haker: we should mainly focus on !IsRegistered():
                // 1. bSameAttachParentAndSocket: already attached to same parent and same socket name
                // 2. not registered yet (create state of rendering and physics)
                // 3. attachment rule should be 'KeepRelative'
                // 4. not applied to AttachChildren (LastAttachIndex == INDEX_NONE)
                //
                // - the component doesn't attach parent yet (cuz, LastAttachIndex == INDEX_NONE)
            }
            else
            {
                // see USceneComponent::DetachFromComponent (goto 010: AttachToComponent)
                DetachFromComponent(DetachmentRules);
            }

            // restore detachment update overlaps flag
            bDisableDetachmentUpdateOverlaps = bSavedDisableDetachmentUpdateOverlaps;

            // haker: skip the exception handling in editor-build case
            {
                /**
                 * this code requires some explaining:
                 * - inside the editor we allow user to attach physically simulated objects to other objects
                 *   - this is done for convenience so that users can group things together in hierarchy
                 * - at runtime we must not attach physically simulated objects as it will cause double transform updates, and you should just use a physical constraint if attachment is the desired behavior 
                 * - Note if bWeldSimulatedBodies=true then they actually want to keep these objects simulating together
                 * - We must fixup the relative location, rotation, scale as attachment is no longer valid:
                 *   - Blueprint uses simple construction to try and attach before ComponentToWorld has ever been updated, so we cannot rely on it
                 * - As such we must calculate the proper Relative information
                 * - Also physics state may not be created yet so we use bSimulatePhyiscs to determine if the object has any intention of being physically simulated
                 */
                UPrimitiveComponent* PrimitiveComponent = Cast<UPrimitiveComponent>(this);
                FBodyInstance* BI = PrimitiveComponent ? PrimitiveComponent->GetBodyInstance() : nullptr;
                if (BI && BI->bSimulatePhysics && !AttachmentRules.bWeldSimualtedBodies)
                {
                    //...
                }
            }

            // detach removes all Prerequisite, so will need to add after Detach happens
            // haker: we add prerequisite tick function as parent's tick function:
            // - as a result, this component's tick function is executed after AttachParent's tick function is executed
            PrimaryComponentTick.AddPrerequisite(Parent, Parent->PrimaryComponentTick); // force us to tick after the parent does

            // save pointer from child to parent
            SetAttachParent(Parent);
            SetAttachSocketName(SocketName);

            OnAttachmentChanged();

            // preserve order of previous attachment if valid (in case we're doing a reattach operation inside a loop that might assume the AttachChildren order won't change)
            // don't do this if updating attachment from replication to avoid overwriting addresses in AttachChildren that may be unmapped
            
            // haker: now we understand why LastAttachIndex is needed:
            // - it preserves the order after detachment happens
            // - so after detach and attach, the component's index in AttachChildren remains same
            if (LastAttachIndex != INDEX_NONE && !bNetUpdateAttachment)
            {
                Parent->AttachChildren.Insert(this, LastAttachIndex);
            }
            else
            {
                Parent->AttachChildren.Add(this);
            }

            Parent->ModifiedAttachChildren();

            // haker: we skip AddToCluster(), it is related optimization for GC-clustering:
            // - GC has two phases:
            //   1. Mark
            //   2. Sweep
            // - Clustering helps to reduce the overhead of marking
            AddToCluster(Parent, true);

            // haker: as I said, ClientAttachedChildren is related to replication:
            // - it works like stamping point (or snaphot) of client's AttachChildren status
            // - when server replicates its AttachChilren to client, the client-side compares ClientAttachChildren
            //   - newly updated AttachChildren, call AttachToComponent/DetachFromComponent properly to catch up server's AttachChildren status
            if (Parent->IsNetSimulating() && !IsNetSimulating())
            {
                Parent->ClientAttachedChildren.AddUnique(this);
            }

            // now apply attachment rules
            // haker: personally, SocketTransform and RelativeTM are used for update this's Relative[Location|Rotation|Scale] for EAttachmentRule::KeepWorld
            // - GetSocketTransform() is not cheap when we covered this function
            // - calculate SocketTransform and RelativeTM only when AttachmentRules.LocationRule is "KeepWorld", which is better in terms of performance
            FTransform SocketTransform = GetAttachParent()->GetSocketTransform(GetAttachSocketName());

            // haker: GetRelativeTransform is A/B
            // - A: GetComponentTransform() == ComponentToWorld
            // - B: SocketTransform
            // in EAttachmentRule::KeepWorld
            // - A: RelativeTransform x BoneSpace x ComponentSpace(world)
            // - B: BoneSpace x ComponentSpace(World)
            // - A/B = RelativeTransform
            // 
            // - Diagram:
            //                                                                                                                                                             
            //                                                                                                                                                             
            //           A                                                                    A                                                                            
            //          ┌──────┐                                                             ┌──────┐                                                                      
            //          │      │                                                             │      │                                                                      
            //          │  x   │                                                             │  x   │       Y                                                              
            //          │      │                                                             │      │        ▲                                                             
            //          └──────┘     B                 Apply Inv(BoneTransform(B))           └──────┘     B  │                                                             
            //    Y                  ┌──────┐                                                             ┌──┼───┐                                                         
            //     ▲                 │      │          ──────────────────────►                            │  │   │                                                         
            //     │                 │  x   │                                                             │  x───┼──►X                                                     
            //     │                 │      │                                                             │      │                                                         
            //     │                 └──────┘                                                             └──────┘                                                         
            //     └──────►X                                                                                                                                               
            //                                                                               A: ComponentToWorld x Inv(BoneTransform(B))                                   
            //     'x' is the origin (or pivot)                                              B: Identity Matrix = BoneTransform(B) x Inv(BoneTransform(B))                 
            //                                                                                                                                                             
            //                                                                               *** A: ComponentToWorld x Inv(BoneTransform(B)) == RelativeTransform(A) on B  
            //                                                                                   
            //                                                                                                                                                             
                                                                                                                                                       
            FTransform RelativeTM = GetComponentTransform().GetRelativeTransform(SocketTransform);

            switch (AttachmentRules.LocationRule)
            {
            case EAttachmentRule::KeepRelative:
                // dont do anything, keep relative position the same
                break;
            case EAttachmentRule::KeepWorld:
                if (IsUsingAbsoluteLocation())
                {
                    SetRelativeLocation_Direct(GetComponentTransform().GetTranslation());
                }
                else
                {
                    SetRelativeLocation_Direct(RelativeTM.GetTranslation());
                }
                break;
            case EAttachmentRule::SnapToTarget:
                // haker: attach bone(or component) without any Relative Offset
                SetRelativeLocation_Direct(FVector::ZeroVector);
                break;
            }

            switch (AttachmentRules.RotationRule)
            {
            case EAttachmentRule::KeepRelative:
                // dont do anything, keep relative rotation the same
                break;
            case EAttachmentRule::KeepWorld:
                if (IsUsingAbsoluteRotation())
                {
                    SetRelativeRotation_Direct(GetComponentRotation());
                }
                else
                {
                    SetRelativeRotation_Direct(RelativeRotationCache.QuatToRotator(RelativeTM.GetRotation()));
                }
                break;
            case EAttachmentRule::SnapToTarget:
                SetRelativeRotation_Direct(FRotator::ZeroRotator);
                break;
            }

            switch (AttachmentRules.ScaleRule)
            {
            case EAttachmentRule::KeepRelative:
                // dont do anything, keep relative scale the same
                break;
            case EAttachmentRule::KeepWorld:
                if (IsUsingAbsoluteScale())
                {
                    SetRelativeScale3D_Direct(GetComponentTransform().GetScale3D());
                }
                else
                {
                    SetRelativeScale3D_Direct(RelativeTM.GetScale3D());
                }
                break;
            case EAttachmentRule::SnapToTarget:
                SetRelativeScale3D_Direct(FVector(1.0f, 1.0f, 1.0f));
                break;
            }

            GetAttachParent()->OnChildAttached(this);

            // haker: apply newly AttachParent to its children, updating ComponentToWorld, Overlaps, and its Bound
            UpdateComponentToWorld(EUpdateTransformFlags::None, ETeleportType::TeleportPhysics);

            if (AttachmentRules.bWeldSimulatedBodies)
            {
                if (UPrimitiveComponent* PrimitiveComponent = Cast<UPrimitiveComponent>(this))
                {
                    if (FBodyInstance* BI = PrimitiveComponent->GetBodyInstance())
                    {
                        // haker: see UPrimitiveComponent::WeldToImplementation (goto 020: AttachToComponent)
                        PrimitiveComponent->WeldToImplementation(GetAttachParent(), GetAttachSocketName(), AttachmentRules.bWeldSimulatedBodies);
                    }
                }
            }

            // update overlaps, in case location changed or overlap state depends on attachment
            // haker: if the component is registered, it means that after World's BeginPlay(), at runtime, the component is attached to
            // - in this case, we need to update overlap states
            if (IsRegistered())
            {
                UpdateOverlaps();
            }

            return true;
        }

        return false;
    }

    /** returns whether the specified body is currently using physics simulation */
    virtual bool IsSimulatingPhysics() const
    {
        return false;
    }

    // 001 - Foundation - AttachToComponent - USceneComponent::OnRegister
    virtual void OnRegister() override
    {
        // if we need to perform a call to AttachTo, do that now
        // at this point scene component still has no any state (rendering, physics)
        // so this call will just add this component to an AttachChildren array of a Parent component

        // haker:
        // - as you remember, registering states(rendering or physics) are done in UActorComponent::OnRegister() which is not executed yet (see last line of the function)
        // - before registering states, we update our component to the Actor's scene graph
        if (GetAttachParent())
        {
            // see USceneComponent::AttachToComponent (goto 002: AttachToComponent)
            if (AttachToComponent(GetAttachParent(), FAttachmentTransformRules::KeepRelativeTransform, GetAttachSocketName()) == false)
            {
                // failed to attach, we need to clear AttachParent so we don't think we're actually attached when we're not
                //...
            }
        }

        // cache the level collection that contains the level in which this component is registered for fast access in IsVisible()
        const UWorld* const World = GetWorld();
        if (World)
        {
            const ULevel* const CachedLevel = GetComponentLevel();
            CachedLevelCollection = CachedLevel ? CachedLevel->GetCachedLevelCollection() : nullptr;
        }

        // haker: now we are going to register states to rendering and physics world with newly-updated scene graph
        UActorComponent::OnRegister();
    }

    void NotifyIsRootComponentChanged(bool bIsRootComponent) { IsRootComponentChanged.Broadcast(this, bIsRootComponent); }

    // 018 - Foundation - CreateWorld ** - USceneComponent's member variables
    // haker: with AttachParent and AttachChildren, it supports tree-structure for scene-graph 

    /** how often this component is allowed to move, used to make various optimizations: only safe to set in constructor */
    EComponentMobility::Type Mobility;

    /** physics volume in which this SceneComponent is located */
    TWeakObjectPtr<class APhysicsVolume> PhysicsVolume;

    /** what we are currently attached to. if valid, RelativeLocation etc. are used relative to this object */
    UPROPERTY(ReplicatedUsing=OnRep_AttachParent)
    TObjectPtr<USceneComponent> AttachParent;

    /** list of child SceneComponents that are attached to us. */
    UPROPERTY(ReplicatedUsing = OnRep_AttachChildren, Transient)
    TArray<TObjectPtr<USceneComponent>> AttachChildren;

    /** set of attached SceneComponents that were attached by the client so we can fix up AttachChildren when it is replicated to us */
    TArray<TObjectPtr<USceneComponent>> ClientAttachedChildren;

    /** current bounds of the component */
    FBoxSphereBounds Bounds;

    /** delegate called when this component is moved */
    FTransformUpdated TransformUpdated;

    /** current transform of the component, relative to the world */
    FTransform ComponentToWorld;

    /** location/rotation/scale of the component relative to its parent */
    FVector RelativeLocation;
    FRotator RelativeRotation;
    FVector RelativeScale3D;

    /** optional socket name on AttachParent that we are attached to */
    FName AttachSocketName;

    /** delegate that will be called when PhysicsVolume has been changed */
    FPhysicsVolumeChanged PhysicsVolumeChangedDelegate;

    /** stack of current movement scopes */
    TArray<FScopedMovementUpdate*> ScopedMovementStack;

    /** indicates if this ActorComponent is currently registered with a scene */
    uint8 bRegistered : 1;

    /** if the render state is currently created for this component */
    uint8 bRenderStateCreated : 1;

    /** true if we have ever updated ComponentToWorld based on RelativeLocation/Rotation/Scale; used at startup to make sure it is initialized */
    uint8 bComponentToWorldUpdated : 1;

    /**
     * if true, it indicates we don't need to call UpdateOverlaps
     * this is an optimization to avoid tree traversal when no attached components require UpdateOverlaps to be called
     * this should only be set to true as a result of UpdateOverlaps
     * To dirty this flag see ClearSkipUpdateOverlaps() which is expected when state affecting UpdateOverlaps changes (attachment, collision settings, etc...)
     */
    uint8 bSkipUpdateOverlaps : 1;

    /**
     * if true, this component uses its parents bounds when attached
     * this can be a significant optimization with many components attached together
     */
    uint8 bUseAttachParentBound : 1;

    /** if true, OnUpdateTransform virtual will be called each time this component is moved */
    uint8 bWantsOnUpdateTransform : 1;

    /** whether or not the cached PhysicsVolume this component overlaps should be updated when the component is moved */
    uint8 bShouldUpdatePhysicsVolume : 1;

    /** transient flag that temporarily disables UpdateOverlaps within DetachFromParent() */
    uint8 bDisableDetachmentUpdateOverlaps : 1;

    /** whether to hide the primitive in game, if the primitive is visible */
    uint8 bHiddenInGame : 1;

    /** whether to completely draw the primitive; if flase, the primitive is not drawn, does not cast a shadow */
    uint8 bVisible : 1;

    /** delegate invoked when this scene component becomes the actor's root component or when it no longer is */
    FIsRootComponentChanged IsRootComponentChanged;
};