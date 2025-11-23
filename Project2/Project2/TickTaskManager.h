#include "EngineBaseTypes.h"

// 47 - Foundation - CreateWorld - FTickTaskLevel
// see FTickTaskLevel's member variables (goto 48)
class FTickTaskLevel
{
    /** return true if this tick function is in primary list */
    // 58 - Foundation - CreateWorld - FTickTaskLevel::HasTickFunction
    bool HasTickFunction(FTickFunction* TickFunction)
    {
        return AllEnabledTickFunctions.Contains(TickFunction)
            || AllDisabledTickFunctions.Contains(TickFunction) 
            // haker: look at FCoolingDownTickFunctionList::Contains()
            // see FCoolingDownTickFunctionList::Contains (goto 59)
            || AllCoolingDownTickFunctions.Contains(TickFunction);
	}

    /** add the tick function to the primary list */
    // 57 - Foundation - CreateWorld - FTickTaskLevel::AddTickFunction
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
    // 56 - Foundation - CreateWorld - FTickTaskLevel::RemoveTickFunction
    void RemoveTickFunction(FTickFunction* TickFunction)
    {
        // haker: AllEnabledTickFunctions, AllDisabledTickFunctions, TickFunctionsToReschedule and AllCoolingDownTickFunctions(linked-list) are need to be synchronized
        switch(TickFunction->TickState)
        {
        case FTickFunction::ETickState::Enabled:
            if (TickFunction->InternalData->bWasInternal)
            {
                // an enabled function with a tick interval could be in either the enabled or cooling down list
                // haker: 
                // - what is the meaning of AllEnabledTickFunctions.Remove() equals 0?
                //   - it is in cooling-down list or in reschedule list
                if (AllEnabledTickFunctions.Remove(TickFunction) == 0)
                {
                    auto FindTickFunctionInRescheduleList = [TickFunction](const FTickScheduleDetails& TSD)
                    {
                        return (TSD.TickFunction == TickFunction);
                    };
                    // haker: note that TickFunctionsToReschedule is the array (liner-search)
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
                            bFound = true;
                            if (PrevComparisonFunction)
                            {
                                PrevComparisonFunction->InternalData->Next = TickFunction->InternalData->Next;
                            }
                            else
                            {
                                // haker: it starts from AllCoolingDownTickFunctions.Head, so matches the below condition
                                check(TickFunction == AllCoolingDownTickFunctions.Head);
                                AllCoolingDownTickFunctions.Head = TickFunction->InternalData->Next;
                            }
                            TickFunction->InternalData->Next = nullptr;
                        }
                        else
                        {
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
                // it it is not internal, it already ticking so we can remove AllEnabledTickFunctions
                verify(AllEnabledTickFunctions.Remove(TickFunction) == 1);
            }
            break;

        case FTickFunction::ETickState::Disabled:
            // haker: when TickState is Disabled, it should be in AllDisabledTickFunctions
            verify(AllDisabledTickFunctions.Remove(TickFunction) == 1);
            break;

        case FTickFunction::ETickState::CoolingDown:
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

    struct FTickScheduleDetails
    {
        FTickFunction* TickFunction;
        float Cooldown;
        bool bDeferredRemove;
    };

    struct FCoolingDownTickFunctionList
    {
        // 59 - Foundation - CreateWorld - FCoolingDownTickFunctionList::Contains
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

    // 48 - Foundation - CreateWorld - FTickTaskLevel's member variables

    /** primary list of enabled tick functions */
    TSet<FTickFunction*> AllEnabledTickFunctions;

    /** primary list of enabled tick functions */
    // see FCoolingDownTickFunctionList
    FCoolingDownTickFunctionList AllCoolingDownTickFunctions;

    /** primary list of disabled tick functions */
    TSet<FTickFunction*> AllDisabledTickFunctions;

    /** utility array to avoid memory reallocation when collecting functions to reschedule */
    // see FTickScheduleDetails
    // haker: FTickScheduleDetails is separate data for each tick-function to describe how it will be scheduled
    TArrayWithThreadsafeAdd<FTickScheduleDetails> TickFunctionsToReschedule;

    /** list of tick functions added during a tick phase; these items are also duplicated in AllLiveTickFunctions for future frames */
    TSet<FTickFunction*> NewlySpawnedTickFunctions;
    
    /** true during the tick phase, when true, tick function adds also go to the newly spawned list */
    bool bTickNewlySpawned;

    // haker: who is the owner of FTickTaskLevel to create/destroy?
    // see ULevel::ULevel() constructor (goto 49)

    // haker: from here we can know two things:
    // 1. FTickFunction -> FTickTaskLevel:
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
};

/** class that aggregates the individual levels and deals with parallel tick setup */
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
    // 50 - Foundation - CreateWorld - FTickTaskManager::AllocateTickTaskLevel()
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
};