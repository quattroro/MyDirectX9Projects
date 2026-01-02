
/** projection data for a FSceneView */
// 019 - Foundation - SceneRenderer * - FSceneViewProjectionData
struct FSceneViewProjectionData
{
    void SetViewRectangle(const FIntRect& InViewRect)
    {
        ViewRect = InViewRect;
        ConstrainedViewRect = InViewRect;
    }

    // haker: SceneViewProjectionData has all data related Camera:
    // - ViewOrigin, ViewRotationMatrix, ProjectMatrix, ViewRect, ... etc

    /** the view origin */
    FVector ViewOrigin;

    /** rotation matrix transforming from world space to view space */
    FMatrix ViewRotationMatrix;

    /** UE projection matrix projects such that clip space Z=1 is the near plane, and Z=0 is the infinite far plane */
    FMatrix ProjectionMatrix;

    /** the unconstrained (no aspect ratio bars applied) view rectangle (also unscaled) */
    FIntRect ViewRect;

    /** the constrained view rectangle (identical to UnconstrainedUnscaleViewRect if aspect ratio is not constrained) */
    FIntRect ConstrainedViewRect;
};

/** construction parameters for a FSceneView */
// 018 - Foundation - SceneRenderer * - FSceneViewInitOptions
// haker: see FSceneViewProjectionData first (goto 019: SceneRenderer)
struct FSceneViewInitOptions : public FSceneViewProjectionData
{
    const FSceneViewFamily* ViewFamily;
    FSceneViewStateInterface* SceneViewStateInterface;

    // haker: ViewActor is the target in the world:
    // - could be CameraActor or Pawn
    const AActor* ViewActor;

    /** the view origin and rotation */
    FVector ViewLocation;
    FRotator ViewRotation;

    /** actual field of view and that desired by the camera originally */
    float FOV;
    float DesiredFOV;
};

/** a projection from screen space into a 2D screen region */
// 015 - Foundation - SceneRenderer * - FSceneView
class FSceneView
{
    // haker: where SceneView is included in the family(SceneViewFamily)
    const FSceneViewFamily* Family;

    // haker: it has the instance of FSceneViewState:
    // - FSceneViewState inherits FSceneViewStateInterface
    // - we are not going to deal with FSceneViewState in detail, but try to understand it conceptually:
    //                                                                                                                                                      
    //    LocalPlayer                                                                                                                                       
    //     │                                  1.Using PlayerCameraManager, create new SceneView every frame                                                 
    //     │                                     : We can think of 1 on 1 relation between PlayerCameraManager and SceneView                                
    //     │                                                      ────┬─────────────────────────────────────────────────────                                
    //     ├──PlayerController                                        │          ***                                                                        
    //     │   │                                                      │                                                         SceneViewFamilyContext      
    //     │   │                                                      │                                                                            │        
    //     │   │                                                      │                                                                            │        
    //     │   └──PlayerCameraManager ──────────────────────────┐     │         ┌──────────────┐                                                   │        
    //     │       │                                            ├─────▼─────────┤new FSceneView├────────────────► SceneViews: TArray<FSceneView>───┘        
    //     │       │                                            │               └─────┬────────┘                                                            
    //     │       │                                            │                     │                                                                     
    //     │       ├──ViewTarget: FTViewTarget ─────────────────┤                     └─State: FSceneViewStateInterface*                                    
    //     │       │                                            │                         ▲                                                                 
    //     │       │                                            │                         │                                                                 
    //     │       │                                            │                         │                                                                 
    //     │       └──CameraCachePrivate: FCameraCacheEntry ────┘                         │                                                                 
    //     │                                                                              │                                                                 
    //     │                                                                              │                                                                 
    //     │                                                                              │                                                                 
    //     └──ViewStates: TArray<FSceneViewStateReference> ───────────────────────────────┘                                                                 
    //                                                                                                                                                      
    //                                                                                                                                                      
    //                                                      2.FSceneViewState is cached in 'LocalPlayer'                                                    
    //                                                        : whereas FSceneView is created and destroyed every frame                                     
    //                                                          we can think of SceneViewState as persistent state for camera information                   
    //                                                                 ──────────────────────────────────────────────────────────────────                   
    //                                                                   *** so, we cache SceneViewState in 'LocalPlayer'                                   
    //                                                                                                                                                      
    // - see brifely ULocalPlayer::ViewStates (goto 016)
    FSceneViewStateInterface* State;
};

/** 
 * an interface to the private scene manager implementation of a scene
 * use GetRendererModule().AllocateScene() to create the scene 
 */
class FSceneInterface
{

};

/**
 * encapsulates the data which is mirrored the render a UPrimitiveComponent parallel to the game thread
 * this is intended to be subclassed to support different primitive types
 */
// 008 - Foundation - CreateRenderState * - FPrimitiveSceneProxy
// - try to understand FPrimitiveSceneProxy with the diagram:
//                                                                                
//   World                                                     Scene(RenderWorld) 
//    │                                                                       │   
//    └──Actor0                                                               │   
//        │                                                                   │   
//        ├──PrimitiveComponent0 ─────────▲──┐           PrimitiveSceneInfo0──┤   
//        │                               │  │                           │    │   
//        │                               │  └─────►PrimitiveScneProxy0──┘    │   
//        │                               │                                   │   
//        ├──StaticMeshComponent0───────▲─┼──┐           PrimitiveSceneInfo1──┤   
//        │                             │ │  │                           │    │   
//        │                             │ │  └─────►PrimitiveScneProxy1──┘    │   
//        │                             │ │                                   │   
//        └──SkeletalMeshComponent0───▲─┼─┼──┐           PrimitiveSceneInfo2──┘   
//                                    │ │ │  │                           │        
//                                    │ │ │  └─────►PrimitiveScneProxy2──┘        
//                                    ├─┴─┘                                       
//                                    │                                           
//                                    │                                           
//                                    │                                           
//                 *** Component creates respective scene proxy                   
//
// - you can think of 'SceneProxy' as state reflecting corresponding PrimitiveComponent's status
//   - PrimitiveSceneInfo as 'Actor' or 'owner of the data'
//   - PrimitiveSceneProxy as 'Component' or 'the data'
//
// - see FPrimitiveSceneProxy's member variables ***
class FPrimitiveSceneProxy
{
    FPrimitiveSceneProxy(const UPrimitiveComponent* InComponent, FName InResourceName)
        // haker: PrimitiveComponentId comes from PrimitiveComponent::ComponentId
        // - see UPrimitiveComponent's constructor (goto 009: CreateRenderState)
        : PrimitiveComponentId(InComponent->ComponentId)
        , PrimitiveSceneInfo(nullptr)
    {
        //...
    }

    /**
     * updates the primitive proxy's cached transforms, and calls OnUpdateTransform to notify it of the change
     * called in the thread that owns the proxy; game or rendering
     */
    // 013 - Foundation - CreateRenderState * - FPrimitiveSceneProxy::SetTransform()
    void SetTransform(const FMatrix& ToLocalToWorld, const FBoxSphereBounds& InBounds, const FBoxSphereBounds& InLocalBounds, FVector InActorPosition)
    {
        // haker: the above comment says that it can be called in game thread...
        // - it is UN-safe to call this in game thread, it can cause race condition while render-thread accessing proxy's member variables

        // update the cached transforms
        LocalToWorld = ToLocalToWorld;
        bIsLocalToWorldDeterminantNegative = LocalToWorld.Determinant() < 0.f;

        // update the cached bounds: pad them to account for max WPO and material displacement
        const float PadAmount = GetAbsMaxDisplacement();
        Bounds = PadBounds(InBounds, PadAmount);
        LocalBounds = PadLocalBounds(InLocalBounds, LocalToWorld, PadAmount);
        ActorPosition = InActorPosition;

        //...
    }

    /**
     * id for the component this proxy belongs to 
     */
    // haker: primitive-component id is 'persistent':
    // - see FPrimitiveSceneProxy's constructor
    FPrimitiveComponentId PrimitiveComponentId;

    /** pointer back to the PrimitiveSceneInfo that owns this proxy */
    // haker: the owner of SceneProxy in render-world
    FPrimitiveSceneInfo* PrimitiveSceneInfo;

    /** the primitive's local to world transform */
    FMatrix LocalToWorld;

    /** the primitive's bounds */
    FBoxSphereBounds Bounds;

    /** the primitive's local space bounds */
    FBoxSphereBounds LocalBounds = FBoxSphereBounds(ForceInit);

    /** the component's actor's position */
    FVector ActorPosition;

    uint8 bIsLocalToWorldDeterminantNegative : 1;
};

/** 
 * the renderer's internal state for a single UPrimitiveComponent
 * this has a one to one mapping FPrimitiveSceneProxy, which is in the engine module
 */
// 006 - Foundation - CreateRenderState * - FPrimitiveSceneInfo
// - what is the PrimitiveSceneInfo?
//                                                                           
//   World                                              Scene(RenderWorld)   
//    │                                                                │     
//    ├──Actor0                                                        │     
//    │   │                                                            │     
//    │   ├──PrimitiveComponent0 ───────────────► PrimitiveSceneInfo0──┤     
//    │   │                                                            │     
//    │   ├──StaticMeshComponent0 ──────────────► PrimitiveSceneInfo1──┤     
//    │   │                                                            │     
//    │   └──SkeletalMeshComponent0 ────────────► PrimitiveSceneInfo2──┤     
//    │                                                                │     
//    │                                                                │     
//    ├──Actor1                                                        │     
//    │   │                                                            │     
//    │   └──StaticMeshComponent1 ──────────────► PrimitiveSceneInfo3──┤     
//    │                                                                │     
//    │                                                                │     
//    └──Actor2                                                        │     
//        │                                                            │     
//        ├──SkeletalMeshComponent1 ────────────► PrimitiveSceneInfo4──┤     
//        │                                                            │     
//        └──StaticMeshComponent2 ──────────────► PrimitiveSceneInfo5──┘     
//                                                                           
// - see FPrimitiveSceneInfo's member variables (goto 007: CreateRenderState)
//                                                                         
class FPrimitiveSceneInfo : public FDeferredCleanupInterface
{
    FPrimitiveSceneInfo(UPrimitiveComponent* InComponent, FScene* InScene) :
        Proxy(InComponent->SceneProxy),
        PrimitiveComponentId(InComponent->ComponentId),
        RegistrationSerialNumber(InComponent->RegistrationSerialNumber),
    {

    }

