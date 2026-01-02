#include "SceneComponent.h"

/**
 * predicate for comparing FOverlapInfos when exact weak object pointer index/serial numbers should match, assuming one is not null and not invalid
 * compare to operator== for WeakObjectPtr which does both HasSameIndexAndSerialNumber *and* IsValid() checks on both pointers 
 */
// 017 - Foundation - UpdateOverlaps - FFastOverlapInfoCompare
// haker: see operator overloading()
struct FFastOverlapInfoCompare
{
    FFastOverlapInfoCompare(const FOverlapInfo& BaseInfo)
        : MyBaseInfo(BaseInfo)
    {}

    bool operator()(const FOverlapInfo& Info)
    {
        // haker: we can understand why the predicate class name is FastOverlapInfoCompare:
        // - it only compares 
        // 1) Component's InternalIndex and SerialNumber
        //    - what is InternalIndex and SerialNumber?
        //      - we are going to cover GC yet, so conceptually explain what is InternalIndex and SerialNumber:
        //                                                                                                 
        //                                              NewObject<UObject>:StaticAllocateObject()  
        //                                               assign new slot in GObjObjects            
        //                                 ┌─────────────────────────────────────────┐             
        //                                 │                                         │             
        //     GObjObjects(in UE3)         │                                         │             
        //       ┌─────────┬─────────┬─────▼───┬─────────┐                    ┌──────┴──────┐      
        //       │         │         │         │         │                    │             │      
        //       │ Object1 │ Object2 │ Object3 │ Object4 │                    │ New UObject │      
        //       │         │         │         │         │                    │             │      
        //       └─────────┴─────────┴─────────┴─────────┘                    └─────────────┘      
        //                                                                                         
        //        Index :0  Index :1  Index :2  Index :3                                           
        //        Serial:2  Serial:4  Serial:0  Serial:1                                           
        //           ▲                                                                             
        //           │                                                                             
        //           │                                                                             
        //           │                                                                             
        //           │                                                                             
        //           │                                                                             
        //  You can think of Serial as Generation:                                                 
        //   '2' means that this slot is assigned twice for UObjects                               
        //                                                                                         
        // 2) BodyIndex (Item: extra data)
        return MyBaseInfo.OverlapInfo.Component.HasSameIndexAndSerialNumber(Info.OverlapInfo.Component)
            && MyBaseInfo.GetBodyIndex() == Info.GetBodyIndex();
    }

    bool operator()(const FOverlapInfo* Info)
    {
        return MyBaseInfo.OverlapInfo.Component.HasSameIndexAndSerialNumber(Info->OverlapInfo.Component)
            && MyBaseInfo.GetBodyIndex() == Info->GetBodyIndex();
    }

    const FOverlapInfo& MyBaseInfo;
};

/** helper for finding the index of an FOverlapInfo in an Array using the FFastOverlapInfoCompare predicate, knowing that at least one overlap is valid (no-null) */
// 016 - Foundation - UpdateOverlaps - IndexOfOverlapFast
template <class AllocatorType>
int32 IndexOfOverlapFast(const TArray<FOverlapInfo, AllocatorType>& OverlapArray, const FOverlapInfo& SearchItem)
{
    // see FFastOverlapInfoCompare (goto 017: UpdateOverlaps)
    return OverlapArray.IndexOfPredicate(FFastOverlapInfoCompare(SearchItem));
}

/** helper to see if two components can possibly generate overlaps with each other */
// 018 - Foundation - UpdateOverlaps - CanComponentsGenerateOverlap
static bool CanComponentsGenerateOverlap(const UPrimitiveComponent* MyComponent, UPrimitiveComponent* OtherComp)
{
    return OtherComp
        && OtherComp->GetGenerateOverlapEvents()
        && MyComponent
        && MyComponent->GetGenerateOverlapEvents()
        // haker: both components should match bGenerateOverlapEvents as well as its response as ECR_Overlap
        && MyComponent->GetCollisionResponseToComponent(OtherComp) == ECR_Overlap;
}

// 019 - Foundation - UpdateOverlaps - AreActorsOverlapping
bool AreActorsOverlapping(const AActor& A, const AActor& B)
{
    // due to the implementation of IsOverlappingActor() that scans and queries all owned primitive components and their overlaps
    // we guess that it's cheaper to scan the shorter of the lists
    // haker: computation efficiency, we select smaller actor to call IsOverlappingActor
    // see AActor::IsOverlappingActor (goto 020: UpdateOverlaps)
    if (A.GetComponents().Num() <= B.GetComponents().Num())
    {
        return A.IsOverlappingActor(&B);
    }
    else
    {
        return B.IsOverlappingActor(&A);
    }
}

template <class AllocatorType>
void AddUniqueOverlapFast(TArray<FOverlapInfo, AllocatorType>& OverlapArray, FOverlapInfo&& NewOverlap)
{
    if (IndexOfOverlapFast(OverlapArray, NewOverlap) == INDEX_NONE)
    {
        OverlapArray.Add(NewOverlap);
    }
}

/** used to determine if it is ok to call a notification on this object */
bool IsActorValidToNotify(AActor* Actor)
{
    return IsValid(Actor) && !Actor->GetClass()->HasAnyClassFlags(CLASS_NewerVersionExists);
}

template <class ElementType, class AllocatorTyp1, typename PredicateT>
static void GetPointersToArrayDataByPredicate(TArray<const ElementType*, AllocatorType1>& Pointers, const TArrayView<const ElementType>& DataArray, PredicateT Predicate)
{
    Pointers.Reserve(Pointers.Num() + DataArray.Num());
    for (const ElementType& Item : DataArray)
    {
        if (Invoke(Predicate, Item))
        {
            Pointers.Add(&Item);
        }
    }
}

template <class ElementType, class AllocatorType1>
static void GetPointersToArrayData(TArray<const ElementType*, AllocatorType1>& Pointers, const TArrayView<const ElementType>& DataArray)
{
	const int32 NumItems = DataArray.Num();
	Pointers.SetNumUninitialized(NumItems);
	for (int32 i=0; i < NumItems; i++)
	{
		Pointers[i] = &(DataArray[i]);
	}
}

/** predicate to identify components from overlaps array that can overlap */
struct FPredicateFilterCanOverlap
{
    FPredicateFilterCanOverlap(const UPrimitiveComponent& OwningComponent)
        : MyComponent(OwningComponent)
    {}

    bool operator()(const FOverlapInfo& Info) const
    {
        return CanComponentsGenerateOverlap(&MyComponent, Info.OverlapInfo.GetComponent());
    }

    const UPrimitiveComponent& MyComponent;
};

/** predicate to determine if an overlap is *NOT* witha certain AActor */
struct FPredicateOverlapHasDifferentActor
{
    FPredicateOverlapHasDifferentActor(const AActor& Owner)
        : MyOwner(&Owner)
    {}

    bool operator()(const FOverlapInfo& Info)
    {
        // MyOwnerPtr is always valid, so we don't need the IsValid() checks in the WeakObjectPtr comparison operator
        return !MyOwnerPtr.HasSameIndexAndSerialNumber(Info.OverlapInfo.HitObjectHandle.FetchActor());
    }

    const TWeakObjectPtr<const AActor> MyOwnerPtr;
};

// 025 - Foundation - UpdateOverlaps - ShouldIgnoreOverlapResult
static bool ShouldIgnoreOverlapResult(const UWorld* World, const AActor* ThisActor, const UPrimitiveComponent& ThisComponent, const AActor* OtherActor, const UPrimitiveComponent& OtherComponent, bool bCheckOverlapFlags)
{
    // don't overlap with self
    if (&ThisComponent == &OtherComponent)
    {
        return true;
    }

    if (bCheckOverlapFlags)
    {
        // both components must set GetGenerateOverlapEvents()
        if (!ThisComponent.GetGenerateOverlapEvents() || !OtherComponent.GetGenerateOverlapEvents())
        {
            return true;
        }
    }

    if (!ThisActor || !OtherActor)
    {
        return true;
    }

    if (!World || OtherActor == World->GetWorldSettings() || !OtherActor->IsActorInitialized())
    {
        return true;
    }

    return false;
}

