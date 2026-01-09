#include "EngineBaseTypes.h"

// 016 - Foundation - TickTaskManager - FTickContext
// haker: while UWorld::Tick, Tick Task's context is recorded
struct FTickContext
{
    /** delta time to tick */
    float DeltaSeconds;
    /** tick type */
    ELevelTick TickType;
    /** tick group */
    ETickingGroup TickGroup;
    /** current or desired thread */
    ENamedThreads::Type Thread;
    /** the world in which the object being ticked is contained */
    UWorld* World;
};

// 047 - Foundation - CreateWorld * - FTickTaskLevel
// see FTickTaskLevel's member variables (goto 48)
class FTickTaskLevel
{
    /** return true if this tick function is in primary list */
    // 058 - Foundation - CreateWorld * - FTickTaskLevel::HasTickFunction
    bool HasTickFunction(FTickFunction* TickFunction)
    {
        return AllEnabledTickFunctions.Contains(TickFunction)
            || AllDisabledTickFunctions.Contains(TickFunction) 
            // haker: look at FCoolingDownTickFunctionList::Contains()
            // see FCoolingDownTickFunctionList::Contains (goto 59)
            || AllCoolingDownTickFunctions.Contains(TickFunction);
	}

    /** add the tick function to the primary list */
    // 057 - Foundation - CreateWorld * - FTickTaskLevel::AddTickFunction
    void AddTickFunction(FTickFunction* TickFunction)
    {
        // see HasTickFunction (goto 58)
        // haker: absolutely, there should NOT be in any queues of FTickTaskLevel
        check(!HasTickFunction(TickFunction));
        if (TickFunction->TickState == FTickFunction::ETickState::Enabled)
        {
            AllEnabledTickFunctions.Add(TickFunction);
            // haker: NewlySpawnedTickFunctions handles addition operations, but now skip it
            // - we'll cover when we look into the detail of how FTickFunction ticks
            if (bTickNewlySpawned)
            {
                NewlySpawnedTickFunctions.Add(TickFunction);
            }
        }
        else
        {
            check(TickFunction->TickState == FTickFunction::ETickState::Disabled);
            AllDisabledTickFunctions.Add(TickFunction);
        }
    }

    /** remove the tick function from the primary list */
    // 056 - Foundation - CreateWorld * - FTickTaskLevel::RemoveTickFunction
    void RemoveTickFunction(FTickFunction* TickFunction)
    {
        // haker: AllEnabledTickFunctions, AllDisabledTickFunctions, TickFunctionsToReschedule and AllCoolingDownTickFunctions(linked-list) are need to be synchronized
        switch(TickFunction->TickState)
        {
        // haker: note that in our callstack, the TickFunction is not in Enabled state, but see how RemoveTickFunction() works when TickFunction is Enabled
        case FTickFunction::ETickState::Enabled:
            // 활성 상태지만 완전한 활성상태가 아니고 TickFunctionsToReschedule이나 AllCoolingDownTickFunctions 리스트에 들어가서 상태 업데이트를 기다리고 있는 경우 
            if (TickFunction->InternalData->bWasInternal)
            {
                // an enabled function with a tick interval could be in either the enabled or cooling down list
                // haker: 
                // - what is the meaning of AllEnabledTickFunctions.Remove() equals 0?
                //   - ***it is in cooling-down list or in reschedule list
                if (AllEnabledTickFunctions.Remove(TickFunction) == 0)
                {
                    auto FindTickFunctionInRescheduleList = [TickFunction](const FTickScheduleDetails& TSD)
                    {
                        return (TSD.TickFunction == TickFunction);
                    };
                    // haker: note that TickFunctionsToReschedule is the array (linear-search)
                    int32 Index = TickFunctionsToReschedule.IndexOfByPredicate(FindTickFunctionInRescheduleList);
                    bool bFound = Index != INDEX_NONE;

                    // haker: 
                    // 1. remove TickFunction from TickFunctionsToReschedule
                    if (bFound)
                    {
                        TickFunctionsToReschedule.RemoveAtSwap(Index);
                    }

                    // haker:
                    // 2. remove TickFunction from AllCoolingDownTickFunctions
                    // - if we already found the TickFunction in TickFunctionsToReschedule(using bFound), the TickFunction is not in cooling-down list
                    FTickFunction* PrevComparisonFunction = nullptr;
                    FTickFunction* ComparisonFunction = AllCoolingDownTickFunctions.Head;
                    while (ComparisonFunction && !bFound)
                    {
                        if (ComparisonFunction == TickFunction)
                        {
                            // haker: by setting bFound as true, we will be out of while-loop
                            bFound = true;
                            // haker: cooling-down list is singly-linked-list                                          
                            if (PrevComparisonFunction)
                            {
                                // Diagram:
                                //         AllCoolingDownTickFunctions                                                                                                   
                                //          │                                                                                                                            
                                //          │                                                                                                                            
                                //          └───Head: FTickFunction                                                                                                      
                                //               │                  ┌────────────────────────────────────────────────────────────┐                                       
                                //               │                  │                                                            ▼                                       
                                //               └──Next: FTickFunction────────x─────────►Next: FTickFunction                 Next: FTckCuntion                          
                                //                  (PrevComparisonFunction)   ▲          (TickFunction: **Remove)            (TickFunction->InternalData->Next)         
                                //                                             │                                                                                         
                                //                                             │                                                                                         
                                //                                       ***Disconnect         
                                //                                                                       
                                PrevComparisonFunction->InternalData->Next = TickFunction->InternalData->Next;
                            }
                            else
                            {
                                // haker: it starts from AllCoolingDownTickFunctions.Head, so matches the below condition
                                // Diagram:
                                //            AllCoolingDownTickFunctions                             AllCoolingDownTickFunctions                                        
                                //             │                                                       │                                                                 
                                //             │                                                       │                                                                 
                                //             └───Head: FTickFunction                   ───────►      └───Head: FTickFunction                                           
                                //                 (TickFunction)                                          (TickFunction->InternalData->Next)                            
                                //                  │                                                       │                                                            
                                //                  └──Next: FTckCuntionon                                  └──Next: NULL                                                
                                //                     (TickFunction->InternalData->Next)                                                                                
                                check(TickFunction == AllCoolingDownTickFunctions.Head);
                                AllCoolingDownTickFunctions.Head = TickFunction->InternalData->Next;
                            }
                            TickFunction->InternalData->Next = nullptr;
                        }
                        else
                        {
                            // haker: to iterate next element in cooling-down list, update local variables
                            PrevComparisonFunction = ComparisonFunction;
                            ComparisonFunction = ComparisonFunction->InternalData->Next;
                        }
                    }
                    // haker: when TickFunction::TickState == Enabled and it is not in AllEnabledTickFunctions, it should be found in TickFunctionsToReschedule or AllCoolingDownTickFunctions
                    check(bFound);
                }
            }
            else
            {
                // haker:
                // it it is not internal, it should be in AllEnabledTickFunctions
                verify(AllEnabledTickFunctions.Remove(TickFunction) == 1);
            }
            // haker:
            // by looking through how RemoveTickFunction works in Enabled state, we can understand overall TickFunction's life cycle:
            // - when it is in ETickState::Enabled:
            //   1. bWasInternal == true : can(maybe) not be in AllEnabledTickFunctions
            //      1-1. TickFunction is in TickFunctionsToReschedule:
            //           - it means that TickFunction is already out of the cooling-down list
            //      1-2. TickFunction is in the cooling-down list 
            //   2. bWasInternal == false: should be in AllEnabledTickFunctions 
            break;

        case FTickFunction::ETickState::Disabled:
            // haker: when TickState is Disabled, it should be in AllDisabledTickFunctions
            verify(AllDisabledTickFunctions.Remove(TickFunction) == 1);
            break;

        case FTickFunction::ETickState::CoolingDown:
            // haker: when TickFunction is in exact CoolingDown state:
            // 1. TickFunctionsToReschedule
            // 2. AllCoolingDownTickFunctions
            // - similar to the above logic:
            //   - how? 
            //     - leave it in the future when we cover actual TickFunction ticking logic (World::Tick)
            auto FindTickFunctionInRescheduleList = [TickFunction](const FTickScheduleDetails& TSD)
            {
                return (TSD.TickFunction == TickFunction);
            };
            int32 Index = TickFunctionsToReschedule.IndexOfByPredicate(FindTickFunctionInRescheduleList);
            bool bFound = Index != INDEX_NONE;
            if (bFound)
            {
                TickFunctionsToReschedule.RemoveAtSwap(Index);   
            }
            FTickFunction* PrevComparisonFunction = nullptr;
            FTickFunction* ComparisonFunction = AllCoolingDownTickFunctions.Head;
            while (ComparisonFunction && !bFound)
            {
                if (ComparisonFunction == TickFunction)
                {
                    bFound = true;
                    if (PrevComparisonFunction)
                    {
                        PrevComparisonFunction->InternalData->Next = TickFunction->InternalData->Next;
                    }
                    else
                    {
                        check(TickFunction == AllCoolingDownTickFunctions.Head);
                        AllCoolingDownTickFunctions.Head = TickFunction->InternalData->Next;
                    }
                    if (TickFunction->InternalData->Next)
                    {
                        // haker: relative cool-down value is based on prev-TickFunction
                        // - the current TickFunction is going to be popped, so we add the removing TickFunction's RelativeTickCooldown to next-TickFunction
                        TickFunction->InternalData->Next->InternalData->RelativeTickCooldown += TickFunction->InternalData->RelativeTickCooldown;
                        TickFunction->InternalData->Next = nullptr;
                    }
                }
                else
                {
                    PrevComparisonFunction = ComparisonFunction;
                    ComparisonFunction = ComparisonFunction->InternalData->Next;
                }
            }
            check(bFound); // otherwise you changed TickState while the tick function was registered. Call SetTickFunctionEnable instead.
            break;
        }
        if (bTickNewlySpawned)
        {
            NewlySpawnedTickFunctions.Remove(TickFunction);
        }

        // haker: we safely removed FTickFunction from all queues in TickTaskLevel
        // search (goto 56)
    }

