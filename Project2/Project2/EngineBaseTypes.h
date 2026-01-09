
/** type of tick we wish to perform on the level */
enum ELevelTick
{
    /** update the level time only */
    LEVELTICK_TimeOnly = 0,
    /** update time and viewports */
    LEVELTICK_ViewportsOnly = 1,
    /** update all */
    LEVELTICK_All = 2,
    /** delta time is zero, we are paused; components don't tick */
    LEVELTICK_PauseTick = 3,
};

/** URL structure */
struct FURL
{
    /** Map name: i.e. "Seoul" */
    FString Map;

    /** Options */
    TArray<FString> Op;
};

/** traveling from server to server */
enum ETravelType : int32
{
    /** absolute URL. */
    TRAVEL_Absolute,
    /** partial (carry name, reset server). */
    TRAVEL_Partial,
    /** relative URL. */
    TRAVEL_Relative,
    TRAVEL_MAX,
};

/** determines which ticking group a tick function belongs to */
// 009 - Foundation - TickTaskManager - ETickingGroup
// haker: you can think of TickingGroup as join point (or sync point) in task graph:
// - see the diagram below:
//                                                                                                                                                                                                          
//                                 ┌───TickFunction0◄────TickFunction2◄────┐                                                                                                                                
//                                 │                                       │                                                                                                                                
//            ┌───────────────┐    │                                       │    ┌─────────────────┐                                        ┌───────────────┐                                                
//            │ TG_PrePhysics ◄────┼───TickFunction1◄──────────────────────┼────┤ TG_StartPhysics │◄──┬───TickFunction6◄───────┬───────────┤ TG_EndPhysics │                                                
//            └───────────────┘    │                                       │    └─────────────────┘   │                        │           └───────────────┘                                                
//                                 │                                       │                          │                        │                                                                            
//                                 ├───TickFunction3◄─┐                    │                          └───TickFunction7 ◄──────┘                                                                            
//                                 │                  │                    │                                                                                                                                
//                                 │                  ├──TickFunction4 ◄───┘                                                                                                                                
//                                 │                  │                                                                                                                                                     
//                                 └───TickFunction5◄─┘      
// - try to understand above diagram as conceptual figure!                                                                                                                                               
enum ETickingGroup : int
{
    /** any item that needs to be executed before physics simulation starts */
    TG_PrePhysics = 0,

    /** special tick group that start physics simulation */
    TG_StartPhysics,

    /** any item that can be run in parallel with our physics simulation work */
    TG_DuringPhysics,

    /** special tick group that ends physics simulation */
    TG_EndPhysics,

    /** any item that needs rigid body and cloth simulation to be complete before being executed */
    TG_PostPhysics,

    /** any item that needs the update work to be done before being ticked */
    TG_PostUpdateWork,

    /** catchall for anything demoted to the end */
    TG_LastDemotable,

    /** 
     * special tick group that is not actually a tick group
     * after every tick group, this is repeatedly re-run until there are no more newly spawned items to run
     */
    TG_NewlySpawned,

    TG_MAX,
};

/** this is small structure to hold prerequisite tick functions */
// 008 - Foundation - TickTaskManager - FTickPrerequisite
struct FTickPrerequisite
{
    /** tick functions live inside of UObjects, so we need a separate weak pointer to the UObject solely for the purpose of determining if PrerequisiteTickFunction is still valid */
    // haker: normally FTickFunction's aliveness is determined by UObject(ActorTickFunction -> AActor, ActorComponentTickFunction -> UActorComponent)
    TWeakObjectPtr<UObject> PrerequisiteObject;

    /** pointer to the actual tick function and must be completed prior to our tick running */
    FTickFunction* PrerequisiteTickFunction;

    FTickPrerequisite(UObject* TargetObject, struct FTickFunction& TargetTickFunction)
        : PrerequisiteObject(TargetObject)
        , PrerequisiteTickFunction(&TargetTickFunction)
    {}
};

// 024 - Foundation - TickTaskManager - CanDemoteIntoTickGroup
bool CanDemoteIntoTickGroup(ETickingGroup TickGroup)
{
    // haker: TG_StartPhysics and TG_EndPhyiscs is special tick-group not for general tick function!
    // - TG_StartPhysics -> TG_DuringPhysics
    // - TG_EndPhysics -> TG_PostPhysics
    switch (TickGroup)
    {
    case TG_StartPhysics:
    case TG_EndPhysics:
        return false;
    }
    return true;
}