// 017 - Foundation - AttachToComponent - GetRootWelded
UPrimitiveComponent* GetRootWelded(const UPrimitiveComponent* PrimComponent, FName ParentSocketName=NAME_None, FName* OutSocketName=NULL, bool bAboutToWeld=false)
{
    UPrimitiveComponent* Result = NULL;

    // we must find the root component along hierarchy that has bWelded set to true
    UPrimitiveComponent* RootComponent = Cast<UPrimitiveComponent>(PrimComponent->GetAttachParent());

    // check that body itself is welded
    // haker: if we pass bGetWelded as 'false', it will return component's BodyInstance
    // - if component's BI(BodyInstance) is not ready for welded, return NULL
    if (FBodyInstance* BI = PrimComponent->GetBodyInstance(ParentSocketName, false))
    {
        // we're not welded and we aren't trying to become welded
        if (bAboutToWeld == false 
            && BI->WeldParent == nullptr 
            && BI->bAutoWeld == false)
        {
            return NULL;
        }
    }

    FName PrevSocketName = ParentSocketName;
    FName SocketName = NAME_None; // because of skeletal mesh it's important that we check along the bones that we attached
    FBodyInstance* RootBI = NULL;

    // haker: by iterating scene-graph of SceneComponents, looking into whether WeldParent is valid or not
    for (; RootComponent; RootComponent = Cast<UPrimitiveComponent>(RootComponent->GetAttachParent()))
    {
        Result = RootComponent;
        SocketName = PrevSocketName;
        PrevSocketName = RootComponent->GetAttachSocketName();

        RootBI = RootComponent->GetBodyInstance(SocketName, false);
        if (RootBI && RootBI->WeldParent != nullptr)
        {
            continue;
        }

        break;
    }

    if (OutSocketName)
    {
        *OutSocketName = SocketName;
    }

    return Result;
}

/**
 * this filter allows us to refine queries (channel, object) with an additional level of ignore by tagging entire classes of objects (e.g. "red team", "blud team")
 * if (QueryIgnoreMask & ShapeFilter != 0) filter-out 
 */
typedef uint8 FMaskFilter;

class FRegisterComponentContext
{
    FRegisterComponentContext(UWorld* InWorld)
        : World(InWorld)
    {}

    void AddPrimitive(UPrimitiveComponent* PrimitiveComponent)
    {
        AddPrimitiveBatches.Add(PrimitiveComponent);
    }

    UWorld* World;
    TArray<UPrimitiveComponent*> AddPrimitiveBatches;
};

/**
 * class used to identify UPrimitiveComponents on the rendering thread which having to pass the pointer around,
 * which would make it easy for people to access game thread variables from the rendering thread 
 */
class FPrimitiveComponentId
{
    uint32 PrimIDValue;
};

// 0 is reserved to mean invalid
FThreadSafeCounter UPrimitiveComponent::NextComponentId;

/**
 * PrimitiveComponents are SceneComponents that contain or generate some sort of geometry, generally to be rendered or used as collision data
 * There are several subclasses for the various types of geometry, but the most common by far are the ShapeComponents (Capsule, Sphere, Box), StaticMeshComponent, and SkeletalMeshComponent
 * ShapeComponents generate geometry that is used for collision detection but are not rendered, while StaticMeshComponents and SkeletalMeshComponents contain pre-built geometry that is rendered, but can also be used for collision detection
 */