    /** puts a tick function into the cooldown state */
    // 021 - Foundation - TickTaskManager - FTickTaskLevel::ScheduleTickFunctionCooldowns
    void ScheduleTickFunctionCooldowns()
    {
        if (TickFunctionsToReschedule.Num() > 0)
        {
            // haker: first sort TickFunctionsToReschedule in increasing order:
            // [4, 5, 1, 2] -> [1, 2, 4, 5]
            TickFunctionsToReschedule.Sort([](const FTickScheduleDetails& A, const FTickScheduleDetails& B)
            {
                return A.Cooldown < B.Cooldown;
            });

            // haker: before getting into logic, try to understand the logic by diagrams:
            //                                                                                                                                                                                                          
            //            ┌──TickFunctionsToReschedule──────────────────────────────────────────────────────────────────────────────────┐                                                                               
            //            │                                                                                                             │                                                                               
            //            │     ┌──────────────┐     ┌──────────────┐     ┌──────────────┐      ┌──────────────┐     ┌──────────────┐   │                                                                               
            //            │     │TickFunction10│     │TickFunction11│     │TickFunction12│      │TickFunction13│     │TickFunction14│   │                                                                               
            //            │     │              ◄─────┤              ◄─────┤              ◄──────┤              ◄─────┤              │   │                                                                               
            //            │     │Cooldown=5.f  │     │Cooldown=7.f  │     │Cooldown=10.f │      │Cooldown=13.f │     │Cooldown=15.f │   │                                                                               
            //            │     └──────┬───────┘     └─────────────┬┘     └──────┬───────┘      └───────┬──────┘     └───────┬──────┘   │                                                                               
            //            │            │                           │             │                      │                    │          │                                                                               
            //            └────────────┼───────────────────────────┼─────────────┼──────────────────────┼────────────────────┼──────────┘                                                                               
            //                         │                           │             │                      │                    │                                                                                          
            //                         └─────────────────────────┐ │             └─────────────┐        └─────────────────┐  │                                                                                          
            //                                                   │ │                           │                          │  │                                                                                          
            //            AllCoolingDownTickFunctions            │ │                           │                          │  │                                                                                          
            //             │   ┌──────┐    ┌───────────────────┐ ▼ ▼   ┌───────────────────┐   ▼   ┌────────────────────┐ ▼  ▼                                                                                          
            //             └───┤ Head ◄────┤ TickFunction0     ◄───────┤ TickFunction1     ◄───────┤ TickFunction2      ◄───────                                                                                        
            //                 └──────┘    │                   │       │                   │       │                    │                                                                                               
            //                             │    Relative=3.f   │       │    Relative=6.f   │       │    Relative=2.f    │                                                                                               
            //                             │    Cumulative=3.f │       │    Cumulative=9.f │       │    Cumulative=11.f │                                                                                               
            //                             └───────────────────┘       └───────────────────┘       └────────────────────┘                                                                                               
            
            // haker: (1) deal with inserting TickFunction in reschedule-list to AllCoolingDownTickFunctions
            int32 RescheduleIndex = 0;
            float CumulativeCooldown = 0.f;
            FTickFunction* PrevComparisonTickFunction = nullptr;
            FTickFunction* ComparisonTickFunction = AllCoolingDownTickFunctions.Head;
            // haker: we iterates two list:
            // - our source iteration is AllCoolingDownTickFunctions
            // - our target iteration is TickFunctionsToReschedule
            //   - only when we succeeded to insert tick function to reschedule, we iterate next tick function in reschedule-list
            while (ComparisonTickFunction && RescheduleIndex < TickFunctionsToReschedule.Num())
            {
                const float CooldownTime = TickFunctionsToReschedule[RescheduleIndex].Cooldown;

                // haker: CumulativeCooldown + RelativeTickCooldown == TickFunction's Cooldown in coolingdown-list
                if ((CumulativeCooldown + ComparisonTickFunction->InternalData->RelativeTickCooldown) > CooldownTime)
                {
                    // haker: when TickFunction in reschedule list can be inserted between PrevComparisonTickFunction and ComparisonTickFunction
                    FTickFunction* TickFunction = TickFunctionsToReschedule[RescheduleIndex].TickFunction;

                    // haker: if tick function is in reschedule-list or coolingdown-list, bWasInternal is 'true'
                    check(TickFunction->InternalData->bWasInternal);

                    // haker: if tick function is Disabled, it should not be added to reschedule-list
                    if (TickFunction->TickState != FTickFunction::ETickState::Disabled)
                    {
                        // haker: now insert the tick function into coolingdown-list
                        // - link the current tick function in reschedule-list to the position between PrevComparisonTickFunction and ComparisonTickFunction 
                        TickFunction->TickState = FTickFunction::ETickState::CoolingDown;
                        TickFunction->InternalData->RelativeTickCooldown = CooldownTime - CumulativeCooldown;

                        if (PrevComparisonTickFunction)
                        {
                            PrevComparisonTickFunction->InternalData->Next = TickFunction;
                        }
                        else
                        {
                            check(ComparisonTickFunction == AllCoolingDownTickFunctions.Head);
                            AllCoolingDownTickFunctions.Head = TickFunction;
                        }

                        TickFunction->InternalData->Next = ComparisonTickFunction;
                        PrevComparisonTickFunction = TickFunction;
                        // haker: pay attention to RelativeTickCoolodwn for ComparisonTickFunction:
                        // - PrevComparisonTickFunction's RelativeTickCooldown is no need to be update 
                        ComparisonTickFunction->InternalData->RelativeTickCooldown -= TickFunction->InternalData->RelativeTickCooldown;
                        CumulativeCooldown += TickFunction->InternalData->RelativeTickCooldown;
                    }

                    // haker: only if we successfully insert tick function in reschedule-list then we update its index
                    ++RescheduleIndex;
                }
                else
                {
                    // haker: the location between PrevComparisonTickFunction and ComparisonTickFunction is not appropriate to be inserted for current tick function in reschedule-list
                    // - move next tick function in coolingdown-list
                    CumulativeCooldown += ComparisonTickFunction->InternalData->RelativeTickCooldown;
                    PrevComparisonTickFunction = ComparisonTickFunction;
                    ComparisonTickFunction = ComparisonTickFunction->InternalData->Next;
                }
            }

            // haker: (2) rest of tick functions in reschedule-list to the end of AllCoolingDownTickFunctions
            // - PrevComparisonTickFunction and ComparisonTickFunction is cached previously while-iteration
            //   - they indicate the end of coolingdown-list ***if tick functions are left from TickFunctionsToReschedule***
            for (; RescheduleIndex < TickFunctionsToReschedule.Num(); ++RescheduleIndex)
            {
                FTickFunction* TickFunction = TickFunctionsToReschedule[RescheduleIndex].TickFunction;
                if (TickFunction->TickState != FTickFunction::ETickState::Disabled)
                {
                    // haker: explain line by line how it is calculated
                    const float CooldownTime = TickFunctionsToReschedule[RescheduleIndex].Cooldown;
                    TickFunction->TickState = FTickFunction::ETickState::CoolingDown;
                    TickFunction->InternalData->RelativeTickCooldown = CooldownTime - CumulativeCooldown;
                    TickFunction->InternalData->Next = nullptr;

                    if (PrevComparisonTickFunction)
                    {
                        PrevComparisonTickFunction->InternalData->Next = TickFunction;
                    }
                    else
                    {
                        AllCoolingDownTickFunctions.Head = TickFunction;
                    }
                    PrevComparisonTickFunction = TickFunction;
                    CumulativeCooldown += TickFunction->InternalData->RelativeTickCooldown;
                }
            }

            // haker: we finished adding all entries in reschedule-list to coolingdown-list:
            // - from here, we can know a few things:
            //   1. TickFunction's RelativeTickCooldown should be > 0
            //   2. coolingdown-list is sorted: kind of insertion sort manner
            //   3. it is VERY easy to get candidate-list to be inserted from coolingdown-list to enabled-list:
            //      - can you guess how to do that?
            //         - iterate coolingdown-list until we reach to the moment that cumulative time is bigger than DeltaSeconds!
            TickFunctionsToReschedule.Reset();
        }
    }

