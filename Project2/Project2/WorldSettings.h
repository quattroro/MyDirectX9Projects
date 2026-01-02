
/** Actor containing all script accessible world properties */
class AWorldSettings : public AInfo
{
    // 066 - Foundation - CreateInnerProcessPIEGameInstance - AWorldSettings::NotifyBeginPlay
    void NotifyBeginPlay()
    {
        // haker: now finally, we can see the code which iterates all actors in world and call each actor's DispatchBeginPlay()
        UWorld* World = GetWorld();
        if (!World->bBegunWorld)
        {
            for (FActorIterator It(World); It; ++It)
            {
                const bool bFromLevelLoad = true;
                It->DispatchBeginPlay(bFromLevelLoad);
            }
            World->bBegunWorld = true;
        }

        // haker: go back to 064: CreateInnerProcessPIEGameInstance!
    }
};