    /** adds the primitive to the scene */
    // 040 - Foundation - SceneRenderer * - FPrimitiveSceneInfo::AddToScene
    // haker: add primitive to additional data structure (efficiently managing purpose)
    static void AddToScene(FScene* Scene, TArrayView<FPrimitiveSceneInfo*> SceneInfos)
    {
        //...

        // AddToPrimitiveOctree
        {
            for (FPrimitiveSceneInfo* SceneInfo : SceneInfos)
            {
                // create potential storage for our compact info
                FPrimitiveSceneInfoCompact CompactPrimitiveSceneInfo(SceneInfo);

                // Add the primitive to the octree.
                Scene->PrimitiveOctree.AddElement(CompactPrimitiveSceneInfo);
            }
        }

        // update bounds
        {
            for (FPrimitiveSceneInfo* SceneInfo : SceneInfos)
            {
                FPrimitiveSceneProxy* Proxy = SceneInfo->Proxy;
                int32 PackedIndex = SceneInfo->PackedIndex;

                Scene->PrimitiveSceneProxies[PackedIndex] = Proxy;
                Scene->PrimitiveTransforms[PackedIndex] = Proxy->GetLocalToWorld();

                // set bounds.
                FPrimitiveBounds& PrimitiveBounds = Scene->PrimitiveBounds[PackedIndex];
                FBoxSphereBounds BoxSphereBounds = Proxy->GetBounds();
                PrimitiveBounds.BoxSphereBounds = BoxSphereBounds;
                PrimitiveBounds.MinDrawDistance = Proxy->GetMinDrawDistance();
                PrimitiveBounds.MaxDrawDistance = Proxy->GetMaxDrawDistance();
                PrimitiveBounds.MaxCullDistance = PrimitiveBounds.MaxDrawDistance;

                //...
            }
        }
    }

    /** creates cached mesh draw commands for all meshes */
    static void CacheMeshDrawCommands(FScene* Scene, TArrayView<FPrimitiveSceneInfo*> SceneInfos)
    {
        //...
    }

    /**
     * retrieves the index of the primitive in the scene's primitives array
     * this index is only valid until a primitive is added to or removed from the scene 
     */
    int32 GetIndex() const { return PackedIndex; }

    // 007 - Foundation - CreateRenderState * - FPrimitiveSceneInfo's member variables
    
    /** the render proxy for the primitive */
    // haker: see FPrimitiveSceneProxy (goto 008: CreateRenderState)
    FPrimitiveSceneProxy* Proxy;

    /**
     * Id for the component this primitive belong to
     * this will stay the same for the lifetime of the component, so it can be used to identify the component across re-register 
     */
    // haker: as we saw, PrimitivieComponentId is NOT changed until the component is destroyed
    FPrimitiveComponentId PrimitiveComponentId;

    /**
     * number assigned to this component when it was registered with the world
     * this will only ever be updated if the object is re-registered
     * used by FPrimitiveArraySortKey for deterministic ordering 
     */
    // haker: 'RegistrationSerialNumber' is updated only if the PrimitiveComponent is re-register
    // - what 're-register' means?
    //                                                                                                   
    //  World                                                                                            
    //   │                                                                                               
    //   └──Actor0                                                                                       
    //       │ ┌────────────────────┐                                                                    
    //       └─┤StaticMeshComponent0├──┐                                                                 
    //         └───────────────────▲┘  │Reattch(detach+attach) is called                                 
    //                             │   │    │                                                            
    //                             └───┘    │                                                            
    //                                      │                                                            
    //                                      │                                                            
    //  Scene(RenderWorld)                  │                                                            
    //   │ ┌────────────────────────┐       │Re-create SceneInfo and SceneProxy by calling OnRegister()  
    //   └─┤PrimitiveSceneInfo      │       │                                             ────────────── 
    //     │ │                      ◄───────┘                                                 ***        
    //     │ └──PrimitiveSceneProxy │                                                          │         
    //     │                        │                                                          │         
    //     └────────────────────────┘                          ┌───────────────────────────────┘         
    //                                                         │                                         
    //                                                         │                                         
    //                                                         ▼                                         
    //                                        call unregister/register by reregister():                  
    //                                          'RegistrationSerialNumber' is updated                    
    //  
    // - what 'RegistrationSerialNumber' means?
    //   - we can think of 'RegistrationSerialNumber' as sequence of component creation
    //   - see UPrimitiveComponent::OnRegister()
    //   - if we sort all SceneInfo/SceneProxy as RegistrationSerialNumber, they are sorted by ***"creation-time"
    //                                                                                                    
    int32 RegistrationSerialNumber;

    /**
     * the index of the primitive in the scene's packed arrays:
     * this value may change as parameters are added and removed from the scene 
     */
    // haker: see FScene first (goto 010: CreateRenderState)
    // - now we can understand what 'PackedIndex' is:
    //   - it is the 'INDEX' of SOA in Scene to lookup render-object's properties(or attributes)
    int32 PackedIndex;
};

// 038 - Foundation - SceneRenderer * - FPrimitiveArraySortKey
struct FPrimitiveArraySortKey
{
    inline bool operator()(const FPrimitiveSceneInfo& A, const FPrimitiveSceneInfo& B) const
    {
        // haker: see FStaticMeshSceneProxy::GetTypeHash (goto 039: SceneRenderer)
        uint32 AHash = A.Proxy->GetTypeHash();
        uint32 BHash = B.Proxy->GetTypeHash();

        if (AHash == BHash)
        {
            // haker: if A and B's proxy hash is same, we sort them by 'RegistrationSerialNumber'
            // - by doing this, we can preserve deterministic order of proxies in the scene 
            return A.RegistrationSerialNumber < B.RegistrationSerialNumber;
        }
        else
        {
            return AHash < BHash;
        }
    }
};

/** a simple chunked array representation for scene primitives data arrays */
template <typename T>
class TScenePrimitiveArray
{
    static const int32 NumElementsPerChunk = 1024;

    T& Add(const T& Element)
    {
        return *(new (&AddUninitialized()) T(Element));
    }

    T& AddUninitialized()
    {
        if (NumElements % NumElementsPerChunk == 0)
        {
            ChunkType* Chunk = new ChunkType;
            Chunk->Reserve(NumElementsPerChunk);
            Chunks.Emplace(Chunk);
        }

        NumElements++;
        ChunkType& Chunk = *Chunks.Last();
        Chunk.AddUninitialized();
        return Chunk.Last();
    }

    T& Get(int32 ElementIndex)
    {
        const uint32 ChunkIndex        = ElementIndex / NumElementsPerChunk;
        const uint32 ChunkElementIndex = ElementIndex % NumElementsPerChunk;
        return (*Chunks[ChunkIndex])[ChunkElementIndex];
    }

    FORCEINLINE T& operator[] (int32 Index) { return Get(Index); }

    using ChunkType = TArray<T>;
    TArray<TUniquePtr<ChunkType>> Chunks;
    uint32 NumElements = 0;
};

template<typename T>
static void TArraySwapElements(TArray<T>& Array, int i1, int i2)
{
	T tmp = Array[i1];
	Array[i1] = Array[i2];
	Array[i2] = tmp;
}

/**
 * renderer scene which is private to the renderer module
 * originarily this is the renderer version of a UWorld, but an FScene can be created for previewing in editors which don't have a UWorld as well
 * the scene stores renderer state that is independent of any view or frame, with the primary actions being adding or removing of primitives and lights
 */
// 010 - Foundation - CreateRenderState * - FScene
// haker: Scene is render-world reflecting World(UWorld)
// - see FScene's member variables (goto 011: CreateRenderState)
class FScene : public FSceneInterface
{
    /** adds a primitive to the scene: called in the rendering thread by AddPrimitive */
    // 014 - Foundation - CreateRenderState * - FScene::AddPrimitiveSceneInfo_RenderThread
    void AddPrimitiveSceneInfo_RenderThread(FPrimitiveSceneInfo* PrimitiveSceneInfo, const TOptional<FTransform>& PreviousTransform)
    {
        // haker: we add new SceneInfo to pending queue, 'AddedPrimitiveSceneInfos'
        // - 'AddedPrimitiveSceneInfos' will be processed in FScene::UpdateAllPrimitiveSceneInfos()
        AddedPrimitiveSceneInfos.FindOrAdd(PrimitiveSceneInfo);
    }

    /** updates a primitive's transform, called on the rendering thread */
    void UpdatePrimitiveTransform_RenderThread(FPrimitiveSceneProxy* PrimitiveSceneProxy, const FBoxSphereBounds& WorldBounds, const FBoxSphereBounds& LocalBounds, const FMatrix& LocalToWorld, const FVector& OwnerPosition, const TOptional<FTransform>& PreviousTransform)
    {
        // haker: add pending list's element for updating transform in FScene to UpdatedTransforms
        UpdatedTransforms.Update(PrimitiveSceneProxy, { WorldBounds, LocalBounds, LocalToWorld, AttachmentRootPosition });
    }

