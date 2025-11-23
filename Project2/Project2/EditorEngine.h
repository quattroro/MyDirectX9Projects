#include "World.h"

/** specifies the goal/source of a UWorld object */
// 14 - Foundation - Entry - EWorldType
// haker: note that it is useful pattern define enum in UE5
// - if you wrap it up with namespace, you can avoid enum name conflicts with other global enum
namespace EWorldType
{
    enum Type
    {
        /** an untyped world */
        None,

        /** the game world */
        Game,

        /** a world being edited in the editor */
        Editor,

        /** a play in editor world */
        PIE,
    };
}

/**
 * FWorldContext
 * a context for dealing with UWorlds at the engine level. as the engine brings up and destroys world, we need a way to keep straight
 * what world belongs to what
 * 
 * WorldContexts can be throught of as a track. by default, we have 1 track that we load and unload levels on. adding a second context
 * is adding a second track; another track of progression for worlds to live on.
 * 
 * for the GameEngine, there will be one WorldContext until we decide to support multiple simultaneous worlds.
 * for the EditorEngine, there may be one WorldContext for EditorWorld and one for the PIE World.
 * 
 * FWorldContext provides both a way to manage 'the current PIE UWorld*' as well as state that goes along with connecting/travelling to
 * new worlds
 * 
 * FWorldContext should remain internal to the UEngine classes. outside code should not keep pointers or try to manage FWorldContexts directly.
 * outside code can still deal with UWorld*, and pass UWorld*s into Engine level functions. the Engine code can look up the relevant context
 * for a given UWorld*
 * 
 * for convenience, FWorldContext can maintain outside pointers to UWorld*s. for example, PIE can tie UWorld* UEditorEngine::PlayWorld to the PIE
 * world context. if the PIE UWorld changes, the UEditorEngine::PlayWorld pointer will be automatically updated. this is done with AddRef() and 
 * SetCurrentWorld()
 */
// 13 - Foundation - Entry - FWorldContext
// haker: 
// - the context for world used in engine level (UEngine, UEditorEngine, etc)
//   - you can think of FWorldContext as descriptor for loosing dependency between engine and world
// - for GameEngine:
//   - usually one WorldContext, when switching worlds like lobby to in-game world, there is the moment when two world contexts exist
// - for EditorEngine:
//   - when we editing the world with level viewport, we normally face one world context
//   - when we execute the PIE, the new world context is generated and simultaneously, two world contexts exists
//   - when you try to run multiplay game in PIE, multiple world contexts can exist
// - as I said, the world context is for engine, so do NOT maintain FWorldContext elsewhere
struct FWorldContext
{
    void SetCurrentWorld(UWorld* World)
    {
        UWorld* OldWorld = ThisCurrentWorld;
        ThisCurrentWorld = World;

        if (OwningGameInstance)
        {
            OwningGameInstance->OnWorldChanged(OldWorld, ThisCurrentWorld);
        }
    }

    // see EWorldType
    TEnumAsByte<EWorldType::Type> WorldType;
    
    // haker: assign separate name to context different from UWorld's name
    FName ContextHandle;

    // haker: GameInstance owns ONE world context
    TObjectPtr<class UGameInstance> OwningGameInstance;

    // haker: the world which the context is referencing
    TObjectPtr<UWorld> ThisCurrentWorld;
};

// 12 - Foundation - Entry - UEngine
class UEngine : public UObject, public FExec
{
    FWorldContext& CreateNewWorldContext(EWorldType::Type WorldType)
    {
        FWorldContext* NewWorldContext = new FWorldContext;
        WorldList.Add(NewWorldContext);
        NewWorldContext->WorldType = WorldType;
        NewWorldContext->ContextHandle = FName(*FString::Printf(TEXT("Context_%d"), NextWorldContextHandle++));
        return *NewWorldContext;
    }

    /** initialize the game engine */
    // 17 - Foundation - Entry - UEngine::Init
    virtual void Init(IEngineLoop* InEngineLoop)
    {
        if (GIsEditor)
        {
            // create a WorldContext for the editor to use and create an initially empty world
            // haker: here, we make sure that at least, one editor world exists
            FWorldConext& InitialWorldContext = CreateNewWorldContext(EWorldType::Editor);

            // haker: 
            // see UWorld::CreateWorld
            // 0 - Foundation - CreateWorld - BEGIN
            InitialWorldContext.SetCurrentWorld(UWorld::CreateWorld(EWorldType::Editor, true));
            GWorld = InitialWorldContext.World();
        }
    }

    // haker: recommend to look into TIndirectArray, focusing on differences from TArray
    // see FWorldContext
    TIndirectArray<FWorldContext> WorldList;
    int32 NextWorldContextHandle;
};

// 11 - Foundation - Entry - UEditorEngine
// see UEngine
class UEditorEngine : public UEngine
{
public:
    /** returns the WorldContext for the editor world. for now, there will always be exactly 1 of these in the editor */
    // 16 - Foundation - Entry - GetEditorWorldContext
    FWorldContext& GetEditorWorldContext(bool bEnsureIsGWorld = false)
    {
        for (int32 i = 0; i < WorldList.Num(); ++i)
        {
            if (WorldList[i].WorldType == EWorldType::Editor)
            {
                return WorldList[i];
            }
        }

        // haker: if we get into this line of code, it will crashed:
        // - where is initial editor world created?
        // - the hint is provided by the below comment
        check(false); // there should have already been one created in ***UEngine::Init***
        // see UEngine::Init()

        return CreateNewWorldContext(EWorldType::Editor);
    }

    // 10 - Foundation - Entry - UEditorEngine::Tick
    // see UEditorEngine
    virtual void Tick(float DeltaSeconds, bool bIdleMode) override
    {
        // 15 - Foundation - Entry
        // haker: where does GWorld is updated initially?
        UWorld* CurrentGWorld = GWorld;

        // see GetEditorWorldContext()
        FWorldContext& EditorContext = GetEditorWorldContext();
    }
};