/**
 * abstract base class for all tick functions 
 */
// 045 - Foundation - CreateWorld * - FTickFunction
// see FTickFunctions' member variables (goto 46)
struct FTickFunction
{
    /** Abstract function actually execute the tick */
    // haker: see ActorTickFunction
    virtual void ExecuteTick(float DeltaTime, ELevelTick TickType, ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent) PURE_VIRTUAL(,);

    /** see if the tick function is currently registered */
    bool IsTickFunctionRegistered() const { return (InternalData && InternalData->bRegistered); }

    /** returns whether the tick function is currently enabled */
    bool IsTickFunctionEnabled() const { return TickState != ETickState::Disabled; }

    /** enables or disables this tick function */
    // 055 - Foundation - CreateWorld * - FTickFunction::SetTickFunctionEnable
    void SetTickFunctionEnable(bool bInEnabled)
    {
        // haker: this function is to enable self(tick-function) to tick()
        // see IsTickFunctionRegistered
        // - bRegistered is prerequisite condition to enable TickFunction
        // 
        // from our callstack, we are not registered yet, but see the logic where we'll covered soon
        if (IsTickFunctionRegistered())
        {
            // haker: carefully read the condition in the if statement
            // - state is changed from disabled -> enabled
            // - if our state is Enabled, no need to re-enable TickFunction!
            // - if our state is Disabled and bInEnable is 'false', enable -> disable

            // 비활성화 -> 활성화, 활성화 -> 비활성화 이렇게 2가지 경우만 들어온다. 잘 만들어놨네ㅣ...
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
    // 063 - Foundation - CreateWorld * - FTickFunction::RegisterTickFunction
    void RegisterTickFunction(ULevel* Level)
    {
        // see IsTickFunctionRegistered()
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
                // - we'll see TickTaskManager later
                FTickTaskManager::Get().AddTickFunction(Level, this);

                // haker: mark bRegistered as true
                InternelData->bRegistered = true;
            }
        }
        else
        {
            // haker: when TickFunction is already registered, it should be there!
            check(FTickTaskManager::Get().HasTickFunction(Level, this));
        }
    }

    /** removes the tick function from the primary list of tick functions */
    // 064 - Foundation - CreateWorld * - FTickFunction::UnRegisterTickFunction
    void UnRegisterTickFunction()
    {
        if (IsTickFunctionRegistered())
        {
            // haker: remove it and mark bRegistered as false
            FTickTaskManager::Get().RemoveTickFunction(this);
            InternalData->bRegistered = false;
        }
    }

    /** adds a tick function to the list of prerequisites... in other words, add the requirement that TargetTickFunction is called before this tick function */
    // 006 - Foundation - TickTaskManager - FTickFunction::AddPrerequisite
    void AddPrerequisite(UObject* TargetObject, struct FTickFunction& TargetTickFunction)
    {
        // haker: CanTick is determined by bRegistered && bCanEverTick
        const bool bThisCanTick = (bCanEverTick || IsTickFunctionRegistered());
        const bool bTargetCanTick = (TargetTickFunction.bCanEverTick || TargetTickFunction.IsTickFunctionRegistered());
        if (bThisCanTick && bTargetCanTick)
        {
            // see FTickFunction's member variables again (goto 007)
            // haker: now we can understand what Prerequisites is
            // - the detail of how it works will be seen in the code later
            Prerequisites.AddUnique(FTickPrerequisite(TargetObject, TargetTickFunction));
        }
    }

    /** removes a prerequisite that was previously added */
    // 010 - Foundation - TickTaskManager - FTickFunction::RemovePrerequisite
    void RemovePrerequisite(UObject* TargetObject, struct FTickFunction& TargetTickFunction)
    {
        Prerequisites.RemoveSwap(FTickPrerequisite(TargetObject, TargetTickFunction));
    }