    // 008 - Foundation - SceneRenderer * - FScene::UpdatePrimitiveTransform
    virtual void UpdatePrimitiveTransform(UPrimitiveComponent* Primitive) override
    {
        // save the world transform for next time the primitive is added to the scene
        // haker: similar to BatchAddPrimitives()
        const float WorldTime = GetWorld()->GetTimeSeconds();
        float DeltaTime = WorldTime - Primitive->LastSubmitTime;
        if (DeltaTime < -0.0001f || Primitive->LastSubmitTime < 0.0001f)
        {
            // Time was reset?
            Primitive->LastSubmitTime = WorldTime;
        }
        else if (DeltaTime > 0.0001f)
        {
            // First call for the new frame?
            Primitive->LastSubmitTime = WorldTime;
        }

        if (Primitive->SceneProxy)
        {
            // check if the primitive needs to recreate its proxy for the transform update
            // haker: we could also update transform by removing and adding primitive
            if (Primitive->ShouldRecreateProxyOnUpdateTransform())
            {
                // re-add the primitive from scratch to recreate the primitive's proxy
                RemovePrimitive(Primitive);
                AddPrimitive(Primitive);
            }
            else
            {
                // haker: see GetActorPositionForRenderer (goto 009: SceneRenderer)
                FVector AttachmentRootPosition = Primitive->GetActorPositionForRenderer();

                // haker: similar to BatchAddPrimitives(), we construct parameter data to transfer to RenderThread
                struct FPrimitiveUpdateParams
                {
                    FScene* Scene;
                    FPrimitiveSceneProxy* PrimitiveSceneProxy;
                    FBoxSphereBounds WorldBounds;
                    FBoxSphereBounds LocalBounds;
                    FMatrix LocalToWorld;
                    TOptional<FTransform> PreviousTransform;
                    FVector AttachmentRootPosition;
                };

                FPrimitiveUpdateParams UpdateParams;
                UpdateParams.Scene = this;
                UpdateParams.PrimitiveSceneProxy = Primitive->SceneProxy;
                UpdateParams.WorldBounds = Primitive->Bounds;
                UpdateParams.LocalToWorld = Primitive->GetRenderMatrix();
                UpdateParams.AttachmentRootPosition = AttachmentRootPosition;
                UpdateParams.LocalBounds = Primitive->GetLocalBounds();
                UpdateParams.PreviousTransform = FMotionVectorSimulation::Get().GetPreviousTransform(Primitive);

                // haker: see UpdatePrimitiveTransform_RenderThred ***
                ENQUEUE_RENDER_COMMAND(UpdateTransformCommand)(
                    [UpdateParams](FRHICommandListImmediate& RHICmdList)
                    {
                        UpdateParams.Scene->UpdatePrimitiveTransform_RenderThread(
                            UpdateParams.PrimitiveSceneProxy,
                            UpdateParams.WorldBounds,
                            UpdateParams.LocalBounds,
                            UpdateParams.LocalToWorld,
                            UpdateParams.AttachmentRootPosition,
                            UpdateParams.PreviousTransform
                        );
                    }
                );
            }
        }
    }

    // 005 - Foundation - CreateRenderState * - FScene::BatchAddPrimitives
    virtual void BatchAddPrimitives(TArrayView<UPrimitiveComponent*> InPrimitives) override
    {
        // haker: see briefly FCreateRenderThreadParameters
        // - now it's time to see what render object is, aligned to UPrimitiveComponent
        // - see FPrimitiveSceneInfo (goto 006: CreateRenderState)
        // - 'FCreateRenderThreadParameters' is the wrapper class to transfer necessary inputs to "render-thread"
        //    - try to understand how reading/writing to scene works:
        //                                                                                                                                                     
        //      :World is only allowed to read/write in GameThread         :Scene is only allowed to read/write in RenderThread                                
        //                                ────────────────────────                                   ────────────────────────                                  
        //               ***GameThread     ****                                 ***RenderThread       ****                                                     
        //                  ┌───────┐                                        ┌─────────────────────┐                                                           
        //                  │ World │                                        │ Scene(Render-World) │                                                           
        //                  └───┬───┘                                        └──────────┬──────────┘                                                           
        //                      │                                                       │                                                                      
        //                      │                                                       │                                                                      
        //                      │                                                       │                                                                      
        //                      ├────┐                                                  │                                                                      
        //                      │    │ Create PrimitiveSceneInfo/SceneProxy             │                                                                      
        //                      │    │ (in GameThread)                                  │                                                                      
        //                      ◄────┘                                                  │                                                                      
        //                      │                                                       │                                                                      
        //                      ├───────────────────────────────────────────────────────►                                                                      
        //                      │       Add RenderCommand with ParamList                │                                                                      
        //                      │       : ParamList is                                  │                                                                      
        //                      │         TArray<FCreateRenderThreadParameters>         │                                                                      
        //                      │                                                       ├────┐                                                                 
        //                      │                                                       │    │1.FPrimitiveSceneProxy::SetTransform()                           
        //                      │                                                       │    │2.Process ParamList to add inputs into AddedPrimitiveSceneInfos  
        //                      │                                                       ◄────┘                                       ──┬─────────────────────  
        //                      │                                                       │                                              │                       
        //                      │                                                       │                                              │                       
        //                      │                                                       │                                              │                       
        //                      │                                                       ├────┐                                         │                       
        //                      │                                                       │    │UpdateAllPrimitiveSceneInfos():          ▼                       
        //                      │                                                       │    │  process pending scene infos(AddedPrimitiveSceneInfos)          
        //                      │                                                       ◄────┘                                                                 
        //                      │                                                       │                                                                      
        //                                                                                                                                                             
        //            
        struct FCreateRenderThreadParameters
        {
            FPrimitiveSceneInfo* PrimitiveSceneInfo;
            FPrimitiveSceneProxy* PrimitiveSceneProxy;
            FMatrix RenderMatrix;
            FBoxSphereBounds WorldBounds;
            FVector AttachmentRootPosition;
            FBoxSphereBounds LocalBounds;
            TOptional<FTransform> PreviousTransform;
        };

        TArray<FCreateRenderThreadParameters> ParamsList;
        ParamsList.Reserve(InPrimitives.Num());

        // haker: iterate batched primitives and prepare 'FCreateRenderThreadParameters'
        for (UPrimitiveComponent* Primitive : InPrimitives)
        {
            const float WorldTime = GetWorld()->GetTimeSeconds();

            // save the world transform for next time the primitive is added to the scene
            float DeltaTime = WorldTime - Primitive->LastSubmitTime;
            if (DeltaTime < -0.0001f || Primitive->LastSubmitTime < 0.0001f)
            {
                // the was reset?
                // haker: do we have negative DeltaTime?...
                Primitive->LastSubmitTime = WorldTime;
            }
            else if (DeltaTime > 0.001f)
            {
                // first call for the new frame
                Primitive->LastSubmitTime = WorldTime;
            }
            // haker: else condition is -0.0001f < DeltaTime < 0.001f
            // - Primitive->LastSubmitTime is remained when it is nearly '0'

            // create the primitive's scene proxy
            // haker: see UStaticMeshComponent::CreateSceneProxy (goto 012: CreateRenderState)
            FPrimitiveSceneProxy* PrimitiveSceneProxy = Primitive->CreateSceneProxy();

            // haker: see UPrimitiveComponent's SceneProxy briefly:
            // - PrimitiveComponent has the ownership of SceneProxy
            Primitive->SceneProxy = PrimitiveSceneProxy;
            if (!PrimitiveSceneProxy)
            {
                // primitive which don't have a proxy are irrelevant to the scene manager
                continue;
            }

            // create the primitive scene info
            // haker: PrimitiveSceneInfo creation is also done by GameThread!
            // - construct link between PrimitiveSceneInfo and PrimitiveSceneProxy
            FPrimitiveSceneInfo* PrimitiveSceneInfo = new FPrimitiveSceneInfo(Primitive, this);
            PrimitiveSceneProxy->PrimitiveSceneInfo = PrimitiveSceneInfo;

            // cache the primitives initial transform
            // haker: see GetRenderMatrix briefly:
            // - it returns ComponentTransform()
            // - RenderMatrix is the transform data to apply to the render-world(FScene)
            FMatrix RenderMatrix = Primitive->GetRenderMatrix();
            FVector AttachmentRootPosition = Primitive->GetActorPositionForRenderer();

            // haker: we contruct each parameter(FCreateRenderThreadParameters) per PrimitiveComponent
            ParameterList.Emplace(	
                PrimitiveSceneInfo,
                PrimitiveSceneProxy,
                RenderMatrix,
                Primitive->Bounds,
                AttachmentRootPosition,
                Primitive->GetLocalBounds()
            );
        }

        // create any RenderThreadResources required and send a command to the rendering thread to add the primitive to the scene
        FScene* Scene = this;

        // haker: ENQUEUE_RENDER_COMMAND is lambda function executed in render-thread:
        // - let's understand ENQUEUE_RENDER_COMMAND conceptually:
        //                                                                                                        
        //   [GameThread]                                                                                 
        //   ─────────────────┬────────────────────────────────────────────────────────────────────────►  
        //                    │                                                                           
        //                    │                                                                           
        //                    │ENQUEUE_RENDER_COMMAND(Lambda)                                             
        //                    │                                                                           
        //                    │                                                                           
        //                  ┌─▼──────────────┐                                                            
        //   [RenderThread]:│  CommandQueue  │                                                            
        //                  └──────────────▲─┘                                                            
        //                                 │ Polling each command and execute in RenderThread             
        //   ──────────────────────────────┴───────────────────────────────────────────────────────────►  
        //                                                                                                
        ENQUEUE_RENDER_COMMAND(AddPrimitiveCommand)(
            [ParamList = MoveTemp(ParamsList), Scene](FRHICommandListImmediate& RHICmdList)
            {
                // haker: iterating ParamList in 'RenderThread'
                for (const FCreateRenderThreadParameters& Params : ParamsList)
                {
                    FPrimitiveSceneProxy* SceneProxy = Params.PrimitiveSceneProxy;

                    // haker: see FPrimitiveSceneProxy::SetTransform (goto 013: CreateRenderState)
                    SceneProxy->SetTransform(Params.RenderMatrix, Params.WorldBounds, Params.LocalBounds, Params.AttachmentRootPosition);

                    // haker: see AddPrimitiveSceneInfo_RenderThread (goto 014: CreateRenderState)
                    Scene->AddPrimitiveSceneInfo_RenderThread(Params.PrimitiveSceneInfo, Params.PreviousTransform);
                }
            }
        );
    }

