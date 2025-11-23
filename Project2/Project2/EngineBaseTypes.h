/**
 * abstract base class for all tick functions 
 */
// 45 - Foundation - CreateWorld - FTickFunction
// see FTickFunctions' member variables (goto 46)
struct FTickFunction
{
    /** see if the tick function is currently registered */
    bool IsTickFunctionRegistered() const { return (InternalData && InternalData->bRegistered); }

    /** returns whether the tick function is currently enabled */
    bool IsTickFunctionEnabled() const { return TickState != ETickState::Disabled; }

    /** enables or disables this tick function */
    // 55 - Foundation - CreateWorld - FTickFunction::SetTickFunctionEnable
    void SetTickFunctionEnable(bool bInEnabled)
    {
        // haker: this function is to enable self(tick-function) to tick()
        // see IsTickFunctionRegistered
        if (IsTickFunctionRegistered())
        {
            // haker: carefully read the condition in the if statement
            // - state is changed from disabled -> enabled
            if (bInEnabled == (TickState == ETickState::Disabled))
            {
                // haker: InternalData has tick-task-level which it is resides in
                FTickTaskLevel* TickTaskLevel = InternalData->TickTaskLevel;

                // see FTickTaskLevel::RemoveTickFunction (goto 56)
                TickTaskLevel->RemoveTickFunction(this);
                TickState = (bInEnabled ? ETickState::Enabled : ETickState::Disabled);

                // see FTickTaskLevel::AddTickFunction (goto 57)
                TickTaskLevel->AddTickFunction(this);

                // haker: from here, you should see simple-rule in coding:
                // when Enable/Disable FTickFunction, just ***re-insert it***
                // - it doesn't add any complex conditions to handle each case
            }

            if (TickState == ETickState::Disabled)
            {
                InternalData->LastTickGameTimeSeconds = -1.f;
            }
        }
        else
        {
            // haker: if it is NOT registered yet, just update the TickState
            // - when TickFunction is registered, it will handle it as we covered above
            TickState = (bInEnabled ? ETickState::Enabled : ETickState::Disabled);
        }

        // search (goto 55)
    }

    /** adds the tick function to the primary list of tick functions */
    // 63 - Foundation - CreateWorld - FTickFunction::RegisterTickFunction
    void RegisterTickFunction(ULevel* Level)
    {
        if (!IsTickFunctionRegistered())
        {
            // only allow registration of tick if we are allowed on dedicated server, or we are not a dedicated server
            const UWorld* World = Level ? Level->GetWorld() : nullptr;

            // haker: we didn't care about dedicated server for now
            if (bAllowTickOnDedicatedServer || !(World && World->IsNetMode(NM_DedicatedServer)))
            {
                if (InternelData == nullptr)
                {
                    InternalData.Reset(new FInternalData());
                }
                // haker: where FTickTaskLevel is allocated in ULevel?
                // - in ULevel's constructor, TickTaskLevel is allocated

                // haker: registering TickFuction means adding TickFunction
                FTickTaskManager::Get().AddTickFunction(Level, this);

                // haker: mark bRegistered as true
                InternelData->bRegistered = true;
            }
        }
        else
        {
            // haker: ?! 
            check(FTickTaskManager::Get().HasTickFunction(Level, this));
        }
    }

    /** removes the tick function from the primary list of tick functions */
    // 64 - Foundation - CreateWorld - FTickFunction::UnRegisterTickFunction
    void UnRegisterTickFunction()
    {
        if (IsTickFunctionRegistered())
        {
            // haker: remove it and mark bRegistered as false
            FTickTaskManager::Get().RemoveTickFunction(this);
            InternalData->bRegistered = false;
        }
    }

    // 46 - Foundation - CreateWorld - FTickFunction's member variables

    /** If false, this tick function will never be registered and will never tick. Only settable in defaults */
    // haker: if true, it will tick every frame
    uint8 bCanEverTick : 1;

    /** if true, this tick function will start enabled, but can be disabled later on */
    // haker: like bAutoActivate, when tick function is instantiated, automatically register tick function to tick every frame
    uint8 bStartWithTickEnabled : 1;

    enum class ETickState : uint8
    {
        Disabled,
        Enabled,
        CoolingDown
    };

    /**
     * if Disabled, this tick will not fire
     * if CoolingDown, this tick has an interval frequency that is being adhered to currently
     * CAUTION: do not set this directly
     */
    // see ETickState:
    // haker: tick function could be specified its frequency
    // - when frequency value is set, tick function will have a moment to be in CoolingDown
    ETickState TickState;

    /** internal data structure that contains members only required for a registered tick function */
    struct FInternalData
    {
        /** whether the tick function is registered */
        bool bRegistered : 1;

        /** cache whether this function was rescheduled as an interval function during StartParallel */
        // haker: if true, the TickFunction is in CoolingDown list
        bool bWasInternal : 1;

        /** the next function in the cooling down list for ticks with an interval */
        // haker: cooling down tick-function list is in a form of linked list
        FTickFunction* Next;

        /** back pointer to the FTickTaskLevel containing this tick function if it is registered */
        // haker: FTickFunction has similar relationship between AActor and ULevel:
        // - FTickFunction is contained in FTickTaskLevel
        // see FTickTaskLevel (goto 47)
        class FTickTaskLevel* TickTaskLevel;
    };

    /** lazily allocated struct that contains the necessary data for a tick function that is registered */
    // haker: why does it separate data as InternalData?
    // - we can achieve the following benefits, splitting hot/cold data
    //   - memory optimization
    //   - performance optimization
    // - think how many tick functions exist: Actor & ActorComponent
    // see FInternalData
    TUniquePtr<FInternalData> InternalData;
};