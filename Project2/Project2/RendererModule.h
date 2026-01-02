/** public interface into FSceneRenderer. Used as the scope for scene rendering functions */
class ISceneRenderer
{

};

class FSceneRendererBase : public ISceneRenderer
{

};

/**
 * used as the scope for scene rendering functions
 * it is initialized in the game thread by FSceneViewFamily::BeginRender, and then passed to the rendering thread
 * the rendering thread calls Render(), and deletes the scenerenderer when it returns 
 */
// haker: we can think of 'SceneRenderer' as 'Scene + Renderer'
// - SceneRenderer is the renderer to render the scene
// - SceneRenderer is created every frame:
//   - SceneRenderer is a render-command for rendering the scene
class FSceneRenderer : public FSceneRendererBase
{
    /** creates multiple scene renderers based on the current feature level: all view families must point to the same scene */
    // 035 - Foundation - SceneRenderer * - FSceneRenderer::CreateSceneRenderer
    // haker: we create each SceneRenderer per SceneViewFamilys
    static void CreateSceneRenderers(TArrayView<const FSceneViewFamily*> InViewFamilies, FHitProxyConsumer* HitProxyConsumer, TArray<FSceneRenderer*>& OutSceneRenderers)
    {
        OutSceneRenderers.Empty(InViewFamilies.Num());
        if (InViewFamilies.Num())
        {
            return;
        }

        const FSceneInterface* Scene = InViewFamilies[0]->Scene;
        check(Scene);

        for (int32 FamilyIndex = 0; FamilyIndex < InViewFamilies.Num(); ++FamilyIndex)
        {
            const FSceneViewFamily* InViewFamily = InViewFamilies[FamilyIndex];

            // haker: actually I abbreviate the code a lot!
            // - I leave the code for creating DeferredShadingSceneRenderer
            FDeferredShadingSceneRenderer* SceneRenderer = new FDeferredShadingSceneRenderer(InViewFamily, HitProxyConsumer);
            OutSceneRenderers.Add(SceneRenderer);
        }
    }

    /** the scene being rendered */
    FScene* Scene;
};

/** scene renderer that implements a deferred shading pipeline and associated features */
// 034 - Foundation - SceneRenderer * - FDeferredShadingSceneRenderer
// haker: see FSceneRenderer
// - DeferredShadingSceneRenderer is the renderer supporting 'deferred-shading'
// - e.g. we have another type of SceneRenderer, 'MobileShadingSceneRenderer'
class FDeferredShadingSceneRenderer : public FSceneRenderer
{
    struct FInitViewTaskDatas
    {

    };

    IVisibilityTaskData UpdateScene(FRDGBuilder& GraphBuilder, FGlobalDynamicBuffers GlobalDynamicBuffers)
    {
        // haker: FScene::UpdateAllPrimitiveSceneInfos (goto 037: SceneRenderer)
        Scene->UpdateAllPrimitiveSceneInfos(GraphBuilder, GetUpdateAllPrimitiveSceneInfosSyncOps());

        //...
    }

    /** render the view family */
    // 036 - Foundation - SceneRenderer * - FDeferredShadingSceneRenderer::Render()
    virtual void Render(FRDGBuilder& GraphBuilder) override
    {
        // haker: see FDeferredShadingSceneRenderer::UpdateScene
        FInitViewTaskDatas InitViewTaskDatas = UpdateScene(GraphBuilder, FGlobalDynamicBuffers(DynamicIndexBufferForInitViews, DynamicVertexBufferForInitViews, DynamicReadBufferForInitViews));

        //...
    }
};