    // 004 - Foundation - CreateRenderState * - FScene::AddPrimitive
    virtual void void AddPrimitive(UPrimitiveComponent* Primitive) override
    {
        //...

        // haker: see FScene::BatchAddPrimitives (goto 005: CreateRenderState)
        // - note that we create array view which contains only one!
        BatchAddPrimitives(MakeArrayView(&Primitive, 1));
    }

    // 037 - Foundation - SceneRenderer * - FScene::UpdateAllPrimitiveSceneInfos
    // haker: we add render-object to the Scene from pending queues like 'UpdatedTransforms' and 'AddedPrimitiveSceneInfos'
    // - UpdatedTransforms's commands are queued by 'UpdatePrimitiveTransform_RenderThread'
    // - AddedPrimitiveSceneInfos's commands are queued by 'AddPrimitiveSceneInfo_RenderThread'
    void UpdateAllPrimitiveSceneInfos(FRDGBuilder& GraphBuilder, EUpdateAllPrimitiveSceneInfosAsyncOps AsyncOps)
    {
        // haker: we are focusing on 'AddedPrimitiveSceneInfos' and 'UpdatedTransforms'
        // - other than that, rest of codes are omitted

        // haker: we copy AddedPrimitiveSceneInfos to 'AddedLocalPrimitiveSceneInfos'
        TArray<FPrimitiveSceneInfo*> AddedLocalPrimitiveSceneInfos;
        AddedLocalPrimitiveSceneInfos.Reserve(AddedPrimitiveSceneInfos.Num());
        for (FPrimitiveSceneInfo* SceneInfo : AddedPrimitiveSceneInfos)
        {
            AddedLocalPrimitiveSceneInfos.Add(SceneInfo);
        }

        // haker: this is why we copy it to local variable:
        // - sort them with PrimitiveArraySortKey
        // - see FPrimitiveArraySortKey (goto 038: SceneRenderer)
        // - the expected result of sorting is like this:
        //  0               4                          11                                          21    
        //  │               │                           │                                           │    
        //  │               │                           │                                           │    
        //  │               │                           │                                           │    
        //  ├───┬───┬───┬───┼───┬───┬───┬───┬───┬───┬───┼───┬───┬───┬───┬───┬───┬───┬───┬───┬───┬───┼───┐
        //  │ 0 │ 1 │ 2 │ 3 │ 4 │ 5 │ 6 │ 7 │ 8 │ 9 │ 10│ 11│ 12│ 13│ 14│ 15│ 16│ 17│ 18│ 19│ 20│ 21│...│
        //  ├───┴───┴───┴───┼───┴───┴───┴───┴───┴───┴───┼───┴───┴───┴───┴───┴───┴───┴───┴───┴───┴───┼───┘
        //  │               │                           │                                           │    
        //  │               │                           │                                           │    
        //  │◄─────────────►│◄─────────────────────────►│◄─────────────────────────────────────────►│    
        //  │               │                           │                                           │    
        //                                                                                               
        // StaticMeshSceneProxy    PrimitiveSceneProxy           SkeletalMeshSceneProxy                   
        // :Offset==4              :Offset==11                   :Offset==21                              
        AddedLocalPrimitiveSceneInfos.Sort(FPrimitiveArraySortKey());

        //...

        // haker: classify SceneInfos depending on initialization phases:
        // 1. AddToScene: call FPrimitiveSceneInfo::AddToScene()
        // 2. StaticDrawListUpdate: call FPrimitiveSceneInfo::CacheMeshDrawCommands()
        // - we'll see this phase in detail on the below code
        TArray<FPrimitiveSceneInfo*, SceneRenderingAllocator>& SceneInfosWithAddToScene = *GraphBuilder.AllocateObject<TArray<FPrimitiveSceneInfo*, SceneRenderingAllocator>>();
        TArray<FPrimitiveSceneInfo*, SceneRenderingAllocator>& SceneInfosWithStaticDrawListUpdate = *GraphBuilder.AllocObject<TArray<FPrimitiveSceneInfo*, SceneRenderingAllocator>>();
        SceneInfosWithAddToScene.Reserve(SceneInfosContainerReservedSize);
        SceneInfosWithStaticDrawListUpdate.Reserve(SceneInfosContainerReservedSize);

        // haker: QueueAddToScene and QueueAddStaticMeshes are the lambda to enqueue into SceneInfosWithAddToScene and SceneInfosWithStaticDrawListUpdate respectively
        const auto QueueAddToScene = [&](FPrimitiveSceneInfo* SceneInfo) -> bool
        {
            if (!SceneInfo->bPendingAddToScene)
            {
                SceneInfo->bPendingAddToScene = true;
                SceneInfosWithAddToScene.Push(SceneInfo);
                return true;
            }
            return false;
        };

        const auto QueueAddStaticMeshes = [&](FPrimitiveSceneInfo* SceneInfo)
        {
            if (!SceneInfo->bPendingAddStaticMeshes)
            {
                SceneInfo->bPendingAddStaticMeshes = 1;
                SceneInfosWithStaticDrawListUpdate.Push(SceneInfo);
                PrimitivesNeedingStaticMeshUpdate[SceneInfo->PackedIndex] = false;
                return true;
            }
            return false;
        };

        //...

        // haker: preserve SOA of the scene:
        // - we'll add SceneInfos into the end of SOA of the scene:
        //   e.g. Primitives, PrimitiveTransforms, PrimitiveSceneProxies, PrimitiveBounds, ...
        //                                                                                                                           │                                             
        //                                                                                                                           │*** We add new PrimitiveSceneInfo from here  
        //                                                                                                                           ├───────────────►                             
        //                                                                                                                           │                                             
        //                            ┌─────────────┬─────────────┬─────────────┬─────────────┬─────────────┬─────────────┬──────────┼──────────┐                                  
        //                Primitives: │ SceneInfo0  │ SceneInfo1  │ SceneInfo2  │ SceneInfo3  │ SceneInfo4  │ SceneInfo5  │ ...      │          │                                  
        //                            └─────────────┴─────────────┴─────────────┴─────────────┴─────────────┴─────────────┴──────────┼──────────┘                                  
        //                                                                                                                           │                                             
        //                            ┌─────────────┬─────────────┬─────────────┬─────────────┬─────────────┬─────────────┬──────────┼──────────┐                                  
        //       PrimitiveTransforms: │ Transform0  │ Transform1  │ Transform2  │ Transform3  │ Transform4  │ Transform5  │ ...      │          │                                  
        //                            └─────────────┴─────────────┴─────────────┴─────────────┴─────────────┴─────────────┴──────────┼──────────┘                                  
        //                                                                                                                           │                                             
        //                            ┌─────────────┬─────────────┬─────────────┬─────────────┬─────────────┬─────────────┬──────────┼──────────┐                                  
        //     PrimitiveSceneProxies: │ SceneProxy0 │ SceneProxy1 │ SceneProxy2 │ SceneProxy3 │ SceneProxy4 │ SceneProxy5 │ ...      │          │                                  
        //                            └─────────────┴─────────────┴─────────────┴─────────────┴─────────────┴─────────────┴──────────┼──────────┘                                  
        //                                                                                                                           │                                             
        //                            ┌─────────────┬─────────────┬─────────────┬─────────────┬─────────────┬─────────────┬──────────┼──────────┐                                  
        //           PrimitiveBounds: │ Bounds0     │ Bounds1     │ Bounds2     │ Bounds3     │ Bounds4     │ Bounds5     │ ...      │          │                                  
        //                            └─────────────┴─────────────┴─────────────┴─────────────┴─────────────┴─────────────┴──────────┼──────────┘                                  
        //                                                                                                                           │                                                                                       
        if (AddedLocalPrimitiveSceneInfos.Num())
        {
            // haker: we only deal with four data types: Primitives, PrimitiveTransforms, PrimitiveSceneProxies and PrimitiveBounds
            Primitives.Reserve(Primitives.Num() + AddedLocalPrimitiveSceneInfos.Num());
            PrimitiveTransforms.Reserve(PrimitiveTransforms.Num() + AddedLocalPrimitiveSceneInfos.Num());
            PrimitiveSceneProxies.Reserve(PrimitiveSceneProxies.Num() + AddedLocalPrimitiveSceneInfos.Num());
            PrimitiveBounds.Reserve(PrimitiveBounds.Num() + AddedLocalPrimitiveSceneInfos.Num());

            //...
        }

        // haker: we deal with the below code logic conceptually:
        //
        //                                                            ***sorted by GetTypeHash(), SceneProxy's type                                                             
        //                                                                                  │                                                                                   
        //                                                                                  │                                                                                   
        //                                                      ┌───────────────────────────┼────────────────────────┐                                                          
        //                                                      │                           │                        │                                                          
        //                                                      │                           │                        │                                                          
        //                                                      ▼                           ▼                        ▼                                                          
        //                                         │   StaticMeshSceneProxy    │   SkeletalMeshSceneProxy  │    PrimitiveSceneProxy    │                                        
        //                                         │◄─────────────────────────►│◄─────────────────────────►│◄─────────────────────────►│                                        
        //                                         │                           │                           │                           │                                        
        //                                         ├─────────────┬─────────────┼─────────────┬─────────────┼─────────────┬─────────────┤                                        
        //          AddedLocalPrimitiveSceneInfos: │ SceneInfo0  │ SceneInfo1  │ SceneInfo2  │ SceneInfo3  │ SceneInfo4  │ SceneInfo5  │                                        
        //                                         ├─────────────┴─────────────┼─────────────┴─────────────┼─────────────┴─────────────┤                                        
        //                                         │                           │                           │                           │                                        
        //                                         │            (1)            │            (2)            │            (3)            │                                        
        //                                                                                                                                                                      
        //                                                                                                                                                                      
        //          1. We iterates AddedLocalPrimitiveSceneInfos in order of (3)──►(2)──►(1)                                                                                    
        //                                                                                                                                                                      
        //          2. When we adding each block (e.g. (3),(2),(1)):                                                                                                            
        //              │                                                                                                                                                       
        //              │                                                                                                                                                       
        //              ├───2-0. Suppose we have Primitives(PrimitiveSceneInfos) like below:                                                                                    
        //              │                                                                                                                                                       
        //              │                         │             │             │                                                                                                 
        //              │                         ├─────────────┼─────────────┤                                                                                                 
        //              │             Primitives: │ SceneInfo6  │ SceneInfo7  │                                                                                                 
        //              │                         ├─────────────┼─────────────┤                                                                                                 
        //              │                         │   ▲         │          ▲  │                                                                                                 
        //              │                         │   │         │          │  │                                                                                                 
        //              │                      StaticMeshSceneProxy    SkeletalMeshSceneProxy                                                                                   
        //              │                                                                                                                                                       
        //              │                                                                                                                                                       
        //              │                         ┌─────────────────────────┬────────────────────────────┐                                                                      
        //              │        TypeOffsetTable: │(StaticMeshSceneProxy, 1)│ (SkeletalMeshSceneProxy, 2)│                                                                      
        //              │                         └─────────────────────────┴────────────────────────────┘                                                                      
        //              │                                                                                                                                                       
        //              │                                                                                                                                                       
        //              ├───2.1. add elements in block(3) to SOA of the scene:                                                                                                  
        //              │                                                                                                                                                       
        //              │                        │             │             │ *** newly added!                                                                                 
        //              │                        ├─────────────┼─────────────┼────────────┬────────────┐                                                                        
        //              │            Primitives: │ SceneInfo6  │ SceneInfo7  │ SceneInfo4 │ SceneInfo5 │                                                                        
        //              │                        ├─────────────┼─────────────┼────────────┴────────────┘                                                                        
        //              │                        │   ▲         │          ▲  │                                                                                                  
        //              │                        │   │         │          │  │                                                                                                  
        //              │                     StaticMeshSceneProxy    SkeletalMeshSceneProxy                                                                                    
        //              │                                                                                                                                                       
        //              │                        ┌─────────────┬─────────────┬────────────┬────────────┐                                                                        
        //              │   PrimitiveTransforms: │ Transform6  │ Transform7  │ Transform4 │ Transform5 │                                                                        
        //              │                        └─────────────┴─────────────┴────────────┴────────────┘                                                                        
        //              │                                                                                                                                                       
        //              │                       *** like PrimitiveTransforms, PrimitiveSceneProxies and PrimitiveBounds are extended based on block(3)                          
        //              │                           ───────────────────────────────────────────────────────────────────────────────────────────────────                         
        //              │                                                                                                                                                       
        //              │                                                                                                                                                       
        //              ├────2-2. Find the entry by 'FScene::TypeOffsetTable'                                                                                                   
        //              │          │                                                                                                                                            
        //              │          │                                                                                                                                            
        //              │          ├────if we failed to find the entry                                                                                                          
        //              │          │    : we add the new 'TypeOffsetEntry' to the FScene::TypeOffsetTable                                                                       
        //              │          │        │                                                                                                                                   
        //              │          │        │                                                                                                                                   
        //              │          │        └───In our case, we don't have 'PrimitiveSceneProxy'                                                                                
        //              │          │            : when we process the block(3), we add new TypeOffsetEntry for 'PrimitiveSceneProxy' as (PrimitiveSceneProxy, 2)                
        //              │          │                                                                                                                                            
        //              │          │                                                                                                                                            
        //              │          └────TypeOffsetTable looks like this:                                                                                                        
        //              │                                                                                                                                                       
        //              │                                   ┌─────────────────────────┬────────────────────────────┬─────────────────────────┐                                  
        //              │                  TypeOffsetTable: │(StaticMeshSceneProxy, 1)│ (SkeletalMeshSceneProxy, 2)│ (PrimitiveSceneProxy, 2)│                                  
        //              │                                   └─────────────────────────┴──────────────────────────┬─┴───────────────────────▲─┘                                  
        //              │                                                                                        │                         │                                    
        //              │                                                                                        └─────────────────────────┘                                    
        //              │                                                                                        initialize as '2'                                              
        //              ├────2.3. Swap the SOA of the scene:                                                     ─────────┬────────                                             
        //              │          │                                                                                      │                                                     
        //              │          │                     ┌────────────┬────────────┐                                      │                                                     
        //              │          └───Iterate block(3): │ SceneInfo4 │ SceneInfo5 │                                      │                                                     
        //              │                │               └────────────┴────────────┘                                      │                                                     
        //              │                │  SourceIndex:     2             3                                              │                                                     
        //              │                │                   │             │                                              │                                                     
        //              │                │                   │             └────────────────────────────┐                 │                                                     
        //              │                │                   │                                          │                 │                                                     
        //              │                │                   └─────────────────────────────┐            │                 │                                                     
        //              │                │                                                 │            │                 │                                                     
        //              │                │               ┌─────────────┬─────────────┬─────▼──────┬─────▼──────┐          │                                                     
        //              │                │   Primitives: │ SceneInfo6  │ SceneInfo7  │ SceneInfo4 │ SceneInfo5 │          │                                                     
        //              │                │               └─────────────┴─────────────┴────────────┴────────────┘          │                                                     
        //              │                │                    [0]            [1]          [2]          [3]                │                                                     
        //              │                │                                                                                │                                                     
        //              │                │                                                                                │                                                     
        //              │                └────Iterate [BroadIndex, TypeOffsetTable.Num()] as TypeIndex                    │ *** As a result, Offset becomes 2──►4               
        //              │                     : BroadIndex == 2: (PrimitiveSceneProxy, 2) == TypeOffsetTable[2]           │                                                     
        //              │                       │                                                                         │                                                     
        //              │                       │                                                                         │                                                     
        //              │                       ├───Increase TypeOffsetTable[TypeIndex].Offset++ ◄────────────────────────┘                                                     
        //              │                       │    :DestIndex = TypeOffsetTable[TypeIndex].Offset++                                                                           
        //              │                       │                                                                                                                               
        //              │                       │                                                                                                                               
        //              │                       │                                                                                                                               
        //              │                       └───Swap the SOA of the scene only if DestIndex and SourceIndex is different:                                                   
        //              │                            │                                                                                                                          
        //              │                            │                                                                                                                          
        //              │                            └───In our case:                                                                                                           
        //              │                                ┌────────────────────────────────┐                                                                                     
        //  Block(3)    │                                │ SourceIndex: 2, DestIndex: 2   │                                                                                     
        //    ▲         │                                │          │                     │ *** No need to swap the SOA!                                                        
        //    │         │                                │          ▼                     │                                                                                     
        //    │         │                                │ SourceIndex: 3, DestIndex: 3   │                                                                                     
        //    │         │                                └────────────────────────────────┘                                                                                     
        //    │         │                                                                                                                                                       
        //  ──┼─────────┼───────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────
        //    │         │                                                                                                                                                       
        //    │         │                                                                                                                                                       
        //    │         │                                                                                                                                                       
        //    │         ├────2-0. we have Primitives(PrimitiveSceneInfos) like below:                                                                                           
        //    │         │                                                                                                                                                       
        //    ▼         │                                                                                                                                                       
        //  Block(2)    │                       │             │             │                         │                                                                         
        //              │                       ├─────────────┼─────────────┼────────────┬────────────┤                                                                         
        //              │           Primitives: │ SceneInfo6  │ SceneInfo7  │ SceneInfo4 │ SceneInfo5 │                                                                         
        //              │                       ├─────────────┼─────────────┼────────────┴────────────┤                                                                         
        //              │                       │   ▲         │          ▲  │                   ▲     │                                                                         
        //              │                       │   │         │          │  │                   │     │                                                                         
        //              │                    StaticMeshSceneProxy    SkeletalMeshSceneProxy   PrimitiveSceneProxy                                                               
        //              │                                                                                                                                                       
        //              │                                                                                                                                                       
        //              │                       ┌─────────────────────────┬────────────────────────────┬─────────────────────────┐                                              
        //              │      TypeOffsetTable: │(StaticMeshSceneProxy, 1)│ (SkeletalMeshSceneProxy, 2)│ (PrimitiveSceneProxy, 4)│                                              
        //              │                       └─────────────────────────┴────────────────────────────┴─────────────────────────┘                                              
        //              │                                                                                                                                                       
        //              │                                                                                                                                                       
        //              │                                                                                                                                                       
        //              │                                                                                                                                                       
        //              ├────2.1. add elements in block(2) to SOA of the scene:                                                                                                 
        //              │                                                                                                                                                       
        //              │                      │             │             │                         │*** newly added SkeletalMeshSceneProxy                                    
        //              │                      ├─────────────┼─────────────┼────────────┬────────────┼────────────┬────────────┐                                                
        //              │          Primitives: │ SceneInfo6  │ SceneInfo7  │ SceneInfo4 │ SceneInfo5 │ SceneInfo2 │ SceneInfo3 │                                                
        //              │                      ├─────────────┼─────────────┼────────────┴────────────┼────────────┴────────────┘                                                
        //              │                      │   ▲         │          ▲  │                   ▲     │                                                                          
        //              │                      │   │         │          │  │                   │     │                                                                          
        //              │                   StaticMeshSceneProxy    SkeletalMeshSceneProxy   PrimitiveSceneProxy                                                                
        //              │                                                                                                                                                       
        //              │                                                                                                                                                       
        //              │                      ┌─────────────┬─────────────┬────────────┬────────────┬────────────┬────────────┐                                                
        //              │ PrimitiveTransforms: │ Transform6  │ Transform7  │ Transform4 │ Transform5 │ Transform2 │ Transform3 │                                                
        //              │                      └─────────────┴─────────────┴────────────┴────────────┴────────────┴────────────┘                                                
        //              │                                                                                                                                                       
        //              │                                                                                                                                                       
        //              │                                                                                                                                                       
        //              ├────2-2. Find the entry by 'FScene::TypeOffsetTable'                                                                                                   
        //              │          │                                                                                                                                            
        //              │          │                                                                                                                                            
        //              │          ├────Successfully find the TypeOffsetTableEntry in the index [1]:                                                                            
        //              │          │                                                                                                                                            
        //              │          │     BroadIndex = 1:                                                                                                                        
        //              │          │ ────────────────────                                                                                                                       
        //              │          │      ***                                                                                                                                   
        //              │          │                                                                                                                                            
        //              │          └────TypeOffsetTable looks like this:                                                                                                        
        //              │                                                                                                                                                       
        //              │                                   ┌─────────────────────────┬────────────────────────────┬─────────────────────────┐                                  
        //              │                  TypeOffsetTable: │(StaticMeshSceneProxy, 1)│ (SkeletalMeshSceneProxy, 2)│ (PrimitiveSceneProxy, 4)│                                  
        //              │                                   └─────────────────────────┴────────────────────────────┴─────────────────────────┘                                  
        //              │                                                                                                                                                       
        //              │                                                                                                                                                       
        //              └────2.3. Swap the SOA of the scene:                                                                                                                    
        //                         │                                                                                                                                            
        //                         │                     ┌────────────┬────────────┐                                                                                            
        //                         └───Iterate block(2): │ SceneInfo2 │ SceneInfo3 │                                                                                            
        //                               │               └────────────┴────────────┘                                                                                            
        //                               │  SourceIndex:     4             5                                                                                                    
        //                               │                   │             │                                                                                                    
        //                               │                   │             └──────────────────────────────────────────────────────┐                                             
        //                               │                   │                                                                    │                                             
        //                               │                   └────────────────────────────────────────────────────────┐           │                                             
        //                               │                                                                            │           │                                             
        //                               │               ┌─────────────┬─────────────┬────────────┬────────────┬──────▼─────┬─────▼──────┐                                      
        //                               │   Primitives: │ SceneInfo6  │ SceneInfo7  │ SceneInfo4 │ SceneInfo5 │ SceneInfo2 │ SceneInfo3 │                                      
        //                               │               └─────────────┴─────────────┴────────────┴────────────┴────────────┴────────────┘                                      
        //                               │                    [0]           [1]            [2]          [3]          [4]         [5]                                            
        //                               │                                                                                                                                      
        //                               │                                                                                                                                      
        //                               ├────Iterate [BroadIndex, TypeOffsetTable.Num()] as TypeIndex                                                                          
        //                               │    : BroadIndex == 1: (SkeletalMeshSceneProxy, 2) == TypeOffsetTable[1]                                                              
        //                               │      │                                                                                                                               
        //                               │      │                                                                                                                               
        //                               │      ├───TypeIndex == 1: (SkeletalMeshSceneProxy, 2) == TypeOffsetTable[1]                                                           
        //                               │      │   Increase TypeOffsetTable[TypeIndex].Offset                                                                                  
        //                               │      │    :DestIndex = TypeOffsetTable[TypeIndex].Offset++  ───► (SkeletalMeshSceneProxy, 3)                                         
        //                               │      │      │                                                                                                                        
        //                               │      │      │                                                                                                                        
        //                               │      │      └───Swap the SOA of the scene only if DestIndex and SourceIndex is different:                                            
        //                               │      │           │                                                                                                                   
        //                               │      │           │                                                                                                                   
        //                               │      │           └───In our case:                                                                                                    
        //                               │      │               │                                                                                                               
        //                               │      │               └────SourceIndex: 4, DestIndex: 2 : SourceIndex != DestIndex                                                    
        //                               │      │                                     ┌─────────────┬─────────────┬────────────┬────────────┬────────────┬────────────┐         
        //                               │      │                         Primitives: │ SceneInfo6  │ SceneInfo7  │ SceneInfo2 │ SceneInfo5 │ SceneInfo4 │ SceneInfo3 │         
        //                               │      │                                     └─────────────┴─────────────┴──────▲─────┴────────────┴─────▲──────┴────────────┘         
        //                               │      │                                                                        │                        │                             
        //                               │      │                                                                        └────────────────────────┘                             
        //                               │      │                                                                                                                               
        //                               │      │                                                                                                                               
        //                               │      └───TypeIndex == 2: (PrimitiveSceneProxy, 4) == TypeOffsetTable[2]                                                              
        //                               │          Increase TypeOffsetTable[TypeIndex].Offset                                                                                  
        //                               │           :DestIndex = TypeOffsetTable[TypeIndex].Offset++ ───► (PrimitiveSceneProxy, 5)                                             
        //                               │                                                                                                                                      
        //                               │                                                                                                                                      
        //                               │                                                                                                                                      
        //                               │                                                                                                                                      
        //                               └────Iterate [BroadIndex, TypeOffsetTable.Num()] as TypeIndex                                                                          
        //                                    : BroadIndex == 1: (SkeletalMeshSceneProxy, 3) == TypeOffsetTable[1]                                                              
        //                                      │                                                                                                                               
        //                                      │                                                                                                                               
        //                                      ├───TypeIndex == 1: (SkeletalMeshSceneProxy, 3) == TypeOffsetTable[1]                                                           
        //                                      │   Increase TypeOffsetTable[TypeIndex].Offset                                                                                  
        //                                      │    :DestIndex = TypeOffsetTable[TypeIndex].Offset++  ───► (SkeletalMeshSceneProxy, 4)                                         
        //                                      │      │                                                                                                                        
        //                                      │      │                                                                                                                        
        //                                      │      └───Swap the SOA of the scene only if DestIndex and SourceIndex is different:                                            
        //                                      │           │                                                                                                                   
        //                                      │           │                                                                                                                   
        //                                      │           └───In our case:                                                                                                    
        //                                      │               │                                                                                                               
        //                                      │               └────SourceIndex: 5, DestIndex: 3 : SourceIndex != DestIndex                                                    
        //                                      │                                     ┌─────────────┬─────────────┬────────────┬────────────┬────────────┬────────────┐         
        //                                      │                         Primitives: │ SceneInfo6  │ SceneInfo7  │ SceneInfo2 │ SceneInfo3 │ SceneInfo4 │ SceneInfo5 │         
        //                                      │                                     └─────────────┴─────────────┴────────────┴─────▲──────┴────────────┴──────▲─────┘         
        //                                      │                                                                                    │                          │               
        //                                      │                                                                                    └──────────────────────────┘               
        //                                      │                                                                                                                               
        //                                      │                                                                                                                               
        //                                      └───TypeIndex == 2: (PrimitiveSceneProxy, 5) == TypeOffsetTable[2]                                                              
        //                                          Increase TypeOffsetTable[TypeIndex].Offset                                                                                  
        //                                           :DestIndex = TypeOffsetTable[TypeIndex].Offset++ ───► (PrimitiveSceneProxy, 6)                                             
        //                                                                                                                                                                      
        //                                                                                                                                                                      
        //                                                                                                                                                                      
        //                                                                                                                                                                      
        //                  ┌─────────────────────────────────────────────────────────────────────────────────────────────────────────────┐                                     
        //                  │      *** Final Result:                                                                                      │                                     
        //                  │                                                                                                             │                                     
        //                  │                                                                                                             │                                     
        //                  │                      StaticMeshSceneProxy   SkeletalMeshSceneProxy                  PrimitiveSceneProxy     │                                     
        //                  │                           │                         │                                  │                    │                                     
        //                  │                           │      │                  │                    │             │           │        │                                     
        //                  │                           ▼      │                  ▼                    │             ▼           │        │                                     
        //                  │                    ┌─────────────┼─────────────┬────────────┬────────────┼────────────┬────────────┤        │                                     
        //                  │        Primitives: │ SceneInfo6  │ SceneInfo7  │ SceneInfo2 │ SceneInfo3 │ SceneInfo4 │ SceneInfo5 │        │                                     
        //                  │                    └─────────────┼─────────────┴────────────┴────────────┼────────────┴────────────┤        │                                     
        //                  │                                  │                                       │                         │        │                                     
        //                  │                                                                                                             │                                     
        //                  │                                                                                                             │                                     
        //                  │                    ┌─────────────────────────┬────────────────────────────┬─────────────────────────┐       │                                     
        //                  │   TypeOffsetTable: │(StaticMeshSceneProxy, 1)│ (SkeletalMeshSceneProxy, 4)│ (PrimitiveSceneProxy, 6)│       │                                     
        //                  │                    └─────────────────────────┴────────────────────────────┴─────────────────────────┘       │                                     
        //                  │                                                                                                             │                                     
        //                  │                                                                                                             │                                     
        //                  └─────────────────────────────────────────────────────────────────────────────────────────────────────────────┘                                     
        //   
        // haker: the conceptual overview is a little bit complicated, but you are now ready to understand the below code!
        // - I hope you to help your understanding the below logic with the diagram~ :)                                                                                                                                                           
        while (AddedLocalPrimitiveSceneInfos.Num())
        {
            // haker: 1. as we said, it starts the end of block which has the same proxy type(e.g. SkeletalMeshSceneProxy) in 'AddedLocalPrimitiveSceneInfos'
            int32 StartIndex = AddedLocalPrimitiveSceneInfos.Num() - 1;
            SIZE_T InsertProxyHash = AddedPrimitiveSceneInfos[StartIndex]->Proxy->GetTypeHash();
            while (StartIndex > 0 && AddedLocalPrimitiveSceneInfos[StartIndex - 1]->Proxy->GetTypeHash() == InsertProxyHash)
            {
                StartIndex--;
            }

            // haker: 2-1. we add all SceneInfos in the block which has same proxy type at the end of SOA of the scene
            {
                for (int32 AddIndex = StartIndex; AddIndex < AddedLocalPrimitiveSceneInfos.Num(); ++AddIndex)
                {
                    FPrimitiveSceneInfo* PrimitiveSceneInfo = AddedLocalPrimitiveSceneInfos[AddIndex];
                    Primitives.Add(PrimitiveSceneInfo);

                    const FMatrix LocalToWorld = PrimitiveSceneInfo->Proxy->GetLocalToWorld();
                    PrimitiveTransforms.Add(LocalToWorld);

                    PrimitiveSceneProxies.Add(PrimitiveSceneInfo->Proxy);
                    PrimitiveBounds.AddUninitialized();
                    
                    //...

                    // haker: note that we cached our current index of the SOA in the FPrimitiveSceneInfo::PackedIndex
                    const int32 SourceIndex = PrimitiveSceneProxies.Num() - 1;
                    PrimitiveSceneInfo->PackedIndex = SourceIndex;

                    //...
                }
            }

            // haker: 2-2. we try to find TypeOffsetTableEntry matching proxy type 
            // - 'BroadIndex' is the index of TypeOffsetTable
            bool EntryFound = false;
            int32 BroadIndex = -1;
            {
                // broad phase search for a matching type
                for (BroadIndex = TypeOffsetTable.Num() - 1; BroadIndex >= 0; BroadIndex--)
                {
                    // example how the prefix sum of the tails could look like:
                    // haker: we already covered the simple example to understand how the overall logic works
                    // - let's deal with the comments of UnrealEngine
                    // PrimitiveSceneProxies[0,0,0,6,6,6,6,6,2,2,2,2,1,1,1,7,4,8]
                    // TypeOffsetTable[3,8,12,15,16,17,18] 

                    // haker: as we saw that 'ProxyHash' is the type-id of SceneProxy
                    if (TypeOffsetTable[BroadIndex].PrimitiveSceneProxyType == InsertProxyHash)
                    {
                        EntryFound = true;
                        break;
                    }
                }

                // new type encountered
                // haker: if we failed to find 'EntryFound', we add 'new entry'
                if (EntryFound == false)
                {
                    BroadIndex = TypeOffsetTable.Num();
                    if (BroadIndex)
                    {
                        // haker: we get the last TypeOffset entry and get the 'offset' and using it, create new entry
                        FTypeOffsetTableEntry Entry = TypeOffsetTable[BroadIndex - 1];

                        // adding to the end of the list and offset of the tail (will be incremented once during the while loop)
                        TypeOffsetTable.Push(FTypeOffsetTableEntry(InsertProxyHash, Entry.Offset));
                    }
                    else
                    {
                        // starting with an empty list and offset zero (will be incremented once during during the while loop)
                        TypeOffsetTable.Push(FTypeOffsetTableEntry(InsertProxyHash, 0));
                    }
                }
            }

            // swap PrimitiveSceneInfos
            // haker: 2-3. swap the SOA of the scene
            {
                for (int32 AddIndex = StartIndex; AddIndex < AddedLocalPrimitiveSceneInfos.Num(); AddIndex++)
                {
                    // haker: PackedIndex already is cached when adding it at the end of 'AddedLocalPrimitiveSceneInfos' in 2-1
                    int32 SourceIndex = AddedLocalPrimitiveSceneInfos[AddIndex]->PackedIndex;

                    // haker: 'BroadIndex' is calculated at the phase 2-2
                    for (int32 TypeIndex = BroadIndex; TypeIndex < TypeOffsetTable.Num(); TypeIndex++)
                    {
                        FTypeOffsetTableEntry& NextEntry = TypeOffsetTable[TypeIndex];
                        int32 DestIndex = NextEntry.Offset++;

                        // example swap chain of inserting a type of 6 at the end
                        // 1. PrimitiveSceneProxies[0,0,0,6,6,6,6,6,2,2,2,2,1,1,1,7,4,8,[6]]
                        // - TypeOffsetTable[3,8,12,15,16,17,18] 

                        // 2. PrimitiveSceneProxies[0,0,0,6,6,6,6,6,[6],2,2,2,1,1,1,7,4,8,[2]]
                        // - TypeOffsetTable[3,[9],12,15,16,17,18] 

                        // 3. PrimitiveSceneProxies[0,0,0,6,6,6,6,6,6,2,2,2,[2],1,1,7,4,8,[1]]
                        // - TypeOffsetTable[3,9,[13],15,16,17,18] 

                        // 4. PrimitiveSceneProxies[0,0,0,6,6,6,6,6,6,2,2,2,2,1,1,[1],4,8,[7]]
                        // - TypeOffsetTable[3,9,13,[16],16,17,18] 

                        // 5. PrimitiveSceneProxies[0,0,0,6,6,6,6,6,6,2,2,2,2,1,1,1,[7],8,[4]]
                        // - TypeOffsetTable[3,9,13,16,[17],17,18] 

                        // 6. PrimitiveSceneProxies[0,0,0,6,6,6,6,6,6,2,2,2,2,1,1,1,7,[4],[8]]
                        // - TypeOffsetTable[3,9,13,16,17,[18],18] 

                        // 7. no swap happens(DestIndex != SourceIndex), but TypeOffsetTableEntry's Offset is updated
                        // - TypeOffsetTable[3,9,13,16,17,18,[19]]

                        if (DestIndex != SourceIndex)
                        {
                            Primitives[DestIndex]->PackedIndex = SourceIndex;
                            Primitives[SourceIndex]->PackedIndex = DestIndex;

                            TArraySwapElements(Primitives, DestIndex, SourceIndex);
							TArraySwapElements(PrimitiveTransforms, DestIndex, SourceIndex);
							TArraySwapElements(PrimitiveSceneProxies, DestIndex, SourceIndex);
							TArraySwapElements(PrimitiveBounds, DestIndex, SourceIndex);

                            //...
                        }
                    }
                }
            }

            // haker: the SOA order in the scene is updated, now it is time to initialize(or update) data of SOA in the scene e.g. Bounds, MeshDrawCommand, ...
            for (int AddIndex = StartIndex; AddIndex < AddedLocalPrimitiveSceneInfos.Num(); AddIndex++)
            {
                FPrimitiveSceneInfo* PrimitiveSceneInfo = AddedLocalPrimitiveSceneInfos[AddIndex];
                int32 PrimitiveIndex = PrimitiveSceneInfo->PackedIndex;

                // haker: create UniformBuffer for each SceneProxy
                FPrimitiveSceneProxy* SceneProxy = PrimitiveSceneInfo->Proxy;
                SceneProxy->CreateUniformBuffer();

                //...

                // haker: we queue the newly added and sorted SceneInfo:
                // 1. see QueueAddToScene
                // 2. see QueueAddStaticMeshes
                QueueAddToScene(PrimitiveSceneInfo);
                QueueAddStaticMeshes(PrimitiveSceneInfo);
            }

            //...

            // haker: we successfully add all SceneInfo in the block, so remove elements in the block
            // - we're ready to iterate next block
            AddedLocalPrimitiveSceneInfos.RemoveAt(Start, AddedLocalPrimitiveSceneInfos.Num() - StartIndex, false);

        } // END: while (AddedLocalPrimitiveSceneInfos.Num())

        //...

        // haker: update accumulated 'FUpdateTransformCommand's in the 'UpdatedTransforms'
        {
            for (const auto& Transform : UpdatedTransforms)
            {
                FPrimitiveSceneProxy* PrimitiveSceneProxy = Transform.Key;
                if (DeletedSceneInfos.Contains(PrimitiveSceneProxy->GetPrimitiveSceneInfo()))
                {
                    continue;
                }

                const FBoxSphereBounds& WorldBounds = Transform.Value.WorldBounds;
                const FBoxSphereBounds& LocalBounds = Transform.Value.LocalBounds;
                const FMatrix& LocalToWorld = Transform.Value.LocalToWorld;
                const FVector& AttachmentRootPosition = Transform.Value.AttachmentRootPosition;
                
                //...

                // update the primitive transform
                // haker: we already had seen FPrimitiveSceneProxy::SetTransform()
                // - rather removing and adding FPrimitiveSceneInfo to update transform, this is a way more cheaper as performance
                PrimitiveSceneProxy->SetTransform(LocalToWorld, WorldBounds, LocalBounds, AttachmentRootPosition);
                PrimitiveTransforms[PrimitiveSceneInfo->PackedIndex] = LocalToWorld;

                //...
            }
        }

        //...

        // haker: we already accumulated SceneInfosWithAddToScene after finishing to add and sort new SceneInfos into the SOA of the scene
        // - see FPrimitiveSceneInfo::AddToScene (goto 040: SceneRenderer)
        if (SceneInfosWithAddToScene.Num() > 0)
        {
            FPrimitiveSceneInfo::AddToScene(this, SceneInfosWithAddToScene);
        }

        //...

        // haker: we are not covering MeshDrawCommand in detail, but let's briefly understand it conceptually with the diagram:
        //
        //                                                                                                              ┌────────────────────────────┐                                                          
        //                                                                                   ***                        │                            │                                                          
        //                                                                       ───────────────────────────────────────┴──                          │                                                          
        //                                                SkeletalMeshComponent: SkeletalMeshSceneProxy(PrimitiveSceneInfo)                          │                                                          
        //                Root                                                                                                                       │                                                          
        //                 ▲                                 ┌──┬──┬──┬──┬──┬──┬──┬──┬──┬──┬──┬──┬──┬──┬──┬──┬──┬──┬──┬──┬──┬──┐                     │                                                          
        //                 │                  Vertex Buffer: │ 0│ 1│ 2│ 3│ 4│ 5│ 6│ 7│ 8│ 9│10│11│12│13│14│15│16│17│18│19│20│21│                     │                                                          
        //                Pelvis                             ├──┴──┴──┴──┴──┴──┼──┴──┴──┴──┼──┴──┴──┴──┴──┼──┴──┴──┴──┼──┴──┴──┤                     │                                                          
        //                 ▲                   Index Buffer: │   [0,5]         │  [6,9]    │   [10,14]    │  [15,18]  │ [19,21]│                     │                                                          
        //                 │                                 │◄───────────────►│◄─────────►│◄────────────►│◄─────────►│◄──────►│                     │                                                          
        //       ┌─────────┼─────────┐                       │   R_Leg         │  L_Leg    │   Head       │  R_Arm    │ R_Hand │                     │                                                          
        //       │         │         │                           +Mat0            +Mat1        +Mat2         +Mat3      +Mat4                        │                                                          
        //      L_Leg     Spine     R_Leg                          │                │            │             │            │                        │ Primitive can be splited into multiple MeshDrawCommands  
        //       ▲          ▲        ▲                             │                │            │             │            │                        │ ──────────────────────────────────────────────────────── 
        //       │          │        │                             ▼                ▼            │             ▼            │                        │           ***                                            
        //     L_Foot       │       R_Foot             MeshDrawCommand0      MeshDrawCommand1    │          MeshDrawCommand3│                        │            │                                             
        //             ┌────┼──────┐                                                             ▼                          ▼                        │            │                                             
        //             │    │      │                                                         MeshDrawCommand2             MeshDrawCommand4           │            │ How MeshDrawCommand consists of?            
        //             │    │      │                                                                                                                 │            │ : MeshDrawCommand is the cache state unit of rendering in GPU                                            
        //           L_Arm  Head  R_Arm                                                                                                              │            │   ──────────────────────────────────────────────────────────                                          
        //             ▲            ▲                                                                                                                │            ▼                     ****                        
        //             │            │                                                                                                                │         MeshDrawCommand                                  
        //          L_Hand        R_Hand                     ┌────────────────┬────────────────┬────────────────┬────────────────┬────────────────┐  │          │                                               
        //                                                   │MeshDrawCommand0│MeshDrawCommand1│MeshDrawCommand2│MeshDrawCommand3│MeshDrawCommand4│◄─┘          ├──PipelineStateObject(PSO)                     
        //                                                   └────────────────┴────────────────┴────────────────┴────────────────┴────────────────┘             │   │                                           
        //                                                                                                                                                      │   ├──VertexShader                             
        //                                                                                                                                                      │   │                                           
        //                                                                                                                                                      │   ├──PixelShader                              
        //                                                                                                                                                      │   │                                           
        //                                                                                                                                                      │   └──RasterizedState                          
        //                                                                                                                                                      │                                               
        //                                                                                                                                                      └──Vertex Buffer + Index Buffer                 
        //                                                                                                                                                                                                      
        if (SceneInfosWithStaticDrawListUpdate.Num() > 0)
        {
            CacheMeshDrawCommandsTask = GraphBuilder.AddSetupTask([this, &SceneInfosWithStaticDrawListUpdate, bLaunchAsyncTask]()
            {
                // haker: CacheMeshDrawCommands
                FPrimitiveSceneInfo::CacheMeshDrawCommands(this, SceneInfosWithStaticDrawListUpdate);
            }, IsMobilePlatform(GetShaderPlatform()) ? CreateLightPrimitiveInteractionsTask : UE::Tasks::FTask(), UE::Tasks::ETaskPriority::High, bLaunchAsyncTask);

            //...
        }

        AddedPrimitiveSceneInfos.Empty();
    }