    /** queues a tick function for execution from the game thread */
    // 023 - Foundation - TickTaskManager - FTickFunction::QueueTickFunction
    void QueueTickFunction(class FTickTaskSequencer& TTS, const FTickContext& TickContext)
    {
        // haker: pay attention to 'TickVisitedGFrameNumberCounter'
        // - use this to check whether this(tick function) is visited to call QueueTickFunction or not
        if (InternalData->TickVisitedGFrameCounter != GFrameCounter)
        {
            // haker: update TickVisitedGFrameNumber to prevent it from queuing twice
            InternalData->TickVisitedGFrameNumber = GFrameCounter;
            if (TickState != FTickFunction::ETickState::Disabled)
            {
                // haker: what is 'MaxPrerequisiteTickGroup' is for?
                // - you'll understand it after seeing how Prerequisites works
                ETickingGroup MaxPrerequisiteTickGroup = ETickingGroup(0);

                // haker: we construct TaskPrerequisites (GraphEventArray)
                // [ ] see engine code in UBaseMovementComponent::RegisterComponentTickFunctions as example for adding prerequisites
                EGraphEventArray TaskPrerequisites;
                for (int32 PrereqIndex = 0; PrereqIndex < Prerequisites.Num(); PrereqIndex++)
                {
                    FTickFunction* Prereq = Prerequisites[PrereqIndex].Get();
                    if (Prereq)
                    {
                        // stale prereq, delete it
                        Prerequisites.RemoveAtSwap(PrereqIndex--);
                    }
                    else if (Prereq->IsTickFunctionRegistered())
                    {
                        // recursive call to make sure my prerequisite is set up so I can use its completion handle
                        // haker: by recursive call, we queue prerequisite's tick function first!
                        // - similar manner to AActor::IncrementalRegisterComponents:
                        // - see the Diagram:
                        //                                                                                                                                                                                                     
                        // ┌────TickTaskLevel(Actual)───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┐                                                            
                        // │                                                                                                                                            │                                                            
                        // │                                                                                                                                            │                                                            
                        // │         ┌──────────────────┐     ┌──────────────────┐      ┌──────────────────┐      ┌──────────────────┐         ┌──────────────────┐     │                                                            
                        // │         │ TickFunction0    ├─────► TickFunction1    ├──────► TickFunction2    ├──────► TickFunction3    ├─────────► TickFunction4    │     │                                                            
                        // │         │                  │     │                  │      │                  │      │                  │         │                  │     │                                                            
                        // │         │    Prerequisite: │     │    Prerequisite: │      │    Prerequisite: │      │    Prerequisite: │         │    Prerequisite: │     │                                                            
                        // │         │    [1]           │     │    [2,3]         │      │    []            │      │    [4]           │         │    []            │     │                                                            
                        // │         └─────┬────────────┘     └────────▲──┬──────┘      └────────▲─────────┘      └───────▲─┬────────┘         └────────▲─────────┘     │                                                            
                        // │               │                           │  │                      │                        │ │                           │               │                                                            
                        // │               └───────────────────────────┘  └──────────────────────┴────────────────────────┘ └───────────────────────────┘               │                                                            
                        // │                        Queue(1)                              Queue(2), Queue(3)                         Queue(4)                           │                                                            
                        // │                                                                                                                                            │                                                            
                        // └────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────┘                                                            
                        //                                                                                                                                                                                                           
                        //                ┌───Concept(as TaskGraph)───────────────────────────────────────────────────────────────────────────────┐                                                                                  
                        //                │                                                                                                       │                                                                                  
                        //                │              ┌─────────────┐       ┌─────────────┐         ┌─────────────┐        ┌─────────────┐     │                                                                                  
                        //                │              │TickFunction4◄───────┤TickFunction3◄────┬────┤TickFunction1◄────────┤TickFunction0│     │                                                                                  
                        //                │              └─────────────┘       └─────────────┘    │    └─────────────┘        └─────────────┘     │                                                                                  
                        //                │                                                       │                                               │                                                                                  
                        //                │                                    ┌─────────────┐    │                                               │                                                                                  
                        //                │                                    │TickFunction2◄────┘                                               │                                                                                  
                        //                │                                    └─────────────┘                                                    │                                                                                  
                        //                │                                                                                                       │                                                                                  
                        //                └───────────────────────────────────────────────────────────────────────────────────────────────────────┘                                                                                  
                        //
                        // - Queuing Order:
                        //   - [TickFunction2, TickFunction4, TickFunction3, TickFunction1, TickFunction0]
                        Prereq->QueueTickFunction(TTS, TickContext);

                        // haker: see TickQueuedGFrameCounter below, focusing on where TickQueueGFrameNumber is updated:
                        // - when right before getting out QueueTickFunction(), TickQueuedGFrameCounter is updated
                        if (Prereq->InternalData->TickQueuedGFrameCounter != GFrameNumber)
                        {
                            // this must be up the call-stack, therefore this is a cycle
                            // haker: even though, QueueTickFunction is called, Prereq's TickQueuedGFrameCounter is not updated --> cycle could exist!
                            check(false);
                        }
                        else
                        {
                            // haker: answer the questions:
                            // - Diagram:                                                                                            
                            //   ┌─────────────┐       ┌─────────────┐         ┌─────────────┐        ┌─────────────┐       
                            //   │TickFunction4◄───────┤TickFunction3◄────┬────┤TickFunction1◄────────┤TickFunction0│       
                            //   │  Start:     │       │  Start:     │    │    │   Start:    │        │  Start:     │       
                            //   │    TG_Start │       │    TG_Pre   │    │    │     TG_End  │        │    TG_Pre   │       
                            //   └─────────────┘       └─────────────┘    │    └─────────────┘        └─────────────┘       
                            //                                            │                                                 
                            //                                            │                                                 
                            //                                            │                                                 
                            //                                            │                                                 
                            //                         ┌─────────────┐    │                                                 
                            //                         │TickFunction2◄────┘                                                 
                            //                         │  Start:     │                                                      
                            //                         │    TG_Pre   │                                                      
                            //                         └─────────────┘                                                      
                            // 
                            // 1. What is ActualStartTickGroup of TickFunction0?
                            // 2. What is ActualStartTickGroup of TickFunction1?
                            // 3. What is ActualStartTickGroup of TickFunction3?
                            #pragma region 
                            // 1. TG_End
                            // 2. TG_End
                            // 3. TG_Start
                            #pragma endregion                                                                                             
                            
                            // haker: from above examples, now we can understand why we use FMath::Max for MaxPrerequisiteTickGroup
                            MaxPrerequisiteTickGroup = FMath::Max<ETickingGroup>(MaxPrerequisiteTickGroup, Prereq->InternalData->ActualStartTickGroup.GetValue());

                            // haker: note that we are getting GraphEventRef from tick function
                            TaskPrerequisites.Add(Prereq->GetCompletionHandle());
                        }
                    }
                }

                // tick group is the max of the prerequisites, the current tick group, and the desired tick group
                // haker: of course, we also adjust ActualTickGroup with our own TickGroup and TickContext's TickGroup
                ETickingGroup MyActualTickGroup = FMath::Max<ETickingGroup>(MaxPrerequisiteTickGroup, FMath::Max<ETickingGroup>(TickGroup.GetValue(), TickContext.TickGroup));
                if (MyActualTickGroup != TickGroup)
                {
                    // haker: if tick function's tick group is adjusted, check CanDemote()
                    // - demote == lowering rank
                    //   - in our consensus, successful on adjusting tick group

                    // if the tick was 'demoted', make sure it ends up in an ordinary tick group
                    // see CanDemoteIntoTickGroup (goto 024 : TickTaskManager)
                    while (!CanDemoteIntoTickGroup(MyActualTickGroup))
                    {
                        MyActualTickGroup = ETickingGroup(MyActualTickGroup + 1);
                    }
                }

                InternalData->ActualStartTickGroup = MyActualTickGroup;
                InternalData->ActualEndTickGroup = MyActualTickGroup;

                // haker: adjust ActualEndTickGroup
                if (EndTickGroup > MyActualTickGroup)
                {
                    ETickingGroup TestTickGroup = ETickingGroup(MyActualTickGroup + 1);
                    while (TestTickGroup <= EndTickGroup)
                    {
                        if (CanDemoteIntoTickGroup(TestTickGroup))
                        {
                            InternalData->ActualEndTickGroup = TestTickGroup;
                        }
                        TestTickGroup = ETickingGroup(TestTickGroup + 1);
                    }
                }

                if (TickState == FTickFunction::ETickState::Enabled)
                {
                    // see FTickTaskSequencer::QueueTickTask (goto 025)
                    TTS.QueueTickTask(&TaskPrerequisites, this, TickContext);
                }
            }

            // haker: mark TickQueuedGFrameCounter
            InternalData->TickQueuedGFrameCounter = GFrameCounter;
        }
    }