/** helper function performing actual work in render thread */
static void RenderViewFamilies_RenderThread(FRHICommandListImmediate& RHICmdList, const TArray<FSceneRenderer*>& SceneRenderers)
{
    // all renderers point to the same Scene
    // haker: all SceneRenderers should share same Scene(render-world)
    FScene* const Scene = SceneRenderer[0]->Scene;

    for (FSceneRenderer* SceneRenderer : SceneRenderer)
    {
        const ERHIFeatureLevel::Type FeatureLevel = SceneRenderer->FeatureLevel;

        FSceneViewFamily& ViewFamily = SceneRenderer->ViewFamily;

        // haker: 'RDG', 'Render Dependency Graph' is another big topic in renderering, it is out-of-scope
        // - I'll explain RDG conceptually with the diagram:
        //                                                                                                                                                                             
        //     RDG(Render-Dependency-Graph)                                                                                                                                            
        //                                                                                                                                                                             
        //       1. Why do we need 'RDG' in RenderThread?                                                                                                                              
        //                                                                                                                                                                             
        //           Render Thread───────x──────────────────────────────────────────────►                                                                                              
        //                               │                                                                                                                                             
        //                               │                                                                                                                                             
        //                               │ RDG Execution: RDG is the representative of how to send RHICommand in a form of dependency-graph                                            
        //                               │ ─────────────                                                                                                                               
        //                               │    ***                                                                                                                                      
        //                               ▼                                                                                                                                             
        //              RHI Thread───────x──────────────────────────────────────────────►                                                                                              
        //                                                                                                                                                                             
        //                                                                                                                                                                             
        //       2. What is RHICommand?                                                                                                                                                
        //                                                                                                                                                                             
        //           - RenderCommand is the command to execute in RenderThread                                                                                                         
        //           - RHICommand is the command to execute in RHIThread                                                                                                               
        //                                                                                                                                                                             
        //                            │                                                                                                                                                
        //                            │ What is difference between RenderCommand and RHICommand?                                                                                       
        //                            │                                                                                                                                                
        //                            ▼                                                                                                                                                
        //                  *** To get this answer, we need to know what is Command-List and Command-List Recording                                                                    
        //                                                          ───────────────────────────────────────────────                                                                    
        //                                                                      ***                                                                                                    
        //                                                                                                                                                                             
        //                                                      - What is 'Command-List'?                                                                                              
        //                                                                                                                                                                             
        //                                                          [Traditional]                                                                                                      
        //                                                                           │                                           │                                                     
        //                                                               <User>      │     <GDI(DirectX,OpenGL)>                 │   <GPU>                                             
        //                                                                           │                                           │                                                     
        //                                                                           │       ┌───────┐                           │                                                     
        //                                                         setIndexBuffer()──┼──────►│       │                           │   ┌───────────────────────┐                         
        //                                                                           │       │       │                           │   │                       │                         
        //                                                         setStream()───────┼──────►│       ├───────────────────────────┼──►│ CP(Command Processor) │                         
        //                                                                           │       │ Queue │ 1.GPU command generation  │   │                       │                         
        //                                                         draw()────────────┼──────►│       │ 2.enqueue command to CP   │   └───────────────────────┘                         
        //                                                                           │       │       │                           │                                                     
        //                                                         drawIndexed()─────┼──────►│       │                           │                                                     
        //                                                                           │       └───────┘                           │                                                     
        //                                                                           │                                           │                                                     
        //                                                                           │                                           │                                                     
        //                                                                                                                                                                             
        //                                                                                                                                                                             
        //                                                                                                                                                                             
        //                                                          [Current]                                                                                                          
        //                                                                                               <User>    │    <GDI>                      │  <GPU>                            
        //                                                                                                         │                               │                                   
        //                                                                                                         │                               │                                   
        //                                                                                                                                         │                                   
        //                                                         setIndexBuffer()──┐                   Graphics/Compute Queue                    │                                   
        //                                                                           │                         ┌────────┐                          │                                   
        //                                                         setStream()───────┤       ┌──────────────┐  │        │                          │                                   
        //                                                                           ├──────►│ CommandList0 ├──►        │                          │      ┌───────────────────────┐    
        //                                                         draw()────────────┤   ▲   └──────────────┘  │        │                          │      │                       │    
        //                                                                           │   │                     │        ├──────────────────────────┼──────► CP(Command Processor) │    
        //                                                         drawIndexed()─────┘   │                     │        │ 1.GPU command generation │      │                       │    
        //                                                                               │                     │        │ 2.enqueue command to CP  │      └───────────────────────┘    
        //                                                         setStream()───────┐   │                     │        │                          │                                   
        //                                                                           │   │   ┌──────────────┐  │        │                          │                                   
        //                                                         draw()────────────┼───┼──►│ CommandList1 ├──►        │                          │                                   
        //                                                                           │ ▲ │   └──────────────┘  └────────┘                          │                                   
        //                                                         drawIndexed()─────┘ │ │                                                         │                                   
        //                                                                             ├─┘                         │                               │                                   
        //                                                                             │                           │                               │                                   
        //                                                                             │                           │                               │                                   
        //                                                                             │                                                                                               
        //                                                                     ***Command List Recording***                                                                            
        //                                                                                                                                                                             
        //                                                                                                                                                                             
        //                                                                      - Command List is a collection of all commands to translate into GPU command (consumed in the GPU)     
        //                                                                      - Command List is the abstract unit of GPU Commands to processed in GDI                                
        //                                                                                                                                                                             
        //                                                                      - New API provides the freedom to manage low-level commands                                            
        //                                                                        *** we need to organize them in Command List                                                         
        //                                                                                                                                                                             
        //       3. What is RDG?                                                                                                                                                       
        //          : Depedency Graph to organize 'Command Lists' efficiently                                                                                                          
        //                                                                                                                                                                             
        //            RDG Root:                                                                                                                                                        
        //             │                                                                                                                                                               
        //             ├───Pass0 (Prepass)                                         Graphics Queue                                                                                      
        //             │    │                                                        ┌───────┐                                                                                         
        //             │    ├─────Draw Call 0 ───────┐                               │       │                                                                                         
        //             │    │                        ├────► CommandList0─────────────►       │                                                                                         
        //             │    └─────Draw Call 1 ───────┘                               │       │                                                                                         
        //             │                                                             │       │                                                                                         
        //             ├───Pass1 (Base-Pass)                                         │       │                                                                                         
        //             │    │                                                        │       │                                                                                         
        //             │    ├─────Draw Call 2 ───────┐                               │       │                                                                                         
        //             │    │                        ├────► CommandList1─────────────►       │                                                                                         
        //             │    ├─────Draw Call 3 ───────┘                               │       │                                                                                         
        //             │    │                                                        │       │                                                                                         
        //             │    └─────Draw Call 4 ────────────► CommandList2─────────────►       │                                                                                         
        //             │                                                             │       │                                                                                         
        //             └───Pass1 (Light-Pass)                                        │       │                                                                                         
        //                  │                                                        │       │                                                                                         
        //                  ├─────Draw Call 5 ───────┐                               │       │                                                                                         
        //                  │                        ├────► CommandList3─────────────►       │                                                                                         
        //                  └─────Draw Call 6 ───────┘                               │       │                                                                                         
        //                                                                           └───────┘                                                                                         
        //          *** RDG also gives efficiency to manage transition barriers                                                                                                        
        //                                                                                                                                                                             
        FRDGBuilder GraphBuilder(
            RHICmdList,
            RDG_EVENT_NAME("SceneRenderer_%s(ViewFamily=%s)"),
                ViewFamily.EngineShowFlags.HitProxies ? TEXT("RenderHitProxies") : TEXT("Render"),
                ViewFamily.bResolveScene ? TEXT("Primary") : TEXT("Auxiliary")
            ),
            FSceneRenderer::GetRDGParalelExecuteFlags(FeatureLevel)
        );
        
        // render the scene
        // haker: using DeferredShadingSceneRenderer, starts to render the scene
        // - see the GraphBuilder.Execute first
        // - see FDeferredShadingSceneRenderer::Render (goto 036: SceneRenderer)
        SceneRenderer->Render(GraphBuilder);

        GraphBuilder.Execute();
    }
}