    struct FUpdateTransformCommand
	{
		FBoxSphereBounds WorldBounds;
		FBoxSphereBounds LocalBounds; 
		FMatrix LocalToWorld; 
		FVector AttachmentRootPosition;
	};

    Experimental::TRobinHoodHashMap<FPrimitiveSceneProxy*, FUpdateTransformCommand> UpdatedTransforms;

    // 011 - Foundation - CreateRenderState * - FScene's member variables

    struct FTypeOffsetTableEntry
    {
        SIZE_T PrimitiveSceneProxyType;

        /** e.g. prefix sum where the next type starts */
        uint32 Offset;
    };

    /** during insertion and deletion, used to skip large chunks of items of the same type */
    // haker: we'll see what 'FTypeOffsetTableEntry' for:
    // - for now, just briefly understand what it means conceptually:
    //  0               4                          11                                          21    
    //  │               │                           │                                           │    
    //  │               │                           │                                           │    
    //  │               │                           │                                           │    
    //  ├───┬───┬───┬───┼───┬───┬───┬───┬───┬───┬───┼───┬───┬───┬───┬───┬───┬───┬───┬───┬───┬───┼───┐
    //  │ 0 │ 1 │ 2 │ 3 │ 4 │ 5 │ 6 │ 7 │ 8 │ 9 │ 10│ 11│ 12│ 13│ 14│ 15│ 16│ 17│ 18│ 19│ 20│ 21│...│
    //  ├───┴───┴───┴───┼───┴───┴───┴───┴───┴───┴───┼───┴───┴───┴───┴───┴───┴───┴───┴───┴───┴───┼───┘
    //  │               │                           │                                           │    
    //  │               │                           │                                           │    
    //  │◄─────────────►│◄─────────────────────────►│◄─────────────────────────────────────────►│    
    //  │               │                           │                                           │    
    //                                                                                               
    // StaticMeshSceneProxy    PrimitiveSceneProxy           SkeletalMeshSceneProxy                   
    // :Offset==4              :Offset==11                   :Offset==21                              
    //
    TArray<FTypeOffsetTableEntry> TypeOffsetTable;