    /** returns the delta time to use when ticking this function given the TickContext */
    float CalculateDeltaTime(const FTickContext& TickContext)
    {
        float DeltaTimeForFunction = TickContext.DeltaSeconds;
        //...
        return DeltaTimeForFunction;
    }

    // 046 - Foundation - CreateWorld * - FTickFunction's member variables
    // 007 - Foundation - TickTaskManager

    /** If false, this tick function will never be registered and will never tick. Only settable in defaults */
    // haker: if true, it will tick (every frame or certain interval(cool-time))
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

    /** 
     * frequency in seconds at which this tick function will be executed 
     * - if less than or equal to 0 then it will tick every frame
     */
    // haker: if TickInterval is > 0, means it will be reschedule/cooldown-list when it ticks
    float TickInterval;

    /** prerequisites for this tick function */
    // haker: we can specify prerequisites for the tick function
    // see FTickPrerequisite (goto 008)
    TArray<FTickPrerequisite> Prerequisites;

    /**
     * defines the minimum tick group for this tick function
     * these groups determine the relative order of when objects tick during a frame update
     * - given prerequisites, the tick may be delayed 
     */
    // see ETickingGroup (goto 009)
    // haker: TickGroup --> EndTickGroup is the life-cycle of tick function:
    //                                                                                                                                                                                                          
    //                                                                                                                                                                                                          
    //                                 ┌───TickFunction0◄────TickFunction2◄────┐                                                                                                                                
    //                                 │                                       │                                                                                                                                
    //            ┌───────────────┐    │                                       │    ┌─────────────────┐                                        ┌───────────────┐                                                
    //            │ TG_PrePhysics ◄────┼───TickFunction1◄──────────────────────┼────┤ TG_StartPhysics │◄──┬───TickFunction6◄───────┬───────────┤ TG_EndPhysics │                                                
    //            └───────────────┘    │                                       │    └─────────────────┘   │                        │           └───────────────┘                                                
    //                                 │                                       │                          │                        │                                                                            
    //                                 ├───TickFunction3◄─┐                    │                          └───TickFunction7 ◄──────┤                                                                            
    //                                 │                  │                    │                                                   │                                                                            
    //                                 │                  ├──TickFunction4 ◄───┘                                                   │                                                                            
    //                                 │                  │                                                                        │                                                                            
    //                                 ├───TickFunction5◄─┘                                                                        │                                                                            
    //                                 │                                                                                           │                                                                            
    //                                 │                                                                                           │                                                                            
    //                                 │                                                                                           │                                                                            
    //                                 │                                                                                           │                                                                            
    //                                 │                          ┌────────────────────────┐                                       │                                                                            
    //                                 └──────────────────────────┤  SpecialTickFunction   │◄──────────────────────────────────────┘                                                                            
    //                                                            └─┬──────────────────────┘                                                                                                                    
    //                                                              │                                                                                                                                           
    //                                                              ├─TickGroup=TG_PrePhysics                                                                                                                   
    //                                                              │                                                                                                                                           
    //                                                              └─EndTickGroup=TG_StartPhysics  
    //                                                                                                              
    // - ***SpecialTickFunction*** is the example!
    TEnumAsByte<enum ETickingGroup> TickGroup;

