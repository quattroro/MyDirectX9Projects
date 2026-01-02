
// 055 - Foundation - CreateInnerProcessPIEGameInstance - FSetPlayerStatePawn
// haker: see the constructor
struct FSetPlayerStatePawn
{
    FSetPlayerStatePawn(APlayerState* PlayerState, APawn* Pawn)
    {
        APawn* OldPawn = PlayerState->PawnPrivate;
        // haker: SetPawnPrivate is simple as it means
        PlayerState->SetPawnPrivate(Pawn);
        PlayerState->OnPawnSet.Broadcast(PlayerState, PlayerState->PawnPrivate, OldPawn);
    }
};

/**
 * pawn is the base class of all actors that can be possessed by players or AI
 * they are the phyiscal representation of players and creatures in a level
 */
class APawn : public AActor, public INavAgentInterface
{
    virtual void RecalculateBaseEyeHeight()
    {
        BaseEyeHeight = GetDefault<APawn>(GetClass())->BaseEyeHeight;
    }

    /** updates Pawn's rotation to the given rotation, assumed to be the Controller's ControlRotation: respects the bUseControllerRotation* settings */
    // 062 - Foundation - CreateInnerProcessPIEGameInstance
    virtual void FaceRotation(FRotator NewControlRotation, float DeltaTime = 0.f)
    {
        // only if we actually are going to use any component of rotation
        // haker: do you remember bUseControllerRotation[Pitch|Yaw|Roll] in Lyra Project (LyraCharacter)?
        // - using this variable, we can limit free-of-degree axis rotation:
        //   - e.g. disabling Pitch/Yaw/Roll?
        // [ ] see ALyraCharacter::ALyraCharacter()
        if (bUseControllerRotationPitch || bUseControllerRotationYaw || bUseControllerRotationRoll)
        {
            const FRotator CurrentRotation = GetActorRotation();

            // haker: if we limit specific axis rotation, we use Pawn's rotation
            if (!bUseControllerRotationPitch)
            {
                NewControlRotation.Pitch = CurrentRotation.Pitch;
            }

            if (!bUseControllerRotationYaw)
            {
                NewControlRotation.Yaw = CurrentRotation.Yaw;
            }

            if (!bUseControllerRotationRoll)
            {
                NewControlRotation.Roll = CurrentRotation.Roll;
            }

            // haker: after adopting limit-axis-rotation, reapply rotation to Pawn
            // - Pawn has the ownership to accept which rotation axis to apply from Controller
            SetActorRotation(NewControlRotation);
        }
    }

    /** called when the Pawn is being restarted (usually by being possessed by a Controller): this is called on the server for all pawns and the owning client for player pawns */
    // 059 - Foundation - CreateInnerProcessPIEGameInstance - APawn::Restart
    // haker: this Restart() is usually called when new-Controller possesses 'this' Pawn 
    virtual void Restart()
    {
        // haker: when possession is updated, we need to clear all pending movements from MovementComponent
        // - MovementComponent is out-of-scope in this lecture, we are not going to deal with it in detail
        UPawnMovementComponent* MovementComponent = GetMovementComponent();
        if (MovementComponent)
        {
            MovementComponent->StopMovementImmediately();
        }

        // see ConsumeMovementInputVector (goto 060: CreateInnerProcessPIEGameInstance)
        ConsumeMovementInputVector();

        RecalculateBaseEyeHeight();

        // haker: Restart() is nothing special, just reset Movement-related properties, cuz currently Pawn is newly-possessed by NewController
    }

    virtual UInputComponent* CreatePlayerInputComponent()
    {
        static const FName InputComponentName(TEXT("PawnInputComponent0"));
        const UClass* OverrideClass = OverrideInputComponentClass.Get();
        return NewObject<UInputComponent>(this, OverrideClass ? OverrideClass : UInputSettings::GetDefaultInputComponentClass(), InputComponentName);
    }

    /** allows a Pawn to set up custom input bindings: called upon possession by a PlayerController, using the InputComponent created by CreatePlayerInputComponent() */
    virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
    {
        // haker: in lyra project, it use this method as init state
    }

    /** called on the owning client of a player-controlled Pawn when it is restarted, this calls Restart() */
    // 058 - Foundation - CreateInnerProcessPIEGameInstance - APawn::PawnClientRestart
    virtual void PawnClientRestart()
    {
        // see APawn::Restart (goto 059: CreateInnerProcessPIEGameInstance)
        Restart();

        APlayerController* PC = Cast<APlayerController>(Controller);
        if (PC && PC->IsLocallyControlled())
        {
            // handled camera possession
            // haker: AutoManageActiveCameraTarget() updates PlayerCameraManager's ViewTarget with appropriate CameraActor or Pawn
            // - Possession is changed, so it is natural to update PlayerCameraManager's ViewTarget
            if (PC->bAutoManageActiveCameraTarget)
            {
                // see APlayerController::AutoManageActiveCameraTarget()
                PC->AutoManageAcetiveCameraTarget(this);
            }

            // set up player input component, if there isn't one already
            if (InputComponent == nullptr)
            {
                InputComponent = CreatePlayerInputComponent();
                if (InputComponent)
                {
                    // haker: for LyraClone tutorial, we use this SetupPlayerInputComponent() as our InitState call!
                    SetupPlayerInputComponent(InputComponent);
                    InputComponent->RegisterComponent();
                    //...
                }
            }
        }

        // haker: now we understand what Restart on Pawn means:
        // - the current Pawn's possession is changed by NewController
        // 1. need to reset(uninitialize) its MovementComponent
        // 2. need to update NewController's CameraTarget with 'this'(NewPawn)
        // 3. need to create InputComponent and Push its InputComponent to NewController's PlayerInput
        //
        // - we can think of this process as re-connecting non-world properties(controller) to world properties(pawn)
    }

