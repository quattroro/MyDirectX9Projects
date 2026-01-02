class UPlayer : public UObject, public FExec
{
    /** the actor this player controls */
    // haker: UPlayer has PlayerController
    // - see APlayerController (goto 010: CreateInnerProcessPIEGameInstance)
    TObjectPtr<APlayerController> PlayerController;
};

/**
 * class used to reference an FSceneViewStateInterface that allows destruction and recreation of all FSceneViewStateInterface's when needed
 * this is used to support reloading the renderer module on the fly 
 */
// haker: from 'Allocate' method, SceneViewStateReference is managed class for FSceneViewInterface
class FSceneViewStateReference
{
    static TLinkedList<FSceneViewStateReference*>*& GetSceneViewStateList()
    {
        static TLinkedList<FSceneViewStateReference*>* List = NULL;
        return List;
    }

    void AllocateInternal(FRHIFeatureLevel::Type FeatureLevel)
    {
        // return "new FSceneViewState(FeatureLevel, nullptr);""
        Reference = GetRenderModule().AllocateViewState(FeatureLevel, nullptr);
    }

    // 031 - Foundation - SceneRenderer * - FSceneViewStateReference::Allocate
    void Allocate(FRHIFeatureLevel::Type FeatureLevel)
    {
        AllocateInternal(FeatureLevel);

        GlobalListLink = TLinkedList<FSceneViewStateReference*>(this);
        GlobalListLink.LinkHead(GetSceneViewStateList());
    }

    FSceneViewStateInterface* Reference;
    TLinkedList<FSceneViewStateReference*> GlobalListLink;
};

/**
 * each player that is active on the current client/listen server has a LocalPlayer
 * it stays active across maps, and there may be several spawned in the case of splitscreen/coop
 * there will be 0 spawned on dedicated servers
 */
// 009 - Foundation - CreateInnerProcessPIEGameInstance * - ULocalPlayer
// - LocalPlayer is created in client(or listen server) but dedicated server doesn't spawn LocalPlayer!
// - LocalPlayer's lifetime is 'independent' from World's lifetime:
//   - try to understand the relationship between LocalPlayer and World:
//
//         EditorEngine                                                                                                             
//          │                                                                                                                       
//          └──WorldContexts: TArray<FWorldContext>                                                                                 
//                                                                                                                                  
//                ▲                                                                                                                 
//                │                                                                                                                 
//     [OuterPrivate as 'Engine']                                                                                                   
//                │               ┌───────────────────► GameInstance:                                                               
//          ┌─────┴─────┐         │                      │                                                                          
//          │LocalPlayer├─────────┘                      └──LocalPlayers: TArray<ULocalPlayer>                                      
//          └───────────┘ managed by GameInstance           ────────┬─────────────────────────                                      
//                                                                  │                                                               
//                                                                  │                                                               
//                                                            ***independent from World                                             
//                                                                -GameInstance can change its dependent WorldContext any time      
//                                                                -LocalPlayer is also independent from WorldContext                
//                                                                                                                                  
// - see UPlayer
class ULocalPlayer : public UPlayer
{
    /** create an actor for this player */
    // 027 - Foundation - CreateInnerProcessPIEGameInstance * - ULocalPlayer::SpawnPlayActor
    virtual bool SpawnPlayActor(const FString& URL, FString& OutError, UWorld* InWorld)
    {
        {
            FURL PlayerURL(NULL, *URL, TRAVEL_Absolute);

            // haker: see UWorld::SpawnPlayerActor (goto 028: CreateInnerProcessPIEGameInstance)
            PlayerController = InWorld->SpawnPlayActor(this, ROLE_SimulatedProxy, PlayerURL, UniqueId, OutError, GEngine->GetGamePlayers(InWorld).Find(this));
        }
        return PlayerController != NULL;
    }

    /** retrieve the viewpoint of this player */
    // 023 - Foundation - SceneRenderer * - ULocalPlayer::GetViewPoint
    // haker: as I said that FMinimalViewInfo is the minimal data set for camera, where do we get the camera data in LocalPlayer?
    // - "APlayerController::PlayerCameraManager!""
    virtual void GetViewPoint(FMinimalViewInfo& OutViewInfo) const
    {
        if (PlayerController != NULL)
        {
            if (PlayerController->PlayerCameraManager != NULL)
            {
                // haker: as we expected, we fill MinimalViewInfo from PlayerCameraManager
                // - see CameraCachePrivate (goto 024: SceneRenderer)
                OutViewInfo = PlayerController->PlayerCameraManager->GetCameraCacheView();

                // haker: CamerCacheView's POV is not synced with PlayerCameraManager's camera properties
                // - update FOV, Location and Rotation
                OutViewInfo.FOV = PlayerController->PlayerCameraManager->GetFOVAngle();
                PlayerController->GetPlayerViewPoint(/*out*/OutViewInfo.Location, /*out*/OutViewInfo.Rotation);
            }

            // we store the originally desired FOV as other classes may adjust to account for ultra-wide aspect ratios
            OutViewInfo.DesiredFOV = OutViewInfo.FOV;
        }
    }