    /** queues the ticks for this tick */
    // 020 - Foundation - TickTaskManager - FTickTaskLevel::StartFrame
    // - the comment says that this function queues ticks -> BUT, it is NOT!
    // - it pushes tick functions in TickFunctionsToReschedule to AllCoolingDownTickFunction
    //   - we'll see ***how AllCoolingDownTickFunction is structured***
    int32 StartFrame(const FTickContext& InContext)
    {
        Context.TickGroup = ETickingGroup(0);
        // haker: pay attention to Context.DeltaSeconds
        // - based on this DeltaSeconds, we'll push tick functions in cooling-down list to AllEnabledTickFunctions
        Context.DeltaSeconds = InContext.DeltaSeconds;
        Context.TickType = InContext.TickType;
        Context.Thread = ENamedThreads::GameThread;
        Context.World = InContext.World;

        // haker: see? when we StartFrame(), bTickNewlySpawned is set to 'true'
        bTickNewlySpawned = true;

        // haker: here, we are NOT pushing tick functions which are enabled in coolingdown-list to AllEnabledTickFunctions
        // - we just counting them in CooldownTicksEnabled
        int32 CooldownTicksEnabled = 0;
        {
            // make sure all scheduled tick functions that are ready are put into the cooling down state 
            // see FTickTaskLevel::ScheduleTickFunctionCooldowns (goto 021 : TickTaskManager)
            ScheduleTickFunctionCooldowns();

            // determine which cooled down ticks will be enabled this frame
            // haker: see? you guess is right
            float CumulativeCooldown = 0.f;
            FTickFunction* TickFunction = AllCoolingDownTickFunctions.Head;
            while (TickFunction)
            {
                // haker: we should see the benefit of using RelativeTickCooldown:
                // - compared to use GlobalTickCooldown, updating cooldown is simple, just update HEAD's RelativeTickCooldown 
                // - don't just assume that it will work!!! ESPECIALLY related to time, you should understand completely!
                //   - see the diagram:
                //     │                               │                                                                                                                                                
                //     │◄─────────DeltaSecond─────────►│                                                                                                                                                
                //     │                               │                                                                                                                                                
                //     │                       │       │                                                                                                                                                
                //     │◄─────Cumulative──────►│       │                                                                                                                                                
                //     │                       │       │               │                                                                                                                                
                //     ├─────TickFunction0─────┼─────TickFunction1─────┤                                                                                                                                
                //     │                       │       │               │                                                                                                                                
                //     │                       │◄──┬──►│◄─────────┬───►│                                                                                                                                
                //     │                       │   │   │          │    │                                                                                                                                
                //                                 │              │                                                                                                                                     
                //                  (DeltaSecond-Cumulative)      │                                                                                                                                     
                //                                                │                                                                                                                                     
                //                                        TickFunction1's NEW Relative                                                                                                                  
                //                                          =TickFunction1's Relative-(DeltaSecond-Cumulative)                                                                                          
                if (CumulativeCooldown + TickFunction->InternalData->RelativeTickCooldown >= Context.DeltaSeconds)
                {
                    TickFunction->InternalData->RelativeTickCooldown -= (Context.DeltaSeconds - CumulativeCooldown);
                    break;
                }

                // haker: mark tick function as Enabled
                // - later, we'll extract marked tick functions in coolingdown-list and append it to enabled-list
                TickFunction->TickState = FTickFunction::ETickState::Enabled;

                // haker: move next tick function in coolingdown-list
                CumulativeCooldown += TickFunction->InternalData->RelativeTickCooldown;
                TickFunction = TickFunction->InternalData->Next;
                ++CooldownTicksEnabled;
            }
        }

        return AllEnabledTickFunctions.Num() + CooldownTicksEnabled;
    }