    /** 
     * defines the tick group that this tick function must finished in
     * these tick group determine the relative order of when objects tick during a frame update
     */
    TEnumAsByte<enum ETickingGroup> EndTickGroup;

    /** internal data structure that contains members only required for a registered tick function */
    // 틱 펑션 컴포넌트는 필수적인 컴포넌트여서 엔진 전체로 생각하면 굉장히 많은 객체가 생성되게 된다.
    // 이런 객체들 중에 TickState 설정에 따라 틱을 사용하는것과 사용하지 않는 것들로 나뉘게 되는데 
    // 사용하지 않는 것들은 설정과 데이터에 메모리를 사용하지 않도록 하기 위해서 틱 펑션에 필요한 대부분의 데이터들을 이렇게 구조체로 만들어 두고
    // 사용하는 객체들에만 아래에 있는 InternalData 데이터에 설정해주도록 한다.
    struct FInternalData
    {
        /** whether the tick function is registered */
        bool bRegistered : 1;

        /** cache whether this function was rescheduled as an interval function during StartParallel */
        // haker: if true, the TickFunction is in CoolingDown list
        // - ***[TickTaskManager] it indicates whether it is in reschedule-list or cooling-down list (we'll see how it works soon)
        bool bWasInterval : 1;

        /** run this tick first within the tick group, presumably to start async tasks that must be completed with this tick group, hiding the latency */
        // haker: TickTaskManager (actually TickTaskSequencer, more precisely FTaskGraph) maintain two queues(?) applying priority (normal + high-priority)
        uint8 bHighPriority : 1;