class UPrimitiveComponent : public USceneComponent
{
    // 009 - Foundation - CreateRenderState * - UPrimitiveComponent's constructor
    UPrimitiveComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get())
        : Super(ObjectInitializer)
    {
        //...

        // haker: see FPrimitiveComponentId briefly
        // - using NextComponentId, we assign unique id to PrimitiveComponent
        // - note that ComponentId is persistent until PrimitiveComponent is going to destroy
        ComponentId.PrimIDValue = NextComponentId.Increment();
    }

    FMatrix GetRenderMatrix() const
    {
        return GetComponentTransform().ToMatrixWithScale();
    }

    /** returns the ActorPosition for use by rendering: this comes from walking the attachment chain to the top-level actor */
    // 009 - Foundation - SceneRenderer * - UPrimitiveComponent::GetActorPositionForRenderer                                                                       
    FVector GetActorPositionForRenderer() const
    {
        // haker: retrieve the most ancestor in scene-graph:
        // - try to understand what Top value is expected with the diagram:
        //                                                                                   
        //  World                                                                            
        //   │                                                                               
        //   ├──Actor0                                                                       
        //   │   │                                                                           
        //   │   └──RootComponent0 ◄────── Where 'Top' will be retrieved                     
        //   │       │                     ─────────────────────────────                     
        //   │       └──RootComponent1            ****                                       
        //   │           │                                                                   
        //   │           └──RootComponent2 ◄────── GetActorPositionForRenderer() is called   
        //   │                                                                               
        //   │                                                                               
        //   ├──Actor1                                                                       
        //   │   │                                                                           
        //   │   └──RootComponent1                                                           
        //   │                                                                               
        //   └──Actor2                                                                       
        //       │                                                                           
        //       └──RootComponent2                                                           
        //            
        const USceneComponent* Top;
        for (Top = this; Top->GetAttachParent() && !Top->GetAttachParent()->bIsNotRenderAttachmentRoot; Top = Top->GetAttachParent());

        // haker: we return owner(Actor)'s location
        return (Top->GetOwner() != nullptr) ? Top->GetOwner()->GetActorLocation() : FVector(ForceInitToZero);
    }

    /** looking at various values of the component, determines if this component should be added to the scene */
    bool ShouldComponentAddToScene() const
    {
        bool bSceneAdd = true;

        // see USceneComponent::ShouldRender (goto 003: CreateRenderState)
        return bSceneAdd && ShouldRender();
    }

    // 007 - Foundation - SceneRenderer * - UPrimitiveComponent::SendRenderTransform_Concurrent
    virtual void SendRenderTransform_Concurrent() override
    {
        UpdateBounds();

        // if the primitive isn't hidden update its transform
        if (ShouldRender())
        {
            // update the scene info's transform for this primitive
            // haker: see FScene::UpdatePrimitiveTransform (goto 008: SceneRenderer)
            GetWorld()->Scene->UpdatePrimitiveTransform(this);
        }

        UActorComponent::SendRenderTransform_Concurrent();
    }

    // 002 - Foundation - CreateRenderState * - UPrimitiveComponent::CreateRenderState_Concurrent
    virtual void CreateRenderState_Concurrent(FRegisterComponentContext* Context) override
    {
        // haker: see FRegisterComponentContext briefly:
        // - for our case, Context is usually 'nullptr'
        // - FRegisterComponentContext is used to batch PrimitiveComponents to create renderstates all at once

        UActorComponent::CreateRenderState_Concurrent();

        UpdateBounds();

        // if the primitive isn't hidden and the detail mode setting allows it, add it to the scene
        // haker: see ShouldComponentAddToScene()
        if (ShouldComponentAddToScene())
        {
            // haker: we currently don't have the context:
            // - see FRegisterComponentContext::AddPrimitive() briefly and understand how it is batched up
            if (Context != nullptr)
            {
                Context->AddPrimitive(this);
            }
            else
            {
                // haker: we try to create render state in FScene(render-world)
                // see FScene::AddPrimitive (goto 004: CreateRenderState)
                GetWorld()->Scene->AddPrimitive(this);
            }
        }

        //...
    }

    FCollisionShape GetCollisionShape(float Inflation) const
    {
        // this is intended to be overriden by shape classes, so this is a simple, large bounding shape
        FVector Extent = Bounds.BoxExtent + Inflation;
        if (Inflation < 0.f)
        {
            // don't shrink below zero size
            Extent = Extent.ComponentMax(FVector::ZeroVector);
        }
        return FCollisionShape::MakeBox(Extent);
    }

    /** dispatch notifications for the given HitResult */
    void DispatchBlockingHit(AActor& OutOwner, FHitResult const& BlockingHit)
    {
        UPrimitiveComponent* const BlockingHitComponent = BlockingHit.Component.Get();
        if (BlockingHitComponent)
        {
            Owner.DispatchBlockingHit(this, BlockingHitComponent, true, BlockingHit);

            // dispatch above could kill the component, so we need to check that
            if (IsValid(BlockingHitComponent))
            {
                // BlockingHit.GetActor() could be marked for deletion in DispatchBlockingHit(), which would make the weak pointer return NULL
                if (AActor* const BlockingHitActor = BlockingHit.HitObjectHandle.GetManagingActor())
                {
                    BlockingHitActor->DispatchBlockingHit(BlockingHitComponent, this, false, BlockingHit);
                }
            }
        }
    }

    /**
     * if true, this component will generate overlap events when it is overlapping other components (e.g. BeginOverlap)
     * both components (this and the other) must have this enabled for overlap events to occur 
     */
    bool GetGenerateOverlapEvents() const
    {
        return bGenerateOverlapEvents;
    }

    /** return list of components this component is overlapping */
    const TArray<FOverlapInfo>& GetOverlapInfos() const
    {
        return OverlappingComponents;
    }

    /** check whether this component is overlapping any component of the given Actor */
    // 021 - Foundation - UpdateOverlaps - UPrimitiveComponent::IsOverlappingActor
    bool IsOverlappingActor(const AActor* Other) const
    {
        if (Other)
        {
            // haker: iterating OverlappingComponents, any component of overlapping components which has owner as Other exists
            // - if it is satisified, the 'Other' actor is already added as 'overlapped'
            // - in our case, we don't need to call overlap event for 'Other' actor which is already overlapped
            for (int32 OverlapIdx = 0; OverlapIdx < OverlappingComponents.Num(); ++OverlapIdx)
            {
                UPrimitiveComponent* const* const PrimComp = OverlappingComponents[OverlapIdx].OverlapInfo.Component.Get();
                if (PrimComp && (PrimComp->GetOwner() == Other))
                {
                    return true;
                }
            }
        }
        return false;
    }

    UFUNCTION(BlueprintImplementableEvent, meta=(DisplayName = "ActorBeginOverlap"), Category="Collision")
    void ReceiveActorBeginOverlap(AActor* OtherActor);

    /**
     * event when this actor overlaps another actor, for example a player walking into a trigger
     * for events when objects have a blocking collision, for example a player hitting a wall, see 'Hit' events
     * @note: Components on both this and the outer Actor must have bGenerateOverlapEvents set to true to generate overlap events 
     */
    virtual void NotifyActorBeginOverlap(AActor* OtherActor)
    {
        // call BP handler
        ReceiveActorBeginOverlap(OtherActor);
    }

    /** begin tracking an overlap interaction with the component specified */
    // 015 - Foundation - UpdateOverlaps - BeginComponentOverlap
    void BeginComponentOverlap(const FOverlapInfo& OtherOverlap, bool bDoNotifies)
    {
        // if pending kill, we should not generate any new overlaps
        if (!IsValid(this))
        {
            return;
        }

        // see IndexOfOverlapFast (goto 016: UpdateOverlaps)
        // haker: check whether OtherOverlap is in OverlappingComponents already
        const bool bComponentsAlreadyTouching = (IndexOfOverlapFast(OverlappingComponents, OtherOverlap) != INDEX_NONE);
        if (!bComponentsAlreadyTouching)
        {
            UPrimitiveComponent* OtherComp = OtherOverlap.OverlapInfo.Component.Get();
            // haker: see CanComponentsGenerateOverlap (goto 018: UpdateOverlap)
            if (CanComponentsGenerateOverlap(this, OtherComp))
            {
                GlobalOverlapEventsCounter++;

                // haker: get whether ThisComp and OtherComp share same Actor
                AActor* const OtherActor = OtherComp->GetOwner();
                AActor* const MyActor = GetOwner();
                const bool bSameActor = (MyActor == OtherActor);

                // haker: we are going to notify event as 'overlapped'
                // 1. if both components share same owner
                // 2. AreActorsOverapping() is failed:
                //    - what is meaning of AreActorsOverlapping?
                //      - see AreActorsOverlapping (goto 019)
                //                                                  ┌───────────────────────┐                      
                //                                                  │                       │                      
                //                                   ┌──────────────┼──────┐     Component3 ├───┐                  
                //   ┌─────────┐     ┌────────────┐  │              │ OLD  │                │   │                  
                //   │  Actor0 ◄─────┤ Component0 ◄──┤ Component1   └──────┼────────────────┘   │                  
                //   └─────────┘     └──────▲─────┘  │                     │                    │                  
                //                          │        └──▲──────────────────┘                    │                  
                //                          │           │                                       │   ┌──────────┐   
                //                          │         OverlappingComponents[Component3]         ├───►  Actor1  │   
                //                          │                                                   │   └──────────┘   
                //                          │        ┌─────────────────────┐                    │                  
                //                          │        │                     │                    │                  
                //                          └────────┤ Component2    ┌─────┼────────────────┐   │                  
                //                                   │               │ NEW!│                │   │                  
                //                                   └──▲────────────┼─────┘     Component4 ├───┘                  
                //                                      │            │                      │                      
                //                                      │            └──────────────────────┘                      
                //                                      │                                                          
                //                                    OverlappingComponents[]: still empty!                        
                //                                                                                                 
                //                                                                                                 
                //                       ***Actor0 and Actor1 is already overlapped by Component1 and Component3 
                //
                // - in summary, bNotifyActorTouch enables overlapped actor event only once not multiple times
                const bool bNotifyActorTouch = bDoNotifies && !bSameActor && !AreActorsOverlapping(*MyActor, *OtherActor);

                // perform reflexive touch
                // haker: see AddUniqueOverlapFast briefly
                OverlappingComponents.Add(OtherOverlap);
                AddUniqueOverlapFast(OtherComp->OverlappingComponents, FOverlapInfo(this, INDEX_NONE));

                // haker: we can trigger overlap events only world is begun-play
                const UWorld* World = GetWorld();
                if (bDoNotifies && (World && World->HasBegunPlay()))
                {
                    // haker: 1. component-wise begin-overlap event
                    {
                        // first execute component delegates
                        if (IsValid(this))
                        {
                            OnComponentBeginOverlap.Broadcast(this, OtherActor, OtherComp, OtherOverlap.GetBodyIndex(), OtherOverlap.bFromSweep, OtherOverlap.OverlayInfo);
                        }

                        if (IsValid(OtherComp))
                        {
                            // reverse normals for other component; when it's a sweep, we are the one that moved
                            OtherComp->OnComponentBeginOverlap.Broadcast(OtherComp, MyActor, this, INDEX_NONE, OtherOverlap.bFromSweep, OtherOverlap.bFromSweep ? FHitResult::GetReversedHit(OtherOverlap.OverlapInfo) : OtherOverlap.OverlapInfo);
                        }
                    }

                    // then execute actor notification if this is a new actor touch
                    // haker: 2. actor-wise begin-overlap event
                    if (bNotifyActorTouch)
                    {
                        // first actor virtuals
                        if (IsActorValidToNotify(MyActor))
                        {
                            MyActor->NotifyActorBeginOverlap(OtherActor);
                        }

                        if (IsActorValidToNotify(OtherActor))
                        {
                            OtherActor->NotifyActorBeginOverlap(MyActor);
                        }

                        // then level-script delegates
                        if (IsActorValidToNotify(MyActor))
                        {
                            MyActor->OnActorBeginOverlap.Broadcast(MyActor, OtherActor);
                        }

                        if (IsActorValidToNotify(OtherActor))
                        {
                            OtherActor->OnActorBeginOverlap.Broadcast(OtherActor, MyActor);
                        }
                    }
                }
            }
        }
    }

    /** set collision params on OutParams (such as CollisionResponse) to match the settings on this PrimitiveComponent */
    // 024 - Foundation - UpdateOverlaps - UPrimitiveComponent::InitSweepCollisionParams
    virtual void InitSweepCollisionParams(FCollisionQueryParams& OutParams, FCollisionResponseParams& OutResponseParam) const
    {
        // haker: even though we don't look into BodyInstance, we understand conceptually what channel in response is
        OutResponseParam.CollisionResponse = BodyInstance.GetResponseToChannels();
        OutParams.AddIgnoredActors(MoveIgnoreActors);
        OutParams.AddIgnoredComponents(MoveIgnoreComponents);
        OutParams.bTraceComplex = bTraceComplexOnMove;
        OutParams.bReturnPhysicalMaterial = bReturnMaterialOnMove;

        // haker: if you specify IgnoreMask as custom value, you can avoid collision response on same teams like red-tream or blue-team:
        // - we are not going to cover this in detail for now
        OutParams.IgnoreMask = GetMoveIgnoreMask();
    }

    /** finish tracking an overlap interaction that is no longer occuring between this component and the component specified */
    // 026 - Foundation - UpdateOverlaps - UPrimitiveComponent::EndComponentOverlap
    void EndComponentOverlap(const FOverlapInfo& OtherOverlap, bool bDoNotifies=true, bool bSkipNotifySelf=false)
    {
        UPrimitiveComponent* OtherComp = OtherOverlap.OverlapInfo.Component.Get();
        if (OtherComp == nullptr)
        {
            return;
        }

        // haker: remove self from OtherComp's OverlappingComponents
        // - by doing this, we can remove duplicate call on EndComponentOverlap events
        const int32 OtherOverlapIdx = IndexOfOverlapFast(OtherComp->OverlappingComponents, FOverlapInfo(this, INDEX_NONE));
        if (OtherOverlapIdx != INDEX_NONE)
        {
            OtherComp->OverlappingComponent.RemoveAtSwap(OtherOverlapIdx, 1, false);
        }

        // haker: we try to remove our OverlappingComponents
        const int32 OverlapIdx = IndexOfOverlapFast(OverlappingComponents, OtherOverlap);
        if (OverlapIdx != INDEX_NONE)
        {
            GlobalOverlapEventsCounter++;
            OverlappingComponents.RemoveAtSwap(OveralIdx, 1, false);

            AActor* const MyActor = GetOwner();
            const UWorld* World = GetWorld();
            if (bDoNotifies && ((World && World->HasBegunPlay())))
            {
                AActor* const OtherActor = OtherComp->GetOwner();
                if (OtherActor)
                {
                    if (!bSkipNotifySelf && IsValid(this))
                    {
                        OnComponentEndOverlap.Broadcast(this, OtherActor, OtherComp, OtherOverlap.GetBodyIndex());
                    }

                    if (IsValid(OtherComp))
                    {
                        OtherComp->OnComponentEndOverlap.Broadcast(OtherComp, MyActor, this, INDEX_NONE);
                    }

                    // if this was the last touch on the other actor by this actor, notify that we've untouched the actor as well
                    // haker: same as BeginComponentOverlap(), we don't want to call Actor delegate several times:
                    // - you should come up with that when last component which was overlapped is removed, the ActorEndOverlap will be called only one time!
                    const bool bSameActor = (MyActor == OtherActor);
                    if (MyActor && !bSameActor && !AreActorsOverlapping(*MyActor, *OtherActor))
                    {
                        if (IsActorValidToNotify(MyActor))
                        {
                            MyActor->NotifyActorEndOverlap(OtherActor);
                            MyActor->OnActorEndOverlap.Broadcast(MyActor, OtherActor);
                        }

                        if (IsActorValidToNotify(OtherActor))
                        {
                            OtherActor->NotifyActorEndOverlap(MyActor);
                            OtherActor->OnActorEndOverlap.Broadcast(OtherActor, MyActor);
                        }
                    }
                }
            }
        }
    }

    /** ends all current component overlaps. generally used when destroying this component or when it can no longer generate overlaps */
    // 027 - Foundation - UpdateOverlaps - UPrimitiveComponent::ClearComponentOverlaps
    void ClearComponentOverlaps(bool bDoNotifies, bool bSkipNotifySelf)
    {
        if (OverlappingComponents.Num() > 0)
        {
            // make a copy since EndComponentOverlap will remove items from OverlappingComponents
            const TInlineOverlapInfoArray OverlapsCopy(OverlappingComponents);
            for (const FOverlapInfo& OtherOverlap : OverlapsCopy)
            {
                EndComponentOverlap(OtherOverlap, bDoNotifies, bSkipNotifySelf);
            }
        }
    }

    /** update current physics volume for this component, if bShouldUpdatePhysicsVolume is true, override to use the overlaps to find the physics volume */
    // 028 - Foundation - UpdateOverlaps - UPrimitiveComponent::UpdatePhysicsVolume
    virtual void UpdatePhysicsVolume(bool bTriggerNotifiers) override
    {
        if (GetShouldUpdatePhysicsVolume() && IsValid(this))
        {
            if (UWorld* MyWorld = GetWorld())
            {
                if (MyWorld->GetNonDefaultPhysicsVolumeCount() == 0)
                {
                    SetPhysicsVolume(MyWorld->GetDefaultPhysicsVolume(), bTriggerNotifiers);
                }
                else if (GetGenerateOverlapEvents() && IsQueryDefaultPhysicsVolume())
                {
                    APhysicsVolume* BestVolume = MyWorld->GetDefaultPhysicsVolume();
                    int32 BestPriority = BestVolume->Priority;

                    // haker: see? we just iterating OverlappingComponents
                    for (auto CompIt = OverlappingComponents.CreateIterator(); CompIt; ++CompIt)
                    {
                        const FOverlapInfo& Overlap = *CompIt;
                        UPrimitiveComponent* OtherComponent = Overlap.OverlapInfo.Component.Get();
                        if (OtherComponent && OtherComponent->GetGenerateOverlapEvents())
                        {
                            // haker: just get the owner Actor and compare Physics Volume's priority
                            APhysicsVolume* V = Cast<APhysicsVolume>(OtherComponent->GetOwner());
                            if (V && V->Priority > BestPriority)
                            {
                                if (V->IsOverlapInVolume(*this))
                                {
                                    BestPriority = V->Priority;
                                    BestVolume = V;
                                }
                            }
                        }
                    }

                    SetPhysicsVolume(BestVolume, bTriggerNotifiers);
                }
                else
                {
                    USceneComponent::UpdatePhysicsVolume(bTriggerNotifiers);
                }
            }
        }
    }

    /** queries world and updates overlap tracking state for this component */
    // 014 - Foundation - UpdateOverlaps - UPrimitiveComponent::UpdateOverlapsImpl
    // haker: PrimitiveComponent has OverlappingComponents (state of overlaps)
    virtual bool UpdateOverlapsImpl(const TOverlapArrayView* NewPendingOverlaps=nullptr, bool bDoNotifies=true, const TOverlapArrayView* OverlapsAtEndLocation=nullptr) override
    {
        // if we haven't begun play, we're still setting things up (e.g. we might be inside one of the construction scripts)
        // so we don't want to generate overlaps yet. there is no need to update children yet either, they will update once we are allowed to as well
        // haker: for example, while simple/custom construction script is running, UpdateOverlaps is called:
        // - in this case, we are NOT executing UpdateOverlapsImpl, because we don't want:
        //   - it is still NOT BeginPlay() yet
        // - you can think that UpdateOverlaps() is intended to run after World/Actor begins playing
        const AActor* const MyActor = GetOwner();
        if (MyActor && !MyActor->HasActorBegunPlay() && !MyActor->IsActorBeginningPlay())
        {
            return false;
        }

        // haker: let's understand UPrimitiveComponent::UpdateOverlapsImpl in diagrams (a little bit complicated)
        // - However, with overall picture, you can understand it and easily remember how it works:
        //                                                                                                                                       
        //     1. NewPendingOverlaps: arbitrary overlaps inputs:                                                               
        //        ┌────────┐    ┌────────┐    ┌────────┐                                                                       
        //        │Overlap0├────┤Overlap1├────┤Overlap2│                                                                       
        //        └────────┘    └────────┘    └────────┘                                                                       
        //                                                                                                                     
        //         ───────────────────────────────────►                                                                        
        //       Iterating to call BeginComponentOverlap()                                                                     
        //                       │                                                                                             
        //                       │                                                                                             
        //                       ▼                                                                                             
        //       By calling BeginComponentOverlap(), [Overlap0, Overlap1, Overlap2] is inserted into 'OverlappingComponents'   
        //                                                                                                                     
        //                                                                                                                     
        //                                                                                                                     
        //     2. Two cases:                                                                                                   
        //         │                                                                                                           
        //         ├────Case1) When OverlapsAtEndLocation is provided:                                                         
        //         │    ┌────────┐    ┌────────┐    ┌────────┐                                                                 
        //         │    │Overlap2├────┤Overlap3├────┤Overlap4│                                                                 
        //         │    └───▲────┘    └────────┘    └────────┘                                                                 
        //         │        │                                                                                                  
        //         │        └─────Overlap2 is overlapped between NewPendingOverlaps and OverlapsAtEndLocation                  
        //         │                                                                                                           
        //         │                      │                                                                                    
        //         │                      │ Remove duplicates(Overlap2)                                                        
        //         │                      │ by FPredicateFilterCanOverlap                                                      
        //         │                      │                                                                                    
        //         │                      ▼                                                                                    
        //         │    ┌────────┐    ┌────────┐                                                                               
        //         │    │Overlap3├────┤Overlap4│                                                                               
        //         │    └────────┘    └────────┘                                                                               
        //         │                                                                                                           
        //         │    The filtered Overlaps are inserted into 'NewOverlappingComponentPtrs'                                  
        //         │                                                                                                           
        //         │                                                                                                           
        //         │                                                                                                           
        //         │                                                                                                           
        //         └────Case2) When no OverlapsAtEndLocation is provided:                                                      
        //               │                                                                                                     
        //               └───We query overlap intersections: ComponentOverlapMulti()                                           
        //                   ┌────────┐    ┌────────┐    ┌────────┐                                                            
        //                   │Overlap1├────┤Overlap3├────┤Overlap4│                                                            
        //                   └────────┘    └────────┘    └────────┘                                                            
        //                   Newly queried components are added to 'NewOverlappingComponentPtrs'                               
        //                                                                                                                     
        //                                                                                                                     
        //                                                                                                                     
        //     3.With 'OverlappingComponents' and 'NewOverlappingComponentPtrs', we filter them into BeginOverlap/EndOverlap   
        //       [we are simulating based on Case2)]                                                                           
        //        │                                                                                                            
        //        │                                     ┌────────┐    ┌────────┐    ┌────────┐                                 
        //        │              OverlappingComponents: │Overlap0├────┤Overlap1├────┤Overlap2│                                 
        //        │                                     └────────┘    └────────┘    └────────┘                                 
        //        │                                                                                                            
        //        │                                     ┌────────┐    ┌────────┐    ┌────────┐                                 
        //        │        NewOverlappingComponentPtrs: │Overlap1├────┤Overlap3├────┤Overlap4│                                 
        //        │                                     └────────┘    └────────┘    └────────┘                                 
        //        │                                                                                                            
        //        │                                                                                                            
        //        │        OldOverlappingComponentPtrs:◄───OverlappingComponents                                               
        //        │                                                                                                            
        //        │                                     ┌────────┐    ┌────────┐    ┌────────┐                                 
        //        │                                     │Overlap0├────┤Overlap1├────┤Overlap2│                                 
        //        │                                     └────────┘    └────────┘    └────────┘                                 
        //        │                                                                                                            
        //        │                                                                                                            
        //        ├────1. OverlappedComponents between NewOverlappingComponentPtrs and OldOverlappingComponentPtrs             
        //        │       ┌────────┐                                                                                           
        //        │       │Overlap0│                                                                                           
        //        │       └────────┘                                                                                           
        //        │       == In previous frame and current frame, Overlap0 is still overlapped                                 
        //        │           │                                                                                                
        //        │           └────We need to remove NewOverlappingComponentPtrs and OldOverlappingComponentPtrs               
        //        │                                                                                                            
        //        └────2. What Overlaps are left in NewOverlappingComponentPtrs and OldOverlappingComponentPtrs:               
        //                                                                                                                     
        //                                                                                                                     
        //                                               ┌────────┐    ┌────────┐                                              
        //                  NewOverlappingComponentPtrs: │Overlap3├────┤Overlap4│                                              
        //                    │                          └────────┘    └────────┘                                              
        //                    └───List of Overlaps to call BeginOverlap()                                                      
        //                                                                                                                     
        //                                                                                                                     
        //                                               ┌────────┐    ┌────────┐                                              
        //                  OldOverlappingComponentPtrs: │Overlap0├────┤Overlap2│                                              
        //                    │                          └────────┘    └────────┘                                              
        //                    └───List of Overlaps to call EndOverlap()                                                        
        //                                                                                                                     

        bool bCanSkipUpdateOverlaps = true;

        // first dispatch any pending overlaps
        // haker: we check whether PrimitiveComponent has enabled to generate overlap events
        if (GetGenerateOverlapEvents() && IsQueryCollisionEnabled())
        {
            bCanSkipUpdateOverlaps = false;
            if (MyActor)
            {
                const FTransform PrevTransform = GetComponentTransform();
                // if we are root component we ignore child components:
                // - those children will update their overlaps when we descend into the child tree
                // - this aids an optimization in MoveComponent
                // haker: bIgnoreChildren minor optimization to reduce the duplicate computation:
                // - as we saw in USceneComponent::UpdateOverlaps, we do recursive call UpdateOverlaps on AttachChildren
                const bool bIgnoreChildren = (MyActor->GetRootComponent() == this);

                // haker: this is [1.]
                if (NewPendingOverlaps)
                {
                    // NOTE: BeginComponentOverlap() only triggers overlaps where GetGenerateOverlapEvents() is true on both components
                    const int32 NumNewPendingOverlaps = NewPendingOverlaps->Num();
                    for (int32 Idx = 0; Idx < NumNewPendingOverlaps; ++Idx)
                    {
                        // haker: see BeginComponentOverlap (goto 015: UpdateOverlaps)
                        BeginComponentOverlap((*NewPendingOverlaps)[Idx], bDoNotifies);
                    }
                }

                // now generate full list of new touches, so we can compare to existing list and determine what changed
                // haker: here is [2.]:
                // - get NewOverlappingComponentPtrs depending on cases: Case 1) and Case 2) as we covered
                TInlineOverlapInfoArray OverlapMultiResult;
                TInlineOverlapPointerArray NewOverlappingComponentPtrs;

                // if pending kill, we should not generate any new overlaps; also not if overlaps were just disabled during BeginComponentOverlap
                if (IsValid(this) && GetGenerateOverlapEvents())
                {
                    // might be able to avoid testing for new overlaps at the end location
                    if (OverlapsAtEndLocation != nullptr && PrevTransform.Equals(GetComponentTransform()))
                    {
                        // haker: 1. Skipping overlap test
                        const bool bCheckForInvalid = (NewPendingOverlaps && NewPendingOverlaps->Num() > 0);
                        if (bCheckForInvalid)
                        {
                            // BeginComponentOverlap may have disabled what we thought were valid overlaps at the end (collision response or overlap flags could change)
                            GetPointersToArrayDataByPredicate(NewOverlappingComponentPtrs, *OverlapAtEndLocation, FPredicateFilterCanOverlap(*this));
                        }
                        else
                        {
                            GetPointersToArrayData(NewOverlappingComponentPtrs, *OverlapsAtEndLocation);
                        }
                    }
                    else
                    {
                        // haker: 2. Performing overlaps
                        UWorld* const MyWorld = GetWorld();
                        TArray<FOverlapResult> Overlaps;

                        // note this will optionally include overlaps with components in the same actor (depending on bIgnoreChildren)
                        FComponentQueryParams Params(SCENE_QUERY_STAT(UpdateOverlaps), bIgnoreChildren ? MyActor : nullptr);
                        Params.bIgnoreBlocks = true;

                        // haker: see FCollisionResponseParams (goto 022: UpdateOverlap)
                        // see InitSweepCollisionParams (goto 024: UpdateOverlap)
                        FCollisionResponseParams ResponseParam;
                        InitSweepCollisionParams(Params, ResponseParam);

                        // haker: we get the results in Overlaps: TArray<FOverlapResult>
                        ComponentOverlapMulti(Overlaps, MyWorld, GetComponentLocation(), GetComponentQuat(), GetCollisionObjectType(), Params);

                        // haker: iterating Overlaps, filter overlap result and filtered results are inserted into OverlapMultiResult --> NewOverlappingComponentPtrs
                        for (int32 ResultIdx = 0; ResultIdx < Overlaps.Num(); ResultIdx++)
                        {
                            const FOverlapResult& Result = Overlaps[ResultIdx];
                            UPrimitiveComponent* const HitComp = Result.Component.Get();
                            if (HitComp && (HitComp != this) && HitComp->GetGenerateOverlapEvents())
                            {
                                const bool bCheckOverlapFlags = false; // already checked above
                                // haker: see ShouldIgnoreOverlapResult (goto 025: UpdateOverlaps)
                                if (!ShouldIgnoreOverlapResult(MyWorld, MyActor, *this, Result.OverlapObjectHandle.FetchActor(), *HitComp, bCheckOverlapFlags))
                                {
                                    // don't need to add unique unless the overlap check can return dupes
                                    OverlapMultiResult.Emplace(HitComp, Result.ItemIndex);
                                }
                            }
                        }

                        // fill pointers to overlap results. we ensure below that OverlapMutliResult stays in scope so these pointers remain valid
                        GetPointersToArrayData(NewOverlappingComponentPtrs, OverlapMultiResult);
                    }
                }

                // if we have any overlaps from BeginComponentOverlap() (from now or in the past), see if anything has changed by filtering NewOverlappingComponents
                // haker: Case 3)
                if (OverlappingComponents.Num() > 0)
                {
                    // haker: put OverlappingComponents into OldOverlappingComponentPtrs to filter components to call end-overlap events
                    TInlineOverlapPointerArray OldOverlappingComponentPtrs;
                    if (bIgnoreChildren)
                    {
                        GetPointersToArrayDataByPredicate(OverlappingComponentPtrs, OverlappingComponents, FPredicateOverlapHasDifferentActor(*MyActor));
                    }
                    else
                    {
                        GetPointersToArrayData(OldOverlappingComponentPtrs, OverlappingComponents);
                    }

                    // now we want to compare the old and new overlap lists to determine:
                    // 1. what overlaps are in old and not in new (need end overlap notifies)
                    // 2. what overlaps are in new and not in old (need begin overlap notifies)
                    // we do this by removing common entries from both lists, since overlapping status has not changed for them
                    // what is left over will be what has changed
                    for (int32 CompIdx = 0; CompIdx < OldOverlappingComponentPtrs.Num() && NewOverlappingComponentPtrs.Num() > 0; ++CompIdx)
                    {
                        // RemoveAtSwap is ok, since it is not necessary to maintain order
                        // haker: when you call RemoveAtSwap with bAllowShrinking as 'false', it will not resize, so you can get performance benefits
                        const bool bAllowShrinking = false;

                        // haker: we serach old -> new!
                        const FOverlapInfo* SearchItem = OldOverlappingComponentPtrs[CompIdx];
                        const int32 NewElementIdx = IndexOfOverlapFast(NewOverlappingComponentPtrs, SearchItem);
                        if (NewElementIdx != INDEX_NONE)
                        {
                            NewOverlappingComponentPtrs.RemoveAtSwap(NewElementIdx, 1, bAllowShrinking);
                            OldOverlappingComponentPtrs.RemoveAtSwap(CompIdx, 1, bAllowShrinking);
                            --CompIdx;
                        }
                    }

                    // haker: first we call end-overlap events
                    const int32 NumOldOverlaps = OldOverlappingComponentPtrs.Num();
                    if (NumOldOverlaps)
                    {
                        // now we have to make a copy of the overlaps because we can't keep pointers to them, that list is about to be manipulated in EndComponentOverlap()
                        TInlineOverlapInfoArray OldOverlappingComponents;
                        OldOverlappingComponents.SetNumUninitialized(NumOldOverlaps);
                        for (int32 i = 0; i < NumOldOverlaps; ++i)
                        {
                            OldOverlappingComponents[i] = *(OldOverlappingComponentPtrs[i]);
                        }

                        // OldOverlappingComponents now contains only previous overlaps that can confirmed to no longer be valid
                        for (const FOverlapInfo& OtherOverlap : OldOverlappingComponents)
                        {
                            if (OtherOverlap.OverlapInfo.Component.IsValid())
                            {
                                // haker: see EndComponentOverlap (goto 026: UpdateOverlap)
                                EndComponentOverlap(OtherOverlap, bDoNotifies, false);
                            }
                            else
                            {
                                // remove stale item. reclaim memory only if it's getting large, to try to avoid churn but avoid bloating component's memory usage
                                // haker: if component is not valid, just remove entry from OverlappingComponents
                                const bool bAllowShrinking = (OverlappingComponents.Max() >= 24);
                                const int32 StaleElementIndex = IndexOfOverlapFast(OverlappingComponents, OtherOverlap);
                                if (StaleElementIndex != INDEX_NONE)
                                {
                                    OverlappingComponents.RemoveAtSwap(StaleElementIndex, 1, bAllowShrinking);
                                }
                            }
                        }
                    }
                }

                // ensure these arrays are still in scope, because we kept pointers to them in NewOverlappingComponentPtrs
                static_assert(sizeof(OverlapMultiResult) != 0, "Variable must be in this scope");
                static_assert(sizeof(*OverlapsAtEndLocation) != 0, "Variable must be in this scope");

                // NewOverlappingComponents now contains only new overlaps that didn't exist previously
                for (const FOverlapInfo* NewOverlap : NewOverlappingComponentPtrs)
                {
                    BeginComponentOverlap(*NewOverlap, bDoNotifies);
                }
            }
        }
        else
        {
            // GetGenerateOverlapEvents() is false or collision is disabled
            // End all overlaps that exist, in case GetGenerateOverlapEvents() was true last tick (i.e. was just turned off)
            // haker: if bGenerateOverlapEvents is disabled, clear OverlappingComponents by ClearComponentOverlaps
            if (OverlappingComponents.Num() > 0)
            {
                const bool bSkipNotifySelf = false;
                // haker: see ClearComponentOverlaps (goto 027: UpdateOverlaps)
                ClearComponentOverlaps(bDoNotifies, bSkipNotifySelf);
            }
        }

        // now update any children down the chain
        // since on overlap events could manipulate the child array we need to take a copy of it to avoid missing any children if one is removed from the middle
        // haker: similar to USceneComponent::UpdateOverlapsImpl
        TInlineComponentArray<USceneComponent*> AttachedChildren;
        AttachedChildren.Append(GetAttachChildren());

        for (USceneComponent* const ChildComp : AttachedChildren)
        {
            if (ChildComp)
            {
                // do not pass on OverlapsAtEndLocation, it only applied to this component
                bCanSkipUpdateOverlaps &= ChildComp->UpdateOverlaps(nullptr, bDoNotifies, nullptr);
            }
        }

        // update physics volume using most current overlaps
        if (GetShouldUpdatePhysicsVolume())
        {
            // haker: PrimitiveComponent has its own overlap state
            // - it gives more efficient way of checking PhysicsVolume compared to USceneComponent::UpdatePhysicsVolume
            // see UpdatePhysicsVolume (goto 028: UpdateOverlaps)
            UpdatePhysicsVolume(bDoNotifies);
            bCanSkipUpdateOverlaps = false;
        }

        return bCanSkipUpdateOverlaps;
    }

    /** returns BodyInstance of the component */
    // 012 - Foundation - AttachToComponent - UPrimitiveComponent::GetBodyInstance
    virtual FBodyInstance* GetBodyInstance(FName BoneName=NAME_None, bool bGetWelded=true, int32 Index=INDEX_NONE) const
    {
        // see FBodyInstance (goto 013: AttachToComponent)
        // haker: now we can understand what GetBodyInstance() is for:
        // - if it is welded, it redirect WeldParent instead of its own BodyInstance
        return const_cast<FBodyInstance*>((bGetWelded && BodyInstance.WeldParent) ? BodyInstance.WeldParent : &BodyInstance);
    }

    /** adds the bodies that are currently welded to the OutWeldedBodies array */
    // 018 - Foundation - AttachToComponent - UPrimitiveComponent::GetWeldedBodies
    virtual void GetWeldedBodies(TArray<FBodyInstance*>& OutWeldedBodies, TArray<FName>& OutLabels, bool bIncludingAutoWeld=false)
    {
        // haker: add current component's BodyInstance
        OutWeldedBodies.Add(&BodyInstance);
        OutLabels.Add(NAME_None);

        // haker: iterate AttachChildren and recursively call GetWeldedBodies:
        // - calloect all welded FBodyInstances in AttachChildren
        for (USceneComponent* Child : GetAttachChildren())
        {
            if (USceneComponent* PrimChild = Cast<UPrimitiveComponent>(Child))
            {
                // haker: once more, if we pass 'bGetWelded' as 'false', it returns component's BI
                if (FBodyInstance* BI = PrimChild->GetBodyInstance(NAME_None, false))
                {
                    // haker: check whether WeldParent exists
                    if (BI->WeldParent != nullptr || (bIncludingAutoWeld && BI->bAutoWeld))
                    {
                        PrimChild->GetWeldedBodies(OutWeldedBodies, OutLabels, bIncludingAutoWeld);
                    }
                }
            }
        }
    }

    /** unwelds this component from its parent component: Attachment is maintained (DetachFromParent automatically unwelds) */
    // 011 - Foundation - AttachToComponent - UPrimitiveComponent::UnWeldFromParent
    // haker: note that weld/unweld is independent from attach/detach:
    // - BUT, weld/unweld follows attach/detach cycle
    virtual void UnWeldFromParent()
    {
        // haker: before looking into UnweldFromParent(), let's understand the states of UActorComponent to physics-world and render-world
        // Diagram:
        //                                                                                                                                                              
        //                     OnRegister():ExecuteRegisterEvents()                                 ┌──Game-World──────────────────────┐                        
        //                     ┌───────────────────┐                                                │                                  │                        
        //          ───────────┤                   ├───────────────────►  Game Thread               │ ┌──────────┐      ┌──────────┐   │                        
        //                     └───┬──────┬────────┘                                            ┌───┼─┤Component0│      │Component1├───┼──────────────┐         
        //                         │      │                                                     │   │ └──────────┘      └──────────┘   │              │         
        //                         │      │                                                     │   │                                  │              │         
        //                         │      │                                                     │   └──────────────────────────────────┘              │         
        //  CreateRenderState_Concurrent()│                                                     │                                                     │         
        //                         │      │                                               Synced│   ┌──Render-World─────────────────────────────┐     │Synced   
        //                         │      │                                                     │   │                                           │     │         
        //                         │      │                                                     │   │ ┌───────────────────┐                     │     │         
        //                        ┌▼──────┼──────┐                                              ├───┼─►PrimitiveSceneInfo0│                     │     │         
        //          ──────────────┤       │      ├─────────────────────►  Render Thread         │   │ └───────────────────┘                     │     │         
        //                        └───────┼──────┘                                              │   │                                           │     │         
        //                                │                                                     │   │                     ┌───────────────────┐ │     │         
        //                                │                                                     │   │                     │PrimitiveSceneInfo1◄─┼─────┤         
        //                                │                                                     │   │                     └───────────────────┘ │     │         
        //                           CreatePhysicsState()                                       │   │                                           │     │         
        //                                │                                                     │   └───────────────────────────────────────────┘     │         
        //                                │                                                     │                                                     │         
        //                                │                                                     │   ┌──Physics-World────────────────────────────┐     │         
        //                          ┌─────▼──────┐                                              │   │                                           │     │         
        //          ────────────────┤            ├─────────────────────►  Physics Thread        │   │ ┌─────────────┐        ┌─────────────┐    │     │         
        //                          └────────────┘                                              └───┼─►BodyInstance0│        │BodyInstance1◄────┼─────┘         
        //                                                                                          │ └─────────────┘        └─────────────┘    │               
        //                                                                                          │                                           │               
        //                                                                                          │                                           │               
        //                                                                                          │                                           │               
        //                                                                                          └───────────────────────────────────────────┘               
        //                                                                                                                                            

        // see UPrimitiveComponent::GetBodyInstance (goto 012: AttachToComponent)
        // haker: note that we pass 'bGetWelded' as 'false'
        FBodyInstance* NewRootBI = GetBodyInstance(NAME_None, false);
        UWorld* CurrentWorld = GetWorld();
        // haker: if there is no WeldParent, it means that no weld occured previously
        if (NewRootBI == NULL 
            // haker: GetBodyInstance() returns primitive component's BodyInstance, in this case, we should check NewRootBI->WeldParent
            || NewRootBI->WeldParent == nullptr 
            || CurrentWorld == nullptr 
            || CurrentWorld->GetPhysicsScene() == nullptr 
            || !IsValidChecked(this) 
            || IsUnreachable())
        {
            return;
        }

        // see GetRootWelded (goto 017: AttachToComponent)
        FName SocketName;
        UPrimitiveComponent* RootComponent = GetRootWelded(this, GetAttachSocketName(), &SocketName);
        if (RootComponent)
        {
            // haker: note that 'RootComponent' is not the Actor's root component:
            // - the 'RootComponent' is the component who has root welded BodyInstance
            if (FBodyInstance* RootBI = RootComponent->GetBodyInstance(SocketName, false))
            {
                bool bRootIsBeingDeleted = !IsValidChecked(RootComponent) || RootComponent->IsUnreachable();
                const FBodyInstance* PrevWeldParent = NewRootBI->WeldParent;

                // haker: we unweld this component (NewRootBI)
                // - we are NOT going to see how Unweld() works:
                // - in conceptually, it is the opposite way of Weld()

                // haker: let's try to understand the overall process of unwelding:
                //                                                                                                                                                                                                 
                //      Component0:BodyInstance0                                                                Component0:BodyInstance0                                                                           
                //      ┌───────────┐                                                                           ┌───────────┐                                                                                      
                //      │           │                                                                           │           │                                                                                      
                //      └───────┬───┤                                                                           └───────┬───┤                                                                                      
                //              │   │    Component2:WeldParent=BodyInstance0                                            │   │    Component2:WeldParent=BodyInstance0                                               
                //              │   ├─────────┐                                                                         │   ├─────────┐                                                                            
                //              │   │         │                                                                         │   │         │                                                                            
                //              │   ├─────────┘                                  1.Unweld(Component1)                   │   ├─────────┘                                                                            
                //              │   │                                              : Just UnWeld(Component1)            │   │                                                                                      
                //              └───┘                                            ───────────────────────────►           └───┘                                                                                      
                //               Component1:WeldParent=BodyInstance0                                                     Component1:WeldParent=nullptr                                                             
                //                                                                                                                                                                                                 
                //                                                                                                                                                                                                 
                //                                                                                                                                                                                                 
                //             Component0:AttachChildren[Component1]                                                   Component0:AttachChildren[Component1]                                                       
                //                 ▲      WeldChildren[Component1,Component2]                                              ▲      WeldChildren[Component2]◄───────***Still Component2 is left                      
                //                 │                                                                                       │                                         as welded to Component0                       
                //             Component1:AttachChildren[Component2]                                                   Component1:AttachChildren[Component2]                                                       
                //                 ▲                                                                                       ▲      WeldChildren[]                                                                   
                //                 │                                                                                       │                                                                                       
                //             Component2                                                                              Component2:                                                                                 
                //                                                                                                                                                                                                 
                //                                                                                                                                                                                                 
                //                                                                                                           │                                                                                     
                //                                                                                                           │                                                                                     
                //                                                                                                           │ 2.Iterating Component1's AttachChildren:                                            
                //                                                                                                           │    │                                                                                
                //                                                                                                           │    └─ Unweld(Component2)                                                            
                //                                                                                                           │                                                                                     
                //                                                                                                           │                                                                                     
                //                                                                                                           │                                                                                     
                //                                                                                                           ▼                                                                                     
                //                                                                                                                                                                                                 
                //                                                                                                                                                                                                 
                //    Component0:BodyInstance0                                                                  Component0:BodyInstance0                                                                           
                //    ┌───────────┐                                                                             ┌───────────┐                                                                                      
                //    │           │                                                                             │           │                                                                                      
                //    └───────┬───┤                                                                             └───────┬───┤                                                                                      
                //            │   │    Component2:WeldParent=BodyInstance1                                              │   │    Component2:WeldParent=nullptr                                                     
                //            │   ├─────────┐                                  3.ApplyWeldOnChildren()                  │   ├─────────┐                                                                            
                //            │   │         │                                  :weld Component1's AttachChildren        │   │         │                                                                            
                //            │   ├─────────┘                                                                           │   ├─────────┘                                                                            
                //            │   │                                             ◄───────────────────────────            │   │                                                                                      
                //            └───┘                                                                                     └───┘                                                                                      
                //             Component1:WeldParent=nullptr                                                             Component1:WeldParent=nullptr                                                             
                //                                                                                                                                                                                                 
                //                                                                                                                                                                                                 
                //                                                                                                                                                                                                 
                //           Component0:AttachChildren[Component1]                                                     Component0:AttachChildren[Component1]                                                       
                //               ▲      WeldChildren[]                                                                     ▲      WeldChildren[]                                                                   
                //               │                                                                                         │                                                                                       
                //           Component1:AttachChildren[Component2]                                                     Component1:AttachChildren[Component2]◄───────***Component2 should be welded to Component1   
                //               ▲      WeldChildren[Component1]◄──────────***What we expected!!                           ▲      WeldChildren[]                                                                   
                //               │                                                                                         │                                                                                       
                //           Component2:                                                                               Component2:                                                                                 
                //                                                                                                                                                                                                 

                // haker: 1. Just Unweld from RootBI to NewRootBI(component's BI)
                RootBI->UnWeld(NewRootBI);
                FPlatformAtomics::InterlockedExchangePtr((void**)&NewRootBI->WeldParent, nullptr);

                // if BodyInstance hasn't already been created, we need to initialize it
                bool bHasBodySetup = GetBodySetup() != nullptr;
                if (!bRootIsBeingDeleted && bHasBodySetup && NewRootBI->IsValidBodyInstance() == false)
                {
                    // haker: if BodyInstance is not ready, create new one with InitBody():
                    // - just remember the InitBody() function who initializes FBodyInstance
                    bool bPrevAutoWeld = NewRootBI->bAutoWeld;
                    NewRootBI->bAutoWeld = false;
                    NewRootBI->InitBody(GetBodySetup(), GetComponentToWorld(), this, CurrentWorld->GetPhysicsScene());
                    NewRootBI->bAutoWeld = bPrevAutoWeld;
                }

                // now weld its children to it
                // haker: get children's BodyInstances
                // see GetWeldedBodies (goto 018: AttachToComponent)
                TArray<FBodyInstance*> ChildrenBodies;
                TArray<FName> ChildrenLabels;
                GetWeldedBodies(ChildrenBodies, ChildrenLabels);

                // haker: 2. iterate welded children's BodyInstances and call unweld()
                for (int32 ChildIdx = 0; ChildIdx < ChildrenBodies.Num(); ++ChildIdx)
                {
                    FBodyInstance* ChildBI = ChildrenBodies[ChildIdx];
                    if (ChildBI != NewRootBI)
                    {
                        if (!bRootIsBeingDeleted)
                        {
                            RootBI->UnWeld(ChildBI);
                        }

                        // at this point, NewRootBI must be kinematic because it's being unwelded
                        FPlatformAtomics::InterlockedExchangePtr((void**)&ChildBI->WeldParent, nullptr); // null because we are currently kinematic
                    }
                }

                // if the new root body is simulating, we need to apply the weld on the children
                // haker: 3. weld unwelded childrens to current component's BI
                // see IsInstanceSimulatingPhysics (goto 019: AttachToComponent)
                if (!bRootIsBeingDeleted && NewRootBI->ShouldInstanceSimulatingPhysics())
                {
                    // haker: it calls weld() operation from AttachChildren
                    // - we are not going to ApplyWeldOnChildren in detail
                    NewRootBI->ApplyWeldOnChildren();
                }
            }
        }
    }

    // 021 - Foundation - AttachToComponent - IsSimulatingPhysics
    virtual bool IsSimulatingPhysics(FName BoneName = NAME_None) const override
    {
        // haker: note that bGetWelded is 'true'
        if (FBodyInstance* BodyInst = GetBodyInstance(BoneName))
        {
            return BodyInst->IsInstanceSimulatingPhysics();
        }
        else
        {
            // haker: it handles kinematics for animating object (SkeletalMeshComponent)
            // - for now, it is out of scope, we skip it 
            // - it has complicated process related joint-based physics
            //...
        }
        return false;
    }

    /** does the actual work for welding */
    // 020 - Foundation - AttachToComponent - UPrimitiveComponent::WeldToImplementation
    virtual bool WeldToImplementation(USceneComponent* InParent, FName ParentSocketName=NAME_None, bool bWeldSimulatedChild=true)
    {
        // haker: we already saw UnWeldFromParent(), we can look through WeldToImplementation easily

        // WeldToInternal assumes attachment is already done
        // haker: welding is done after finished to update AttachParent and AttachChildren, Parent and ParentSocketName should be same
        if (GetAttachParent() != InParent || GetAttachSocketName() != ParentSocketName)
        {
            return false;
        }

        // check that we can actually our own socket name
        // haker: if there is no valid BI, return it
        FBodyInstance* BI = GetBodyInstance(NAME_None, false);
        if (BI == NULL)
        {
            return false;
        }

        // haker: only simulated object can be welded
        if (BI->ShouldInstanceSimulatingPhysics() && bWeldSimulatedChild == false)
        {
            return false;
        }

        // make sure that objects marked as non-simulating do not start simulating due to welding
        ECollisionEnabled::Type CollisionType = BI->GetCollisionEnabled();
        if (CollisionType == ECollisionEnabled::QueryOnly || CollisionType == ECollisionEnabled::NoCollision)
        {
            return false;
        }

        // make sure to unweld from wherever we currently are
        // haker: without any condition, first try to unweld!
        UnWeldFromParent();

        // haker: using GetRootWelded, find the outer-most component who has BodyInstance which will be root-BI merging BodyInstances
        FName SocketName;
        UPrimitiveComponent* RootComponent = GetRootWelded(this, ParentSocketName, &SocketName, true);
        if (RootComponent)
        {
            if (FBodyInstance* RootBI = RootComponent->GetBodyInstance(SocketName, false))
            {
                if (BI->WeldParent == RootBI) // already welded so stop
                {
                    return true;
                }

                // there are multiple cases to handle:
                // 1. root is kinematic, simulated
                // 2. child is kinematic, simulated
                // 3. child always inherits from root

                // if root is kinematic simply set child to be kinematic and we're done
                // haker: see UPrimitiveComponent::IsSimulatingPhysics (goto 021: AttachToComponent)
                // - when IsSimulatingPhysics is 'false', set SetSimulatePhysics as false, and failed to weld
                if (RootComponent->IsSimulatingPhysics(SocketName) == false)
                {
                    FPlatformAtomics::InterlockedExchangePtr((void**)&BI->WeldParent, nullptr);
                    SetSimulatePhysics(false);
                    return false;
                }

                // root is simulated so we actually weld the body
                // haker: from here, it is proved that the root is simulating physics object
                RootBI->Weld(BI, GetComponentToWorld());
                return true;
            }
        }
        return false;
    }

    /** get the mask filter we use when moving */
    FMaskFilter GetMoveIgnoreMask() const { return MoveIgnoreMask; }

    virtual void OnRegister() override
    {
        Super::OnRegister();

        // deterministically track primitives via registration sequence numbers
        RegistrationSerialNumber = NextRegistrationSerialNumber.Increment();

        //...
    }

    uint8 bGenerateOverlapEvents : 1;

    /**
     * if true, component sweeps with this component should trace against complex collision during movement (for example, each triangle of a mesh)
     * if false, collision will be resolved against simple collision bounds instead 
     */
    uint8 bTraceComplexOnMove : 1;

    /** if true, component sweeps will return the material in their hit result */
    uint8 bReturnMaterialOnMove : 1;

    /** Primitive is part of a batch being bulk reregistered: applies to UStaticMeshComponent, see FStaticMeshComponentBulkReregisterContext for details */
    mutable uint8 bBulkReregister : 1;

    FMaskFilter MoveIgnoreMask;

    /** 
	 * event called when a component hits (or is hit by) something solid. This could happen due to things like Character movement, using Set Location with 'sweep' enabled, or physics simulation.
	 * for events when objects overlap (e.g. walking into a trigger) see the 'Overlap' event.
	 *
	 * @note:
     * - for collisions during physics simulation to generate hit events, 'Simulation Generates Hit Events' must be enabled for this component.
	 * - when receiving a hit from another object's movement, the directions of 'Hit.Normal' and 'Hit.ImpactNormal' will be adjusted to indicate force from the other object against this object.
	 * - NormalImpulse will be filled in for physics-simulating bodies, but will be zero for swept-component blocking collisions.
	 */
	FComponentHitSignature OnComponentHit;

    /** set of components that this component is currently overlapping */
    TArray<FOverlapInfo> OverlappingComponents;

    /**
     * count of all component overlap events (begin or end) ever generated for any components
     * changes to this number within a scope can also be a simple way to know if any events were triggered
     * it can also be useful for identifying performance issues due to high numbers of events 
     */
    static uint32 GlobalOverlapEventsCounter = 0;

    /**
     * event called when something starts to overlaps this component, for example a player walking into a trigger
     * for events when objects have a blocking collision, for example a player hitting a wall, see 'Hit' events
     * 
     * @note:
     * - both this component and the other one must have GetGenerateOverlapEvents() set to true to generate overlap events
     * - when receiving an overlap from another object's movement, the directions of 'Hit.Normal' and 'Hit.ImpactNormal' will be adjusted to indicate force from the other object against this object
     */
    FComponentBeginOverlapSignature OnComponentBeginOverlap;

    /** event called when something stops overlapping this component */
    FComponentEndOverlapSignature OnComponentEndOverlap;

    /** 
     * set of actors/components to ignore during component sweeps in MoveComponent()
     * - all components owned by these actors will be ignored when this component moves or updates overlaps
     * - components on the other Actor may also need to be told to do the same when they move
     * - does not affact movement of this component when simulating physics 
     */
    TArray<TObjectPtr<AActor>> MoveIgnoreActors;
    TArray<TObjectPtr<UPrimitiveComponent>> MoveIgnoreComponents;

    /** physics scene information for this component, holds a single rigid body with multiple shapes */
    FBodyInstance BodyInstance;

    /** used by the renderer, to identify a component across re-register */
    FPrimitiveComponentId ComponentId;

    /**
     * identifier used to track the time that this component was registered with the world / renderer
     * updated to unique incremental value each time OnRegister() is called. the value of 0 is unused 
     */
    int32 RegistrationSerialNumber = -1;

    /** last time the component was submitted for rendering (called FScene::AddPrimitive) */
    float LastSubmitTime;

    /** the primitive scene info */
    FPrimitiveSceneProxy* SceneProxy;
};