    // 032 - Foundation - TickTaskManager - FTickTaskLevel::RescheduleForInterval
    void RescheduleForInterval(FTickFunction* TickFunction, float InInterval)
    {
        // haker: note that we set bWasInterval as 'true'
        TickFunction->InternalData->bWasInterval = true;
        TickFunctionsToReschedule.Add(FTickScheduleDetails(TickFunction, InInterval));
    }

    /** queue all tick functions for execution */
    // 022 - Foundation - TickTaskManager - FTickTaskLevel::QueueAllTicks
    void QueueAllTicks()
    {
        FTickTaskSequencer& TTS = FTickTaskSequencer::Get();
        
        // haker: (1) queue tick function for enabled-list
        for (TSet<FTickFunction*>::Iterator It(AllEnabledTickFunction); It; ++It)
        {
            FTickFunction* TickFunction = *It;

            // see FTickFunction::QueueTickFunction (goto 023)
            TickFunction->QueueTickFunction(TTS, Context);

            // haker: if tick function has TickInterval, we need to add this tick function to reschedule-list
            // - this frame, tick() will be executed
            // - next frame, tick function will be inserted into coolingdown-list
            if (TickFunction->TickInterval > 0.f)
            {
                // haker: remove tick function from AllEnabledTickFunction and then add it to reschedule list
                It.RemoveCurrent();
                // see FTickTaskLevel::RescheduleForInterval (goto 032)
                RescheduleForInterval(TickFunction, TickFunction->TickInterval);
            }
        }

        // haker: (2) queue tick functions in coolingdown-list which marked as Enabled on FTickTaskLevel::StartFrame()
        float CumulativeCooldown = 0.f;
        while (FTickFunction* TickFunction = AllCoolingDownTickFunctions.Head)
        {
            // haker: we already marked all tick functions to be enabled in coolingdown-list
            if (TickFunction->TickState == FTickFunction::ETickState::Enabled)
            {
                CumulativeCooldown += TickFunction->InternalData->RelativeTickCooldown;
                TickFunction->QueueTickFunction(TTS, Context);

                // haker: see the diagram:
                //     │                               │                                                                                                                                                
                //     │◄─────────DeltaSecond─────────►│                                                                                                                                                
                //     │                               │                                                                                                                                                
                //     │                       │       │                                                                                                                                                
                //     │◄─────Cumulative──────►│       │                                                                                                                                                
                //     │                       │       │               │                                                                                                                                
                //     ├─────TickFunction0─────┼─────TickFunction1─────┤                                                                                                                                
                //     │                       │       │               │                                                                                                                                
                //     │                       │◄──┬──►│◄─────────┬───►│                                                                                                                                
                //     │                       │   │   │          │    │                                                                                                                                
                //                                 │              │                                                                                                                                     
                //                  (DeltaSecond-Cumulative)      │                                                                                                                                     
                //                                                │                                                                                                                                     
                //                                        TickFunction1's NEW Relative                                                                                                                  
                //                                          =TickFunction1's Relative-(DeltaSecond-Cumulative)    
                RescheduleForInterval(TickFunction, TickFunction->TickInterval - (Context.DeltaSeconds - CumulativeCooldown));

                // haker: keep update Head until we reach ETickState::CoolingDown
                AllCoolingDownTickFunctions.Head = TickFunction->InternalData->Next;
            }
            else
            {
                // haker: we finished pop-front all enabled functions in coolingdown-list
                break;
            }
        }
    }

    struct FTickScheduleDetails
    {
        FTickFunction* TickFunction;
        float Cooldown;
        bool bDeferredRemove;
    };

    // haker: see the member-variable, 'Head'
    struct FCoolingDownTickFunctionList
    {
        // 059 - Foundation - CreateWorld - FCoolingDownTickFunctionList::Contains
        bool Contains(FTickFunction* TickFunction) const
        {
            // haker: for junior, the data structure is important to understand the code
            FTickFunction* Node = Head;
            while (Node)
            {
                if (Node == TickFunction)
                {
                    return true;
                }
                Node = Node->InternalData->Next;
            }
            return false;
        }

        // haker: we saw that InternalData->Next is the node for cooling-down list
        FTickFunction* Head;
    };

    // 048 - Foundation - CreateWorld * - FTickTaskLevel's member variables
    // 015 - Foundation - TickTaskManager

    // 각 틱들을 이렇게 각각 상태에 따라 저장하면서 관리한다.
    /** primary list of enabled tick functions */
    TSet<FTickFunction*> AllEnabledTickFunctions;

    /** primary list of enabled tick functions */
    // see FCoolingDownTickFunctionList
    // haker: what is cooldown initial value?
    // - FTickFunction::TickInterval
    FCoolingDownTickFunctionList AllCoolingDownTickFunctions;

    /** primary list of disabled tick functions */
    TSet<FTickFunction*> AllDisabledTickFunctions;

    /** utility array to avoid memory reallocation when collecting functions to reschedule */
    // see FTickScheduleDetails
    // haker: FTickScheduleDetails is separate data for each tick-function to describe how it will be scheduled
    // - TickFunctionsToReschedule is the holder for pushing tick functions to coolingdown-list
    // - TickFunction is ready to be in coolingdown-list (TickInterval is specified)
    //   1. -> TickFunctionsToReschedule
    //   2. -> AllCoolingDownTickFunctions
    //   - the detail of logic will be covered in later
    TArrayWithThreadsafeAdd<FTickScheduleDetails> TickFunctionsToReschedule;

    /** list of tick functions added during a tick phase; these items are also duplicated in AllLiveTickFunctions for future frames */
    // haker: holder for new tick functions while bTickNewlySpawnd is 'true' 
    // - it means that new tick functions are added while executing the tick function
    TSet<FTickFunction*> NewlySpawnedTickFunctions;
    
    /** true during the tick phase, when true, tick function adds also go to the newly spawned list */
    bool bTickNewlySpawned;

    // haker: who is the owner of FTickTaskLevel to create/destroy?
    // see ULevel::ULevel() constructor (goto 49)
    // - ULevel is the owner of TickTaskLevel
    // - TickTaskLevel always follows ULevel's life-cycle
    // ULevel안에 TickTaskLevel이 존재한다.