        /** if false, this tick will run on the game thread, otherwise it will run on any thread in parallel with the game thread and in parallel with other "async ticks" */
        // haker: if true, the tick function will run in task-thread (so running in parallel)
        // - by default(==false), it will run in game-thread
        uint8 bRunOnAnyThread : 1;

        /** the next function in the cooling down list for ticks with an interval */
        // haker: cooling down tick-function list is in a form of linked list
        // 쿨링다운 링크드 리스트의 다음 리스트를 가리킨다.
        FTickFunction* Next;

        /** back pointer to the FTickTaskLevel containing this tick function if it is registered */
        // haker: FTickFunction has similar relationship between AActor and ULevel:
        // - FTickFunction is contained in FTickTaskLevel
        // *** see FTickTaskLevel (goto 047) 
        class FTickTaskLevel* TickTaskLevel;

        /**
         * if TickFrequency is greater than 0 and tick state is CoolingDown, this is the time relative to
         * the element ahead of it in the cooling down list, remaining until the next time this function will tick
         */
        // haker: now we can understand what RelativeTickCooldown is in the code today
        // - for now, try to understand with Diagram concisely:
        // - Diagram:
        // ┌───────────────┐                                                                                                                                                                                        
        // │ TickTaskLevel │                                                                                                                                                                                        
        // └──┬────────────┘                                                                                                                                                                                        
        //    │                                                                                                                                                                                                     
        //    └──AllCoolingDownTickFunctions                                                                                                                                                                        
        //       │                                                                                                                                                                                                  
        //       └───Head: FTickFunction* ◄─────────────────── TickFunction0                          ┌─────────────TickFunction1                              ┌────────────TickFunction2                           
        //                                                      │                                     │              │                                         │             │                                      
        //                                                      ├──TickInterval=5.f                   │              ├──TickInterval=12.f                      │             ├──TickInterval=15.f                   
        //                                                      │                                     │              │                                         │             │                                      
        //                                                      └──InternalData                       │              └──InternalData                           │             └──InternalData                        
        //                                                          │                                 │                  │                                     │                 │                                  
        //                                                          ├─Next: FTickFunction*◄───────────┘                  ├─Next: FTickFunction* ◄──────────────┘                 ├─Next: FTickFunction*             
        //                                                          │                                                    │                                                       │                                  
        //                                                          └─RelativeTickCooldown: float=5.f                    └─RelativeTickCooldown: float=7.f                       └─RelativeTickCooldown: float=3.f  
        //                                                                                                                                                                                                          
        // - we'll see that:
        //   - cooling-down list is actually **sorted!** (how? will see soon!)
        float RelativeTickCooldown;

        /** internal data to track if we have ***started*** visiting this tick function yet this frame */
        // haker: skip! TickVisitedGFrameCounter is related to support prerequisites! (you'll see how soon in the code)
        int32 TickVisitedGFrameCounter;

        /** internal data to track if we have ***finished*** visiting this tick function yet this frame */
        // haker: skip!
        std::atomic<int32> TickQueuedGFrameNumber;

        /** internal data that indicates the tick group we actually started in (it may have been delayed due to prerequisites) */
        // haker: even though TickFunction has Start/EndTickGroup, depending on its prerequisites, tick group can be changed
        TEnumAsByte<enum ETickingGroup> ActualStartTickGroup;

        /** Internal data that indicates the tick group we actually started in (it may have been delayed due to prerequisites) **/
        TEnumAsByte<enum ETickingGroup> ActualEndTickGroup;

        /** pointer to the task, only used during setup; this is often stale */
        // haker: the tick function is wrapped up by TickTaskManager and TickTaskSequencer, but it actually is run by FTaskGraph (we'll NOT cover this in our current course)
        FBaseGraphTask* TaskPointer;
    };

    /** lazily allocated struct that contains the necessary data for a tick function that is registered */
    // haker: why does it separate data as InternalData?
    // - we can achieve the following benefits, splitting hot/cold data
    //   - memory optimization
    //   - performance optimization
    // - think how many tick functions exist: Actor & ActorComponent
    // see FInternalData
    TUniquePtr<FInternalData> InternalData;

