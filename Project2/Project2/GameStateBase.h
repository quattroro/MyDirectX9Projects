
/**
 * GameStateBase is a class that manages the game's global state, and is spawned by GameModeBase
 * It exists on both the client and the server and is fully replicated 
 */
class AGameStateBase : public AInfo
{
    /** called by game mode to set the started play */
    // 065 - Foundation - CreateInnerProcessPIEGameInstance - AGameStateBase::HandleBeginPlay
    virtual void HandleBeginPlay()
    {
        // haker: ah?! now AWorldSetting?
        // - see AWorldSetting::NotifyBeginPlay (goto 066: CreateInnerProcessPIEGameInstance)
        GetWorldSettings()->NotifyBeginPlay();
    }
};