    // haker: from here we can know two things:
    // 1. FTickFunction -> FTickTaskLevel:
    //   - we can see the abstraction between Actor,ActorComponent and TickFunction
    //   - Diagram:
    //          Level─────────────────────────────────►TickTaskLevel                                            
    //           │                                      │                                                       
    //           ├──Actor0────────────────────────────► ├──TickFunction0                                        
    //           │   │                                  │                                                       
    //           │   ├──ActorComponent0───────────────► ├──TickFunction1                                        
    //           │   │                                  │                                                       
    //           │   └──ActorComponent1───────────────► ├──TickFunction2                                        
    //           │                                      │                                                       
    //           └──Actor1────────────────────────────► ├──TickFunction3                                        
    //               │                                  │                                                       
    //               └──ActorComponent2───────────────► └──TickFunction4                                        
    //
    // 2. FTickFunction cycle:
    //   - Diagram: 
    //     haker: see this diagram just try to understand general pattern of tick-function life-cycle      
    //     - it is brief conceptual cycle (we'll see the actual cycle of tick function)                                                                                                                                                                                                       
    //        ┌──TickFunction Cycle──────────────────────────────────────────────────┐                          
    //        │                          frequency is NOT set                        │                          
    //        │                        ┌───┐                                         │                          
    //        │                        │   │                                         │                          
    //        │                        │   │                                         │                          
    //        │    ┌──────────┐    ┌───┴───▼─┐    ┌─────────────┐    ┌─────────┐     │                          
    //        │    │ Register ├────► Enabled ├────► CoolingDown ├────► Disable │     │                          
    //        │    └──────────┘    └───▲─────┘    └──────┬──────┘    └─────────┘     │                          
    //        │                        │                 │                           │                          
    //        │                        └─────────────────┘                           │                          
    //        │                           frequency is set                           │                          
    //        │                                                                      │                          
    //        └──────────────────────────────────────────────────────────────────────┘                          
    // search (goto 47)

    /** tick context */
    // see FTickContext (goto 016: TickTaskManager)
    FTickContext Context;
};

/**
 * class that handles the actual tick tasks and starting and completing tick groups
 * helper class define the task of ticking a component 
 */
// 027 - Foundation - TickTaskManager - FTickFunctionTask
// - see FTickFunctionTask::DoTask (goto 028 : TickTaskManager)
class FTickFunctionTask
{
    /** AActor to tick */
    FTickFunction* Target;
    /** tick context, here thread is desired execution thread */
    FTickContext Context;
    
    FTickFunctionTask(FTickFunction* InTarget, const FTickContext* InContext)
        : Target(InTarget)
        , Context(*InContext)
    {}

    /** actual execute the tick */
    // 028 - Foundation - TickTaskManager FTickFunctionTask::DoTask
    void DoTask(ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
    {
        // haker: TGraphTask will execute DoTask():
        // - if you use this class, but not defined DoTask() method, TGraphTask will gives compile-error, it doesn't find DoTask() method
        if (Target->IsTickFunctionEnabled())
        {
            // CalculateDeltaTime just returns Context.DeltaSeconds
            // see FActorTickFunction::ExecuteTick (goto 029)
            Target->ExecuteTick(Target->CalculateDeltaTime(Context), Context.TickType, CurrentThread, MyCompletionGraphEvent);
        }
        // this is stale and a good time to clear it for safety
        // haker: clear-out TaskPointer, cuz we finished task
        Target->InternalData->TaskPointer = nullptr;
    }
};

/** class that handles the actual tick tasks and starting and completing tick groups */
// 014 - Foundation - TickTaskManager - FTickTaskSequencer
// haker: TickTaskSequencer manages TaskGraph's event handle of tick function for all tick groups
// - It make sure all tick groups are joined properly
// see TickTaskSequencer's member variables (goto 015 : TickTaskManager)
class FTickTaskSequencer
{
    /** singleton to retrieve the global tick task sequencer */
    static FTickTaskSequencer& Get()
    {
        static FTickTaskSequencer SingletonInstance;
        return SingletonInstance;
    }

    // 018 - Foundation - TickTaskManager - FTickTaskManager::WaitForCleanup
    void WaitForCleanup()
    {
        // haker: as we saw, CleanupTasks is the container of graph-event ref to wait for cleaning up TGraphTask
        if (CleanupTasks.Num() > 0)
        {
            // haker: we'll not cover FTaskGraph, WaitUntilTaskComplete() is straightforward to understand what it does for us
            // - CleanupTasks are done, so we can clear the container
            FTaskGraphInterface::Get().WaitUntilTaskComplete(CleanupTasks, ENamedThreads::GameThread);
            CleanupTasks.Reset();
        }
    }

    /** reset the internal state of the object at the start of a frame */
    // 017 - Foundation - TickTaskManager - FTickTaskSequencer::StartFrame
    void StartFrame()
    {
        // see WaitForCleanup (goto 018 : TickTaskManager)
        WaitForCleanup();

        // haker: note that TickCompletionEvents, TickTasks and HiPriTickTasks are cleared at StartFrame() (NOT in EndFrame())
        for (int32 Index = 0; Index < TG_MAX; Index++)
        {
            TickCompletionEvents[Index].Reset();
            for (int32 IndexInner = 0; IndexInner < TG_MAX; IndexInner++)
            {
                TickTasks[Index][IndexInner].Reset();
                HiPriTickTasks[Index][IndexInner].Reset();
            }
        }

        // haker: to start new frame, set the very first TickingGroup(== TG_PrePhysics)
        WaitForTickGroup = (ETickingGroup)0;
    }

    /** start a component tick task */
    // 026 - Foundation - TickTaskManager - FTickTaskSequencer::StartTickTask
    void StartTickTask(const FGraphEventArray* Prerequisites, FTickFunction* TickFunction, const FTickContext& TickContext)
    {
        FTickContext UseContext = TickContext;

        bool bIsOriginalTickGroup = (TickFunction->InternalData->ActualStartTickGroup == TickFunction->TickGroup);
        
        // haker: to make tick function running in parallel, it should meet bIsOriginalTickGroup == true
        // - otherwise, it will result in unexpected result
        // - bRunOnAnyThread is the option you can run tick function in parallel
        if (TickFunction->bRunOnAnyThread && bIsOriginalTickGroup)
        {
            if (TickFunction->bHighPriority)
            {
                UseContext.Thread = CPrio_HiPriAsyncTickTaskPriority.Get();
            }
            else
            {
                UseContext.Thread = CPrio_NormalAsyncTickTaskPriority.Get();
            }
        }
        else
        {
            // haker: otherwise, just tick function run in game thread
            UseContext.Thread = ENamedThreads::SetTaskPriority(ENamedThreads::GameThread, TickFunction->bHighPriority ? ENamedThreads::HighTaskPriority : ENamedThreads::NormalTaskPriority);
        }

        // haker: see FTickFunctionTask (goto 027)
        // - ConstructAndHold vs. ConstructAndDispatchWhenReady
        //   - try to understand them in Diagram:                                                                                                                                           
        //    ┌─────────────────────────┐                                                                                                                       
        //    │        TaskQueue        │                                   ┌───────────────┐                                                                   
        //    ├─────────────────────────┤                                ┌──┤ TickFunction0 │                                                                   
        //    │                         │                                │  └───────────────┘   ┌───Prerequisite[1,]                                            
        //    │ ┌─────────────────────┐ │                                │                      │                                                               
        //    │ │  TickFunction0      ◄─┼───┬────ConstructAndDispatch────┤  ┌───────────────┐   │   ┌───────────────┐                                           
        //    │ └─────────────────────┘ │   │                            └──┤ TickFunction1 ◄───┴───┤ TickFunction2 │                                           
        //    │                         │   │                               └───────────────┘       └───────────────┘                                           
        //    │ ┌─────────────────────┐ │   │                                                           TickFunction2 can't be pushed into TaskQueue            
        //    │ │  TickFunction1      ◄─┼───┘                               ┌───────────────┐           (Even though it is called by ConstructAndDispatch)      
        //    │ └─────────────────────┘ │                                 ┌─┤ TickFunction3 │                                                                   
        //    │                         │                                 │ └───────────────┘                                                                   
        //    │                       ◄─┼────┬───────  ConstructAndHold ◄─┤                                                                                     
        //    │                         │    │                            │ ┌───────────────┐                                                                   
        //    │                         │    │                            └─┤ TickFunction4 │                                                                   
        //    └─────────────────────────┘    │                              └───────────────┘                                                                   
        //                                   │                                                                                                                  
        //                                 Unlock()                                                                                                             
        //                                  :Only when Unlock() called                                                                                          
        //                                   tick function will be pushed into TaskQueue  
        //
        // Q1) How does TickFunction2 will be executed?  
        #pragma region
        // Q1. by subsequents container, checking 'NumberOfPrerequistitesOutstanding == 0'
        //     - see PrerequisitesComplete()
        #pragma endregion                                                                    
                                                                                                                                                      
        // haker: the detail of TGraphTask is skipped 
        // - who does call the Unlock for this task function? -> later we'll know!                                                                                                                                        
        TickFunction->InternalData->TaskPointer = TGraphTask<FTickFunctionTask>::CreateTask(Prerequisites, TickContext.Thread).ConstructAndHold(TickFunction, &UseContext, false, false);
    }