    // 021 - Foundation - SceneRenderer * - ULocalPlayer::GetProjectionData
    virtual bool GetProjectionData(FViewport* Viewport, FSceneViewProjectionData& ProjectionData, int32 StereoViewIndex = INDEX_NONE) const
    {
        // haker: *** we are not dealing with the detail of camera matrix calculation mathmatically, I'll leave it to you~ :)

        // haker: try to understand variables relations with diagram:
        //                                                                     ***                                                   
        //                                                                     ─────                                                 
        //                 ***[InitialPositionXY + Origin.XY * Viewport.SizeXY: MIN                                                  
        //                     InitialPositionXY + Origin.XY * Viewport.SizeXY + SizeXY * Viewport.SizeXY: MAX]                      
        //                                                                                                ─────                      
        //                                                                                                 ***                       
        //           [XY, XY+SizeXY]──────────────────────────────────────────────────────────────────────┐                          
        //            │                                                                                   │                          
        //  Origin    │                                                                                   │                          
        //    │       │                                                                                   │                          
        //    │       │                                                                                   │                          
        //    ▼       │                                                                                   │                          
        //    x───────┼────┬─▲───            ***by multiplying ViewportSizeXY     ┌───────────────────────┼───────┬─▲──              
        //    │       │    │ │                                                    │                       │       │ │                
        //    │       ▼    │ │               ────────────────────────────────►    │                       │       │ │                
        //    │       ┌──┐ │ │ SizeY                                              │                       │       │ │                
        //    │       └──┘ │ │ :[0,1]                                             │                       ▼       │ │                
        //    │            │ │                                                    │                       ┌────┐  │ │                
        //    │            │ │                                                    │                       │    │  │ │ Viewport.SizeY 
        //    ├────────────┼─▼───                                                 │                       │    │  │ │ [0,Height]     
        //    │            │                                                      │                       │    │  │ │                
        //    │◄──────────►│                                                      │                       └────┘  │ │                
        //    │ SizeX      │                                                      │                               │ │                
        //      :[0,1]                                                            │                               │ │                
        //                                                                        │                               │ │                
        //                                                                        │                               │ │                
        //                                                                        │                               │ │                
        //                                                                        │                               │ │                
        //                                                                        │                               │ │                
        //                                                                        ├───────────────────────────────┼─▼──              
        //                                                                        │                               │                  
        //                                                                        │◄─────────────────────────────►│                  
        //                                                                        │        Viewport.SizeX         │                  
        //                                                                                 [0,Width]                                 

        int32 X = FMath::TruncToInt(Origin.X * Viewport->GetSizeXY().X);
        int32 Y = FMath::TruncToInt(Origin.Y * Viewport->GetSizeXY().Y);
        X += Viewport->GetInitialPositionXY().X;
        Y += Viewport->GetInitialPositionXY().Y;

        uint32 SizeX = FMath::TruncToInt(Size.X * Viewport->GetSizeXY().X);
        uint32 SizeY = FMath::TruncToInt(Size.Y * Viewport->GetSizeXY().Y);

        FIntRect UnconstrainedRectangle = FIntRect(X, Y, X+SizeX, Y+SizeY);
        ProjectionData.SetViewRectangle(UnconstrainedRectangle);

        // get the viewport
        // haker: see FMinimalViewInfo (goto 022: SceneRenderer)
        // - see ULocalPlayerGetViewPoint (goto 023: SceneRenderer)
        FMinimalViewInfo ViewInfo;
        GetViewPoint(/*out*/ViewInfo);

        // haker: by retrieving camera data by ViewInfo, we can access up-to-date camera information from the world (after UWorld::Tick)
        // - note that we are in 'GameThread'

        // create the view matrix
        // haker: we'll not covered how view matrix is constructed mathmatically, I'll leave it you~ :)
        ProjectionData.ViewOrigin = ViewInfo.Location;
        ProjectionData.ViewRotationMatrix = FInverseRotationMatrix(ViewInfo.Rotation) * FMatrix(
            FPlane(0, 0, 1, 0),
            FPlane(1, 0, 0, 0),
            FPlane(0, 1, 0, 0),
            FPlane(0, 0, 0, 1));
        
        if (!NeedStereo)
        {
            // create the projection matrix (and possibly constrain the view rectangle)
            // haker: now we are going to construct the projection matrix with 'ProjectedData' which is derived from the viewport
            // - see FMinimalViewInfo::CalculateProjectionMatrixGivenView (goto 029: SceneRenderer)
            FMinimalViewInfo::CalculateProjectionMatrixGivenView(ViewInfo, AspectRatioAxisConstraint, Viewport, /*inout*/ProjectionData);
        }
        else
        {
            //...
        }

        return true;
    }