    // haker: pending PrimitiveSceneInfo to add into 'Primitives'
    TRobinHoodHashSet<FPrimitiveSceneInfo*> AddedPrimitiveSceneInfos;

    /**
     * following arrays are densely packed primitive data needed by various render passes
     * PrimitiveSceneInfo->PackedIndex maintains the index where data is stored in these arrays for a given primitive 
     */

    // haker: render-world manages objects' properties as 'SOA: Structure of Arrays'
    // - let's understand it with the diagram:
    //                          ┌─────────────┬─────────────┬─────────────┬─────────────┬─────────────┬─────────────┬──────────┐ 
    //              Primitives: │ SceneInfo0  │ SceneInfo1  │ SceneInfo2  │ SceneInfo3  │ SceneInfo4  │ SceneInfo5  │ ...      │ 
    //                          └─────────────┴─────────────┴─────────────┴─────────────┴─────────────┴─────────────┴──────────┘ 
    //                                                                                                                           
    //                          ┌─────────────┬─────────────┬─────────────┬─────────────┬─────────────┬─────────────┬──────────┐ 
    //     PrimitiveTransforms: │ Transform0  │ Transform1  │ Transform2  │ Transform3  │ Transform4  │ Transform5  │ ...      │ 
    //                          └─────────────┴─────────────┴─────────────┴─────────────┴─────────────┴─────────────┴──────────┘ 
    //                                                                                                                           
    //                          ┌─────────────┬─────────────┬─────────────┬─────────────┬─────────────┬─────────────┬──────────┐ 
    //   PrimitiveSceneProxies: │ SceneProxy0 │ SceneProxy1 │ SceneProxy2 │ SceneProxy3 │ SceneProxy4 │ SceneProxy5 │ ...      │ 
    //                          └─────────────┴─────────────┴─────────────┴─────────────┴─────────────┴─────────────┴──────────┘ 
    //                                                                                                                           
    //                          ┌─────────────┬─────────────┬─────────────┬─────────────┬─────────────┬─────────────┬──────────┐ 
    //         PrimitiveBounds: │ Bounds0     │ Bounds1     │ Bounds2     │ Bounds3     │ Bounds4     │ Bounds5     │ ...      │ 
    //                          └─────────────┴─────────────┴─────────────┴─────────────┴─────────────┴─────────────┴──────────┘ 
    //
    // - try to remember the difference of how to manage objects in world and render-world(scene)
    // - whenever adding new object in render-world(scene), we need to sort them by proxy type using TypeOffsetTable
    //   - the detail will be covered in FScene::UpdateAllPrimitiveSceneInfos()
    /** packed array of primitives in the scene */
    TArray<FPrimitiveSceneInfo*> Primitives;

    /** packed array of all transforms in the scene */
    TScenePrimitiveArray<FMatrix> PrimitiveTransforms;

    /** packed array of primitive scene proxies in the scene */
    TArray<FPrimitiveSceneProxy*> PrimitiveSceneProxies;

    /** Packed array of primitive bounds. */
    TScenePrimitiveArray<FPrimitiveBounds> PrimitiveBounds;

    //...
};