    /** add a completion handle to a tick group */
    // 031 - Foundation - TickTaskManager - FTickTaskSequencer::AddTickTaskCompletion
    void AddTickTaskCompletion(ETickingGroup StartTickGroup, ETickingGroup EndTickGroup, TGraphTask<FTickFunctionTask>* Task, bool bHiPri)
    {
        // haker: HiPriTickTasks and TickTasks are holder for TGraphTask<>* classified by [Start|End]TickGroup
        if (bHiPri)
        {
            HiPriTickTasks[StartTickGroup][EndTickGroup].Add(Task);
        }
        else
        {
            TickTasks[StartTickGroup][EndTickGroup].Add(Task);
        }

        // haker: cache all GraphEvents to join
        new (TickCompletionEvents[EndTickGroup]) FGraphEventRef(Task->GetCompletionEvent());
    }

    /** start a component tick task and add the completion handle */
    // 025 - Foundation - TickTaskManager - FTickTaskSequencer::QueueTickTask
    void QueueTickTask(const FGraphEventArray* Prerequisites, FTickFunction* TickFunction, const FTickContext& TickContext)
    {
        // see FTickTaskSequencer::StartTickTask (goto 026 : TickTaskManager)
        StartTickTask(Prerequisites, TickFunction, TickContext);

        // see FTickTaskSequencer::AddTickTaskCompletion (goto 031 : TickTaskManager)
        TGraphTask<FTickFunctionTask>* Task = (TGraphTask<FTickFunctionTask>*)TickFunction->InternalData->TaskPointer;
        AddTickTaskCompletion(TickFunction->InternalData->ActualStartTickGroup, TickFunction->InternalData->ActualEndTickGroup, Task, TickFunction->bHighPriority);
    }

    // 037 - Foundation - TickTaskManager - FTickTaskSequencer::ResetTickGroup
    void ResetTickGroup(ETickingGroup WorldTickGroup)
    {
        // haker: it just looks like one line of code, but it is heavy if more than 50
        TickCompletionEvents[WorldTickGroup].Reset();
    }

    /** release the queued ticks for a given tick group and process them */
    // 035 - Foundation - TickTaskManager - FTickTaskSequencer::ReleaseTickGroup
    void ReleaseTickGroup(ETickingGroup WorldTickGroup, bool bBlockTillComplete)
    {
        {
            if (CVarAllowAsyncTickDispatch.GetValueOnGameThread() == 0)
            {
                // haker: single-thread version
                // see FTickTaskSequencer::DispatchTickGroup (goto 036)
                DispatchTickGroup(ENamedThreads::GameThread, WorldTickGroup);
            }
            else
            {
                // dispatch the tick group on another thread, that way, the game thread can be processing ticks while ticks are being queued by another thread
                // haker: pay attention to 'ConstructAndDispatchWhenReady':
                // - FDispatchTickGroupTask is the wrapper task for DispatchTickGroup()
                // - we run this task right away! and wait until it is finished
                FTaskGraphInterface::Get().WaitUntilTaskCompletes(
				    TGraphTask<FDipatchTickGroupTask>::CreateTask(nullptr, ENamedThreads::GameThread).ConstructAndDispatchWhenReady(*this, WorldTickGroup));
            }
        }

        if (bBlockTillComplete)
        {
            // haker: what if WorldTickGroup == TG_StartPhysics?
            // - TG_PrePhysics and TG_StartPhysics will wait until TickFunctionTasks in those tick groups are completed
            for (ETickingGroup Block = WaitForTickGroup; Block <= WorldTickGroup; Block = ETickingGroup(Block + 1))
            {
                if (TickCompletionEvents[Block].Num())
                {
                    // haker: we wait!
                    // - BUT it processes all pending tasks in the queue for game thread
                    FTaskGraphInterface::Get().WaitUntilTasksComplete(TickCompletionEvents[Block],  ENamedThreads::GameThread);

                    // haker: if TickCompletionEvents is < 50, just reset right away
                    // - otherwise, create new task to reset TickGroupTask to reduce performance impacts
                    if (Block == TG_NewlySpawned || TickCompletionEvents[Block].Num() < 50)
                    {
                        // see FTickTaskSequencer::ResetTickGroup (goto 037)
                        ResetTickGroup(Block);
                    }
                    else
                    {
                        // haker: see? we add FResetTickGroupTask in CleanupTasks:
                        // - as we saw, we'll sync these tasks on StartFrame()
                        CleanupTasks.Add(TGraphTask<FResetTickGroupTask>::CreateTask(nullptr, ENamedThreads::GameThread).ConstructAndDispatchWhenReady(*this, Block));
                    }
                }
            }
            // don't advance for newly spawned
            WaitForTickGroup = ETickingGroup(WorldTickGroup + (WorldTickGroup == TG_NewlySpawned ? 0 : 1)); 
        }
        else
        {
            // since this is used to soak up some async time for another task (physics), we should process whatever we have now
            // haker: ProcessThreadUntilIdle() process all pending tasks in the queue (in the game thread)
            // - it calls same function, ProcessTasksNamedThread()
            // - what is difference between WaitUntilTasksComplete() and ProcessThreadUntilIdle()
            //   - see the Diagram:                                                                                     
            //    ┌─────────────┐       ┌─────────────┐         ┌─────────────┐        ┌─────────────┐ 
            //    │TickFunction4◄───────┤TickFunction3◄────┬────┤TickFunction1◄────────┤TickFunction0│ 
            //    │ bAnyThread: │       │ bAnyThread: │    │    │ bAnyThread: │        │ bAnyThread: │ 
            //    │   False     │       │   False     │    │    │   False     │        │   False     │ 
            //    └─────────────┘       └─────────────┘    │    └─────────────┘        └─────────────┘ 
            //                                             │                                           
            //                                             │                                           
            //                                             │                                           
            //                                             │                                           
            //                          ┌─────────────┐    │                                           
            //                          │TickFunction2◄────┘                                           
            //                          │ bAnyThread: │                                                
            //                          │ **True**    │                                                
            //                          └─────────────┘                                                
            //   - TickFunction2 is running on task thread
            //   - rest of tick functions are running on game thread
            //   - TickFunction2 is heavy, so it will complete after other tick functions are finished
            //   1. WaitUntilTasksComplete():
            //      - all tick functions are finished (it wait TickFunction2)
            //      - Completed TickFunction: [TickFunction4, TickFunction3, TickFunction2, TickFunction1, TickFunction 0]
            //   2. ProcessThreadUntilIdle():
            //      - it will NOT wait until TickFunction2 finshes
            //      - Completed TickFunction: [TickFunction4, TickFunction3]
            // - Can you see the difference?
            //   - for now, before we cover FTaskGraphImplementation, end up here~ :)
            FTaskGraphInterface::Get().ProcessThreadUntilIdle(ENamedThreads::GameThread);
        }
    }