    /** Event called after a pawn has been restarted, usually by a possession change. This is called on the server for all pawns and the owning client for player pawns */
	UFUNCTION(BlueprintImplementableEvent)
	void ReceiveRestarted();

	/** Event called after a pawn has been restarted, usually by a possession change. This is called on the server for all pawns and the owning client for player pawns */
	UPROPERTY(BlueprintAssignable, Category = Pawn)
	FPawnRestartedSignature ReceiveRestartedDelegate;

    virtual void NotifyRestarted()
    {
        ReceiveRestarted();
        ReceiveRestartedDelegate.Broadcast(this);
    }

    /** wrapper function to call correct restart functions, enable bCallClientRestart if this is a locally owned player pawn or equivalent */
    // 057 - Foundation - CreateInnerProcessPIEGameInstance - APawn::DispatchRestart
    void DispatchRestart(bool bCallClientRestart)
    {
        // haker: bCallClientRestart is 'true' when this Pawn is locally controlled (by ULocalPlayer)
        // - PawnClientRestart() also calls Restart()
        if (bCallClientRestart)
        {
            // see APawn::PawnClientRestart (goto 058: CreateInnerProcessPIEGameInstance)
            PawnClientRestart();
        }
        else
        {
            Restart();
        }

        // haker: notify to BP 
        NotifyRestarted();
    }

    /** event called when the Pawn is possessed by a Controller. Only called on the server (or in standalone) */
    UFUNCTION(BlueprintImplementableEvent, BlueprintAuthorityOnly, meta=(DisplayName= "Possessed"))
    void ReceivePossessed(AController* NewController);

    /** called when this Pawn is possessed: only called on the server (or in standalone) */
    // 053 - Foundation - CreateInnerProcessPIEGameInstance - APawn::PossessedBy
    virtual void PossessedBy(AController* NewController)
    {
        // haker: see SetOwner briefly, but 'Children' is for network, so we are NOT dealing it in detail
        SetOwner(NewController);

        // haker: now update Pawn's Controller who newly possesses
        AController* const OldController = Controller;
        Controller = NewController;
        ForceNetUpdate();

        // haker: remember that PlayerController has PlayerState!
        if (Controller->PlayerState != nullptr)
        {
            // see SetPlayerState (goto 054: CreateInnerProcessPIEGameInstance)
            SetPlayerState(Controller->PlayerState);
        }
       
        // dispatch Blueprint event if necessary
        if (OldController != NewController)
        {
            // haker: notify this Pawn is possessed by NewController to BP
            ReceivePossessed(Controller);
            NotifyControllerChanged();
        }
    }

    /** set the Pawn's Player State: keep bi-directional reference of Pawn to Player State and back in sync */
    // 054 - Foundation - CreateInnerProcessPIEGameInstance - APawn::SetPlayerState
    void SetPlayerState(APlayerState* NewPlayerState)
    {
        // haker: APawn caches PlayerState! [also PlayerState cached APawn~ :)]
        // - do you remember where APlayerState is initialized?
        //   - APlayerController::PostInitializeComponents()
        APlayerState* OldPlayerState = PlayerState;

        // haker: if PlayerState has Pawn for 'this', clear PlayerState by FSetPlayerStatePawn()
        if (PlayerState && PlayerState->GetPawn() == this)
        {
            // see FSetPlayerStatePawn (goto 055: CreateInnerProcessPIEGameInstance)
            FSetPlayerStatePawn(PlayerState, nullptr);
        }

        // haker: we update new PlayerState of PC and update PlayerState's PawnPrivate as 'this'
        PlayerState = NewPlayerState;
        if (PlayerState)
        {
            FSetPlayerStatePawn(PlayerState, this);
        }

        OnPlayerStateChanged(NewPlayerState, OldPlayerState);
    }

    /** set the owner of this Actor, used primarily for network replication */
    virtual void SetOwner(AActor* NewOwner)
    {
        if (Owner != NewOwner && IsValidChecked(this))
        {
            if (NewOwner != nullptr && NewOwner->IsOwnedBy(this))
            {
                return;
            }

            Owner = NewOwner;

            if (Owner != nullptr)
            {
                // add to new owner's Children array
                Owner->Children.Add(this);
            }
        }
    }

