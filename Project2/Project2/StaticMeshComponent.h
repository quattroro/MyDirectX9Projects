
/** MeshComponent is an abstract base for any component that is an instance of a renderable collection of triangle */
class UMeshComponent : public UPrimitiveComponent
{

};

/** a static mesh component scene proxy */
class FStaticMeshSceneProxy : public FPrimitiveSceneProxy
{
    // 039 - Foundation - SceneRenderer * - FStaticMeshSceneProxy::GetTypeHash
    virtual SIZE_T GetTypeHash() const override
    {
        // haker: UniquePointer is static variable which means it is global variable
        // - returning global variable's pointer means GetTypeHash() has unique value for each type:
        //   - FStaticMeshSceneProxy's GetTypeHash returns unique value
        //   - FSkeletalMeshSceneProxy's GetTypeHash returns unique value
        // - it is unique identifier for PrimitiveSceneProxy types
        static size_t UniquePointer;
        return reinterpret_cast<size_t>(&UniquePointer);
    }
};

/**
 * StaticMeshComponent is used to create an instance of a UStaticMesh
 * A static mesh is a piece of geometry that consists of a static set of polygons 
 */
class UStaticMeshComponent : public UMeshComponent
{
    /** overload this in child implementation that wish to extend StaticMesh or Nanite scene proxy implementations */
    // haker: UStaticMeshComponent, UInstancedStaticMeshComponent, UHierarcicalInstancedStaticMeshComponent shares this method to override SceneProxy creation
    virtual FPrimitiveSceneProxy* CreateStaticMeshSceneProxy(Nanite::FMaterialAudit& NaniteMaterials, bool bCreateNanite)
    {
        if (bCreateNanite)
        {
            return ::new Nanite::FSceneProxy(NaniteMaterials, this);
        }

        // haker: allocate 'new' StaticMeshSceneProxy
        // from here, we can know that:
        // - "SceneProxy" is created in 'GameThread'
        // - the ownership of creating SceneProxy is 'GameThread'
        // - SceneProxy and PrimitiveComponent has one-to-one relationship (1:1)
        auto* Proxy = ::new FStaticMeshSceneProxy(this, false);
        return Proxy;
    }

    // 012 - Foundation - CreateRenderState * - UStaticMeshComponent::CreateSceneProxy
    virtual FPrimitiveSceneProxy* CreateSceneProxy() override
    {
        //...

        // haker: if you enable Nanite, it has Nanite's own proxy, but for simplicity we stick to original StaticMeshSceneProxy
        Nanite::FMaterialAudit NaniteMaterials{};

        //...

        // haker: see CreateStaticMeshSceneProxy
        return CreateStaticMeshSceneProxy(NaniteMaterials, false);
    }

    /** the static mesh that this component uses to render */
    TObjectPtr<class UStaticMesh> StaticMesh;
};