    /** end a tick frame */
    // 038 - Foundation - TickTaskManager - TickTaskManager::EndFrame()
    void EndFrame()
    {
        ScheduleTickFunctionCooldowns();

        // haker: note that TickNewlySpawned is set as false
        bTickNewlySpawned = false;
    }

    /** completion handles for each phase of ticks */
    // haker: we are NOT covering TaskGraph in engine code:
    // - but, we need to understand FGraphEventRef
    // - FGraphEventRef is similar to Future object in Async/Await
    //   - FGraphEventRef is used to check whether task is finished or not
    //   - FGraphEventRef is used to wait until its task is finished
    // - as we saw previous figure, TickCompletionEvents has all graph-events for all tick functions in each tick-group
    TArrayWithThreadsafeAdd<FGraphEventRef, TInlineAllocator<4>> TickCompletionEvents[TG_MAX];

    // haker: we are not covering TGraphTask, FGraphEventRef and FTaskGraph in unreal for now

    /** HiPri held tasks for each tick group */
    // haker: TGraphTask<TaskFunction> is the unit of task in task-graph
    // - TTS(TickTaskSequencer) has HiPri/Normal queues
    // - note that:
    //   - it manages start/end tick group of tick functions as 2D array
    //   - try to understand with the Diagram:
    //     ┌───────────────────┬─────────────────────┬────────────────────────┬────────────────────┐                                                                                                   
    //     │  Start\End        │    TG_PrePhysics    │    TG_StartPhysics     │     TG_EndPhysics  │                                                                                                   
    //     ├───────────────────┼─────────────────────┼────────────────────────┼────────────────────┤                                                                                                   
    //     │  TG_PrePhysics    │    Pre -> Pre       │    Pre -> Start        │    Pre->End        │                                                                                                   
    //     ├───────────────────┼─────────────────────┼────────────────────────┼────────────────────┤                                                                                                   
    //     │  TG_StartPhysics  │    Invalid          │    Start -> Start      │    Start->End      │                                                                                                   
    //     ├───────────────────┼─────────────────────┼────────────────────────┼────────────────────┤                                                                                                   
    //     │  TG_EndPhysics    │    Invalid          │    Invalid             │    End -> End      │                                                                                                   
    //     └───────────────────┴─────────────────────┴────────────────────────┴────────────────────┘    
    // - what is meaning for Pre -> End (two tick-groups)?
    //   - the tick function can be run in two tick groups (pre + start)                                                                                               
    TArrayWithThreadsafeAdd<TGraphTask<FTickFunctionTask>*> HiPriTickTasks[TG_MAX][TG_MAX];

    /** LowPri Held tasks for each tick group. */
    TArrayWithThreadsafeAdd<TGraphTask<FTickFunctionTask>*> TickTasks[TG_MAX][TG_MAX];

    /** these are waited for at the end of the frame; they are not on the critical path, but they have to be done before we leave the frame */
    // haker: releasing TGraphTask is time-consuming, to release them asynchronously, keep track them in CleanupTasks
    FGraphEventArray CleanupTasks;

    /** we keep track of the last TG we have blocked for so when we do block, we know which TG's to wait for */
    // while ticking in UWorld, keep track of tick-group we are currently waiting for
    ETickingGroup WaitForTickGroup;

    /** true during the tick phase, when true, tick function adds also go to the newly spawned list. **/
    // when we call StartFrame(), bTickNewlySpawned becomes 'true', and when EndFrame() is called, bTickNewlySpawned becomes 'false'
    bool bTickNewlySpawned;
};

/** class that aggregates the individual levels and deals with parallel tick setup */
// 012 - Foundation - TickTaskManager - FTickTaskManager
// haker: from the comment, we can see parallel tick setup is supported, but for us, we are focusing on single-threaded version
// - what is tick setup?
//   - the tick setup is to prepare tick functions in TickTaskLevels(ULevels in UWorld) to be ticked
// see FTickTaskManager's member variables (goto 013 : TickTaskManager)
class FTickTaskManager : public FTickTaskManagerInterface
{
public:
    /** singleton to retrieve the global tick task manager */
    static FTickTaskManager& Get()
    {
        // haker: remember how to define singleton pattern in Unreal Engine
        static FTickTaskManager SingletonInstance;
        return SingletonInstance;
    }

    /** allocate a new ticking structure for a ULevel */
    // 050 - Foundation - CreateWorld * - FTickTaskManager::AllocateTickTaskLevel()
    virtual FTickTaskLevel* AllocateTickTaskLevel() override
    {
        return new FTickTaskLevel;
        // search (goto 49)
    }

    /** find the tick level */
    FTickTaskLevel* TickTaskLevelForLevel(ULevel* Level)
    {
        return Level->TickTaskLevel;
    }

    /** return true if this tick function is in the primary list */
    bool HasTickFunction(ULevel* InLevel, FTickFunction* TickFunction)
    {
        FTickTaskLevel* Level = TickTaskLevelForLevel(InLevel);
        return Level->HasTickFunction(TickFunction);
    }

    /** add the tick function to the primary list */
    void AddTickFunction(ULevel* InLevel, FTickFunction* TickFunction)
    {
        FTickTaskLevel* Level = TickTaskLevelForLevel(InLevel);
        Level->AddTickFunction(TickFunction);
        TickFunction->InternelData->TickTaskLevel = Level;
    }

    /** remove the tick function from the primary list */
    void RemoveTickFunction(FTickFunction* TickFunction)
    {
        FTickTaskLevel* Level = TickFunction->InternalData->TickTaskLevel;
        Level->RemoveTickFunction(TickFunction);
    }