    // search (goto 007)
};

namespace ENamedThreads
{
    enum Type : int32
    {
        /** the always-present, named threads are listed next */
        RHIThread,
        GameThread,
        /** the render thread is sometimes the game thread and is sometimes the actual rendering thread */
        ActualRenderingThread = GameThread + 1,
        /** not actually a thread index; means "unknown thread" or "any unnamed thread" */
        AnyThread = 0xff,
    };
}

/** whether to teleport physics body or not */
// 005 - Foundation - UpdateComponentToWorld - ETeleportType
enum class ETeleportType : uint8
{
    /** do not teleport physics body: this means velocity will reflect the movement between initial and final position, and collisions along the way will occur */
    // haker: apply collision and reflect result of collision to velocity
    None,
    /** teleport physics body so that velocity remains the same and no collision occurs */
    // haker: do NOT consider velocity, just teleport remaining velocity
    // - useful initially spawning object or initial placement of object
    TeleportPhysics,
    /** teleport physics body and reset physics state completely */
    // haker: rarely used... cuz, recreating physics state is too expensive
    ResetPhysics,
}

/** enum used to describe what type of collision is enabled on a body */
namespace ECollisionEnabled
{
    enum Type : int
    {
        /** 
         * will not create any representation in physics engine
         * - cannot be used for spaitial queries (raycasts, sweeps, overlaps) or simulation (rigid body, constraints)
         * - best performance possible (especially for moving objects) 
         */
        NoCollision,
        /**
         * only used for spatial queries (raycasts, sweeps, and overlaps)
         * - cannot be used for simulation (rigid body, constraints)
         * - useuful for character movement and things that do not need physical simulation
         * - performance gains by keeping data out of simulation tree
         */
        QueryOnly,
        /**
         * only used only for physics simulation (rigid body, constraints)
         * - cannot be used for spatial queries (raycasts, sweeps, overlaps)
         * - useful for jiggly bits on characters that do not need per bone detection
         * - performance gains by keeping data out of query tree
         */
        PhysicsOnly,
        /**
         * can be used for both spatial queries (raycasts, sweeps, and overlaps) and simulation (rigid body, constraints)
         */
        QueryAndPhysics,
        /**
         * only used for probing the physics simulation (rigidbody, constraints)
         * - cannot be used for spatial queries (raycasts, sweeps, overlaps)
         * - useful for when you want to detect potential physics interactions and pass contact data to hit callbacks or contact modification
         *   - BUT, don't want to physically ract to these contacts
         */
        ProbeOnly,
        /** 
         * can be used for both spatial queries (raycasts, sweeps, overlaps) and probing the physics simulation (rigid body, constraints) 
         * - will not allow for actual physics interaction, but will generate contact data, trigger hit callbacks, and contacts will appear in contact modification
         */
        QueryAndProbe 
    };
}

/** Enum indicating how each type should respond */
enum ECollisionResponse : int
{
	ECR_Ignore,
	ECR_Overlap,
	ECR_Block,
	ECR_MAX,
};

/** 
 * Enum indicating different type of objects for rigid-body collision purposes. 
 */
UENUM(BlueprintType)
enum ECollisionChannel : int
{

	ECC_WorldStatic UMETA(DisplayName="WorldStatic"),
	ECC_WorldDynamic UMETA(DisplayName="WorldDynamic"),
	ECC_Pawn UMETA(DisplayName="Pawn"),
	ECC_Visibility UMETA(DisplayName="Visibility" , TraceQuery="1"),
	ECC_Camera UMETA(DisplayName="Camera" , TraceQuery="1"),
	ECC_PhysicsBody UMETA(DisplayName="PhysicsBody"),
	ECC_Vehicle UMETA(DisplayName="Vehicle"),
	ECC_Destructible UMETA(DisplayName="Destructible"),

	/** Reserved for gizmo collision */
	ECC_EngineTraceChannel1 UMETA(Hidden),

	ECC_EngineTraceChannel2 UMETA(Hidden),
	ECC_EngineTraceChannel3 UMETA(Hidden),
	ECC_EngineTraceChannel4 UMETA(Hidden), 
	ECC_EngineTraceChannel5 UMETA(Hidden),
	ECC_EngineTraceChannel6 UMETA(Hidden),