class FRendererModule final : public IRendererModule
{
    virtual void BeginRenderingViewFamilies(FCanvas* Canvas, TArrayView<FSceneViewFamily*> ViewFamilies) override
    {
        // haker: we already have seen SendAllEndOfFrameUpdates()
        // - we'd like to make sure apply all changes from the world to scene(render-world) before rendering the scene
        // - note that we're still in Game-Thread
        UWorld* World = nullptr;
        FScene* const Scene = ViewFamilies[0]->Scene ? ViewFamilies[0]->Scene : nullptr;
        if (Scene)
        {
            World = Scene->GetWorld();
            if (World)
            {
                // guarantee that all render proxies are up-to-date before kicking off a BeginRenderViewFamily
                World->SendAllEndOfFrameUpdates();
            }
        }

        //...

        if (Scene)
        {
            // construct the scenerenderers: this copies the view family attributes into its own structures
            // haker: we firstly look into the classes:
            // - FDeferredShadingSceneRenderer (goto 034: SceneRenderer)
            TArray<FSceneRenderer*> SceneRenderers;

            TArray<const FSceneViewFamily*> ViewFamiliesConst;
            for (FSceneViewFamily* ViewFamily : ViewFamilies)
            {
                ViewFamilieisConst.Add(ViewFamily);
            }

            // haker: see FSceneRenderer::CreateSceneRenderers(goto 035: SceneRenderer)
            FSceneRenderer::CreateSceneRenderers(ViewFamiliesConst, Canvas->GetHitProxyConsumer(), SceneRenderers);

            // haker: now we create render-command, 'FDrawSceneCommand' with new SceneRenderers
            // - see RenderViewFamilies_RenderThread(goto 036: SceneRenderer)
            ENQUEUE_RENDER_COMMAND(FDrawSceneCommand)(
                [LocalSceneRenderers = CopyTemp(SceneRenderers)](FRHICommandListImmediate& RHICmdList)
                {
                    RenderViewFamilies_RenderThread(RHICmdList, LocalSceneRenderers);
                }
            );
        }
    }

    // 033 - Foundation - SceneRenderer * - BeginRenderingViewFamily
    virtual void BeginRenderingViewFamily(FCanvas* Canvas, FSceneViewFamily* ViewFamily) override
    {
        // haker: see BeginRenderingViewFamilies
        // - we make an array-view containing one SceneViewFamily
        BeginRenderingViewFamilies(Canvas, TArrayView<FSceneViewFamily*>(&ViewFamily, 1));
    }
};