    /** fill the level list */
    // 019 - Foundation - TickTaskManager - FillLevelList
    // - ready for LevelList by Levels
    void FillLevelList(const TArray<ULevel*>& Levels)
    {
        // haker: only if current LC(level collection) is DynamicSourceLevels(==dynamic), add World's TickTaskLevel to the candidate list to tick
        if (Context.World->GetActiveLevelCollection()->GetType() == ELevelCollectionType::DynamicSourceLevels)
        {
            LevelList.Add(Context.World->TickTaskLevel);
        }

        // haker: only visible levels' TickTaskLevels are added
        for (int32 LevelIndex = 0; LevelIndex < Levels.Num(); LevelIndex++)
        {
            ULevel* Level = Levels[LevelIndex];
            if (Level && Level->bIsVisible)
            {
                LevelList.Add(Level->TickTaskLevel);
            }
        }
    }

    /** 
     * ticks the dynamic actors in the given levels based upon their tick group 
     * the function is called once for each ticking group
     */
    // 011 - Foundation - TickTaskManager - FTickTaskManager::StartFrame
    virtual void StartFrame(UWorld* InWorld, float InDeltaSeconds, ELevelTick InTickType, const TArray<ULevel*>& LevelsToTick) override
    {
        // see FTickTaskManager (goto 012 : TickTaskManager)

        // haker: cache the tick context from UWorld
        Context.TickGroup = ETickingGroup(0); // reset this to the start tick group
        Context.DeltaSeconds = InDeltaSeconds;
        Context.TickType = InTickType;
        // haker: note that it is natural to run StartFrame in game thread
        Context.Thread = ENamedThreads::GameThread;
        Context.World = InWorld;

        // see TTS::StartFrame (goto 017 : TickTaskManager)
        TickTaskSequencer.StartFrame();

        // see FTickTaskManager::FillLevelList (goto 019 : TickTaskManager)
        FillLevelList(LevelsToTick);

        // haker: LevelList is prepared from FillLevelList
        int32 TotalTickFunctions = 0;
        for (int32 LevelIndex = 0; LevelIndex < LevelList.Num(); LevelIndex++)
        {
            // see FTickTaskLevel::StartFrame (goto 020 : TickTaskManager)
            TotalTickFunctions += LevelList[LevelIndex]->StartFrame(Context);
        }

        for (int32 LevelIndex = 0; LevelIndex < LevelList.Num(); LevelIndex++)
        {
            // see FTickTaskLevel::QueueAllTicks (goto 022 : TickTaskManager)
            LevelList[LevelIndex]->QueueAllTicks();
        }
    }

    // 036 - Foundation - TickTaskManager - FTickTaskSequencer::DispatchTickGroup
    void DispatchTickGroup(ENamedThreads::Type CurrentThread, ETickingGroup WorldTickGroup)
    {
        // haker: (1) HiPriTickTasks
        // - first, see the diagram we covered before:
        //     ┌───────────────────┬─────────────────────┬────────────────────────┬────────────────────┐                                                                                                   
        //     │  Start\End        │    TG_PrePhysics    │    TG_StartPhysics     │     TG_EndPhysics  │                                                                                                   
        //     ├───────────────────┼─────────────────────┼────────────────────────┼────────────────────┤                                                                                                   
        //     │  TG_PrePhysics    │    Pre -> Pre       │    Pre -> Start        │    Pre->End        │                                                                                                   
        //     ├───────────────────┼─────────────────────┼────────────────────────┼────────────────────┤                                                                                                   
        //     │  TG_StartPhysics  │    Invalid          │    Start -> Start      │    Start->End      │                                                                                                   
        //     ├───────────────────┼─────────────────────┼────────────────────────┼────────────────────┤                                                                                                   
        //     │  TG_EndPhysics    │    Invalid          │    Invalid             │    End -> End      │                                                                                                   
        //     └───────────────────┴─────────────────────┴────────────────────────┴────────────────────┘
        
        // haker: we iterate EndTickGroup from 0(TG_PrePhysics) to TG_MAX
        for (int32 IndexInner = 0; IndexInner < TG_MAX; IndexInner++)
        {
            // haker: let's suppose WorldTickGroup == TG_PrePhysics
            // - from above diagram example, what scopes are executed?
            //   1. TG_PrePhysics -> TG_PrePhysics
            //   2. TG_PrePhysics -> TG_StartPhysics
            //   3. TG_PrePhysics -> TG_EndPhysics
            TArray<TGraphTask<FTickFunctionTask>*>& TickArray = HiPriTickTasks[WorldTickGroup][IndexInner];
            if (IndexInner < WorldTickGroup)
            {
                // haker: e.g. TG_EndPhysics -> TG_PrePhysics && TickArray.Num() != 0 -> ERROR!!!
                check(TickArray.Num() == 0); // make no sense to have and end TG before the start TG
            }
            else
            {
                for (int32 Index = 0; Index < TickArray.Num(); ++Index)
                {
                    // haker: try to run graph-task for tick function
                    // - as I explained, we add TGraphTask as ConstructAndHold***
                    // - to kick off tick-function we need to call Unlock in TGraphTask<>
                    TickArray[Index]->Unlock(CurrentThread);
                }
            }
            TickArray.Reset();
        }

        // haker: (2) TickTasks (normal)
        for (int32 IndexInner = 0; IndexInner < TG_MAX; IndexInner++)
        {
            TArray<TGraphTask<FTickFunctionTask>*>& TickArray = TickTasks[WorldTickGroup][IndexInner]; //-V781
            if (IndexInner < WorldTickGroup)
            {
                check(TickArray.Num() == 0);
            }
            else
            {
                for (int32 Index = 0; Index < TickArray.Num(); Index++)
                {
                    TickArray[Index]->Unlock(CurrentThread);
                }
            }
            TickArray.Reset();
        }
    }

    /** run a tick group, ticking all actors and components */
    // 034 - Foundation - TickTaskManager - FTickTaskManager::RunTickGroup
    virtual void RunTickGroup(ETickingGroup Group, bool bBlockTillComplete) override
    {
        // see FTickTaskSequencer::ReleaseTickGroup (goto 035)
        TickTaskSequencer.ReleaseTickGroup(Group, bBlockTillComplete);

        // new actors go into the next tick group because this one is already gone
        Context.TickGroup = ETickingGroup(Context.TickGroup + 1);

        // haker: skip special handling for TG_NewlySpawned
        // - I'll leave this part for you~ :)
    }

    /** finish a frame of ticks */
    void EndFrame() override
    {
        TickTaskSequencer.EndFrame();
        bTickNewlySpawned = false;
        for( int32 LevelIndex = 0; LevelIndex < LevelList.Num(); LevelIndex++ )
        {
            LevelList[LevelIndex]->EndFrame();
        }
        Context.World = nullptr;
        LevelList.Reset();
    }

    // 013 - Foundation - TickTaskManager - FTickTaskManager's member variables

    /** global sequencer */
    // see FTickTaskSequencer (goto 014 : TickTaskManager)
    // haker: what is TickTaskSequencer?
    // - Can think of it as ***tick function scheduler***
    FTickTaskSequencer& TickTaskSequencer;

    /** list of current levels */
    // see FTickTaskLevel's member variables (goto 015: TickTaskManager)
    TArray<FTickTaskLevel*> LevelList;

    /** tick context */
    FTickContext Context;

    /** true during the tick phase, when true, tick function adds also go to the newly spawned list */
    bool bTickNewlySpawned;
};