	ECC_GameTraceChannel1 UMETA(Hidden),
	ECC_GameTraceChannel2 UMETA(Hidden),
	ECC_GameTraceChannel3 UMETA(Hidden),
	ECC_GameTraceChannel4 UMETA(Hidden),
	ECC_GameTraceChannel5 UMETA(Hidden),
	ECC_GameTraceChannel6 UMETA(Hidden),
	ECC_GameTraceChannel7 UMETA(Hidden),
	ECC_GameTraceChannel8 UMETA(Hidden),
	ECC_GameTraceChannel9 UMETA(Hidden),
	ECC_GameTraceChannel10 UMETA(Hidden),
	ECC_GameTraceChannel11 UMETA(Hidden),
	ECC_GameTraceChannel12 UMETA(Hidden),
	ECC_GameTraceChannel13 UMETA(Hidden),
	ECC_GameTraceChannel14 UMETA(Hidden),
	ECC_GameTraceChannel15 UMETA(Hidden),
	ECC_GameTraceChannel16 UMETA(Hidden),
	ECC_GameTraceChannel17 UMETA(Hidden),
	ECC_GameTraceChannel18 UMETA(Hidden),
	
	/** Add new serializeable channels above here (i.e. entries that exist in FCollisionResponseContainer) */
	/** Add only nonserialized/transient flags below */

	// NOTE!!!! THESE ARE BEING DEPRECATED BUT STILL THERE FOR BLUEPRINT. PLEASE DO NOT USE THEM IN CODE

	ECC_OverlapAll_Deprecated UMETA(Hidden),
	ECC_MAX,
};

bool CollisionEnabledHasQuery(ECollisionEnabled::Type CollisionEnabled)
{
    return (CollisionEnabled == ECollisionEnabled::QueryOnly) || 
           (CollisionEnabled == ECollisionEnabled::QueryAndPhysics) ||
           (CollisionEnabled == ECollisionEnabled::QueryAndProbe);
}

/** Specifies which player index will pass input to this actor/component */
namespace EAutoReceiveInput
{
	enum Type : int
	{
		Disabled,
		Player0,
		Player1,
		Player2,
		Player3,
		Player4,
		Player5,
		Player6,
		Player7,
	};
}

namespace ScopeExitSupport
{
    /** 
     * not meant for direct consumption: use ON_SCOPE_EXIT instead 
     * RAII class that calls a lambda when it is destoryed
     */
    template <typename FuncType>
    class TScopeGuard
    {
        TScopeGuard(TScopeGuard&&) = delete;
        TScopeGuard(const TScopeGuard&) = delete;
        TScopeGuard& operator=(TScopeGuard&&) = delete;
        TScopeGuard& operator=(const TScopeGuard&) = delete;
    public:
        // given a lambda, construct an RAII scope guard
        explicit TScopeGuard(FuncType&& InFunc)
            : Func((FuncType&&)InFunc)
        {}

        // cause the lambda to be executed
        ~TScopeGuard()
        {
            Func();
        }
    private:
        FuncType Func;
    };

    struct FScopeGuardSyntaxSupport
    {
        template <typename FuncType>
        TScopeGuard<FuncType> operator+(FuncType&& InFunc)
        {
            return TScopeGuard<FuncType>((FuncType&&)InFunc);
        }
    };
}

#define UE_PRIVATE_SCOPE_EXIT_JOIN(A, B) UE_PRIVATE_SCOPE_EXIT_JOIN_INNER(A, B)
#define UE_PRIVATE_SCOPE_EXIT_JOIN_INNER(A, B) A##B

/**
 * Enables a lambda to be executed on scope exit
 * 
 * Example:
 *  {
 *    FileHandle* Handle = GetFileHandle();
 *    ON_SCOPE_EXIT
 *    {
 *      CloseFile(Handle);
 *    };
 *    
 *    DoSomethingWithFile(Handle);
 *    
 *    // File will be closed automatically no matter how the scope is exited, e.g.:
 *    // * Any return statement.
 *    // * break or continue (if the scope is a loop body).
 *    // * An exception is thrown outside the block.
 *    // * Execution reaches the end of the block.
 *  } 
 */
#define ON_SCOPE_EXIT const auto UE_PRIVATE_SCOPE_EXIST_JOIN(ScopeGuard_, __INLINE__) = ::ScopeExitSupport::ScopeGuardSyntaxSupport() + [&]()