    /** event called when the Pawn is no longer possessed by a Controller. Only called on the server (or in standalone) */
    UFUNCTION(BlueprintImplementableEvent, BlueprintAuthorityOnly, meta=(DisplayName= "Unpossessed"))
    void ReceiveUnpossessed(AController* OldController);

    /** 
     * return our PawnMovementComponent, if we have one: by default, returns the first PawnMovementComponent found:
     * native classes that create their own movement component should override this method for more efficiency
     */
    virtual UPawnMovementComponent* GetMovementComponent() const
    {
        return FindComponentByClass<UPawnMovementComponent>();
    }

    /** internal function meant for use only within Pawn or by a PawnMovementComponent */
    FVector Internal_ConsumeMovementInputVector()
    {
        // haker: accumulated 'ControlInputVector' is cached to 'LastControlInputVector' and reset to ZeroVector
        LastControlInputVector = ControlInputVector;
        ControlInputVector = FVector::ZeroVector;
        return LastControlInputVector;
    }

    /**
     * returns the pending input vector and resets it to zero
     * this should be used during a movement update (by the Pawn or PawnMovementComponent) to prevent accumulation of control input between frames
     * copies the pending input vector to saved input vector (GetLastMovementInputVector())
     */
    // 060 - Foundation - CreateInnerProcessPIEGameInstance - APawn::ConsumeMovementInputVector
    virtual FVector ConsumeMovementInputVector()
    {
        // haker: see Internal_ConsumeMovementInputVector() briefly
        // - this function is used to transfer accumulated ControlInputVector to MovementComponent
        // - we are not going to deal with MovementComponent, so how to transfer ControlInputVector is skipped
        UPawnMovementComponent* MovementComponent = GetMovementComponent();
        if (MovementComponent)
        {
            return MovementComponent->ConsumeInputVector();
        }
        else
        {
            return Internal_ConsumeMovementInputVector();
        }
    }

    /** called when our Controller no loger possesses us: only called on the server (for in standalone) */
    virtual void UnPossessed()
    {
        AController* const OldController = Controller;

        ForceNetUpdate();

        SetPlayerState(nullptr);
        SetOwner(nullptr);
        Controller = nullptr;

        // unregister input component if we created one
        DestroyPlayerInputComponent();

        // dispatch BP event if necessary
        if (OldController)
        {
            ReceiveUnpossessed(OldController);
        }

        NotifyControllerChanged();

        ConsumeMovementInputVector();
    }

    /** if pawn is possessed by a player, points to its PlayerState: Needed for network play as controllers are not replicated to clients */
    TObjectPtr<APlayerState> PlayerState;

    /** controller currently possessing this Actor */
    TObjectPtr<AController> Controller;

    /** the last control input vector that was processed by ConsumeMovementInputVector() */
    FVector LastControlInputVector;

    /** accumulated control input vector, stored in world space: this is the pending input, which is cleared (zeroed) once consumed */
    FVector ControlInputVector;

    /** base eye height above collision center */
    float BaseEyeHeight;

    /** if set then this InputComponent class will be used instead of the InputSettings' DefaultInputComponentClass */
    TSuclassOf<UInputComponent> OverrideInputComponentClass = nullptr;

    /** If true, this Pawn's pitch will be updated to match the Controller's ControlRotation pitch, if controlled by a PlayerController. */
	uint32 bUseControllerRotationPitch:1;

	/** If true, this Pawn's yaw will be updated to match the Controller's ControlRotation yaw, if controlled by a PlayerController. */
	uint32 bUseControllerRotationYaw:1;

	/** If true, this Pawn's roll will be updated to match the Controller's ControlRotation roll, if controlled by a PlayerController. */
	uint32 bUseControllerRotationRoll:1;
};

/**
 * Characters are Pawns that have a mesh, collision, and built-in movement logic
 * they are responsible for all physical interaction between the player or AI and the world, and also implement basic networking and input models
 * they are designed for a vertically-oriented player representation that can walk, jump, fly and swim through the world using CharacterMovementComponent 
 */
class ACharacter : public APawn
{
    virtual void Restart() override
    {
        Super::Restart();

        //...
    }

    virtual void PawnClientRestart() override
    {
        if (CharacterMovement != nullptr)
        {
            CharacterMovement->StopMovementImmediately();
            CharacterMovement->ResetPredictionData_Client();
        }

        Super::PawnClientRestart();
    }

    virtual void RecalculateBaseEyeHeight() override
    {
        if (!bIsCrouched)
        {
            Super::RecalculateBaseEyeHeight();
        }
        else
        {
            BaseEyeHeight = CrouchedEyeHeight;
        }
    }

    /** movement component used for movement logic in various modes (walking, falling, etc), containing relevent settings and functions to control movement */
    TObjectPtr<UCharacterMovementComponent> CharacterMovement;

    /** default crouched eye height */
    float CrouchedEyeHeight;
};