    // 020 - Foundation - SceneRenderer * - CalcSceneViewInitOptions
    virtual bool CalcSceneViewInitOptions(
        struct FSceneViewInitOptions& ViewInitOptions,
        FViewport* Viewport,
        class FViewElementDrawer* ViewDrawer = NULL,
        int32 StereoViewIndex = INDEX_NONE)
    {
        // get projection data
        // haker: calculate camera project matrix with Viewport:
        // - see ULocalPlayer::GetProjectionData (goto 021: SceneRenderer)
        if (GetProjectionData(Viewport, /*inout*/ViewInitOptions, StereoViewIndex) == false)
        {
            return false;
        }

        const uint32 ViewIndex = 0;

        // make sure the ViewStates array has enough elements for the given ViewIndex
        // haker: we need to ensure ViewState should exists per SceneView
        // - the SceneViewState is once created, it isn't be destroyed like SceneView
        {
            const int32 RequiredViewStates = (ViewIndex + 1) - ViewStates.Num();
            if (RequiredViewStates > 0)
            {
                ViewStates.AddDefaulted(RequiredViewStates);
            }
        }

        // allocate the current ViewState if necessary
        // haker: see FSceneViewStateReference::Allocate (goto 031: SceneRenderer)
        if (ViewStates[ViewIndex].GetReference() == nullptr)
        {
            const UWorld* CurrentWorld = GetWorld();
            const FRHIFeatureLevel::Type FeatureLevel = CurrentWorld ? CurrentWorld->GetFeatureLevel() : GMaxRHIFeatureLevel;
            ViewStates[ViewIndex].Allocate(FeatureLevel);
        }

        ViewInitOptions.SceneViewStateInterface = ViewStates[ViewIndex].GetReference();
        ViewInitOptions.ViewActor = PlayerController->GetViewTarget();

        // haker: other ViewInitOptions initialization...

        return true;
    }

    /** calculate the view init settings for drawing from the view actor */
    // 017 - Foundation - SceneRenderer * - ULocalPlayer::CalcSceneView
    virtual bool CalcSceneView(
        class FSceneViewFamily* ViewFamily,
        FVector& OutViewLocation,
        FRotator& OutViewRotation,
        FViewport* Viewport,
        class FViewElementDrawer* ViewDrawer = NULL,
        int32 StereoViewIndex = INDEX_NONE)
    {
        // haker: see FSceneViewInitOptions (goto 018: SceneRenderer)
        // - by skimming FSceneViewInitOptions, we can think of 'FSceneViewOptions' as init-data for SceneView
        // - see CalcSceneViewInitOptions (goto 020: SceneRenderer)
        FSceneViewInitOptions ViewInitOptions;
        if (!CalcSceneViewInitOptions(ViewInitOptions, Viewport, ViewDrawer, StereoViewIndex))
        {
            return nullptr;
        }

        // fill out the rest of the view init options
        // haker: we need to set SceneViewFamily to ViewInitOptions to initialize SceneView's Family properly
        ViewInitOptions.ViewFamily = ViewFamily;

        //...

        // haker: NOW create new SceneView:
        // - it is created every frame
        FSceneView* const View = new FSceneView(ViewInitOptions);
        OutViewLocation = View->ViewLocation;
        OutViewRotation = View->ViewRotation;
        
        // haker: make sure we add new SceneView to ViewFamily
        ViewFamily->Views.Add(View);
        {
            // post-process setting driven by main-camera
            // haker: as we saw SceneView is camera-data in Scene(render-world)
            // - camera data has its own post-process
            // - post-process's data should be transferred to renderer
            // - SceneView takes care of camera's post-process
            if (PlayerController->PlayerCameraManager)
            {
                //...
            }
        }

        return View;
    }

    /** the coordinates for the upper left corner of the primary viewport subregion allocated to this player: [0,1] */
    FVector2D Origin;

    /** the size of the primary viewport subregion allocated to this player: [0,1] */
    FVector2D Size;

    // 016 - Foundation - SceneRenderer * - ULocalPlayer::ViewStates
    // haker: owner of SceneViewState
    // - see FSceneViewStateReference briefly
    TArray<FSceneViewStateReference> ViewStates;
};