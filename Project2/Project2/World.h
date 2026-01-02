/** struct containing a collection of optional parameters for initialization of a world */
// 003 - Foundation - CreateWorld ** - FWorldInitializationValues
// haker: think of this pattern as one struct encapsulating multiple parameters for the code readability
// - this struct contains all necessary options to create world
struct FWorldInitializationValues
{
    /** should the scenes (physics, rendering) be created */
    // haker: whether we create worlds (render world, physics world, ...)
    uint32 bInitializeScenes:1;

    /** Should the physics scene be created. bInitializeScenes must be true for this to be considered. */
    uint32 bCreatePhysicsScene:1;

    /** Are collision trace calls valid within this world. */
    uint32 bEnableTraceCollision:1;

    //...
};

/** defines available strategies for handling the case where an actor is spawned in such a way that it penetrates blocking collisions */
// 027 - Foundation - CreateWorld ** - ESpawnActorCollisionHandlingMethod:
// haker: spawning policy depending on collision between object and obstacles
enum class ESpawnActorCollisionHandlingMethod : uint8
{
    /** fall back to default settings */
    Undefined,
    /** actor will spawn in desired location, regardless of collisions */
    AlawysSpawn,
    /** actor will try to find a nearby non-colliding location (based on shape components) but will always spawn even if one cannot be found */
    // haker: spawn but adjust a little bit and spawn non-collising location
    AdjustIfPossibleButAlwaysSpawn,
    /** Actor will try to find a nearby non-colliding location (based on shape components), but will NOT spawn unless one is found. */
    AdjustIfPossibleButDontSpawnIfColliding,
    /** Actor will fail to spawn. */
    DontSpawnIfColliding,
};

/** determines how the transform being passed into actor spawning methods interact with the actor's default root component */
enum class ESpawnActorScaleMethod : uint8
{
    /** ignore the default scale in the actor's root component and hard-set it to the value of SpawnTransform Parameter */
    OverrideRootScale,
    /** multiply value of the SpawnTransform Parameter with the default scale in the actor's root component */
    MultiplyWithRoot,
    SelectDefaultAtRuntime,
};

/** struct of optional parameters passed to SpawnActor function(s) */
// 026 - Foundation - CreateWorld ** - FActorSpawnParameters
// haker: I reduce its member variables drastically
struct FActorSpawnParameters
{
    /** a name to assign as the Name of the Actor being spawned; if no value is specified, the name of the spawned Actor will be automatically generated using the form [Class]_[Number] */
    // haker: default format of FName is [ClassName]_[Number]:
    // - e.g. WorldSettings_0
    FName Name;

    /** 
     * an actor to use as a template when spawning the new Actor: the spawned Actor will be initialized using the property values of the template Actor
     * if left NULL, the class default object (CDO) will be used to initialize the spawned Actor
     */
    AActor* Template;

    /** the actor that spawned this Actor (can be left as NULL) */
    AActor* Owner;

    /** the APawn that is responsible for damage done by the spawned Actor (can be left as NULL) */
    APawn* Instigator;

    /** method for resolving collisions at the spawn point; undefined means no override, use the actor's setting */
    // see ESpawnActorCollisionHandlingMethod (goto 27)
    ESpawnActorCollisionHandlingMethod SpawnCollisionHandlingOverride;

    /** determines whether to multiply or override root component with provided spawn transform */
    ESpawnActorScaleMethod TransformScaleMethod = ESpawnActorScaleMethod::MultiplyWithRoot;

    /** 
     * The ULevel to spawn the Actor in, i.e. the Outer of the Actor
     * if left as NULL the outer of the owner is used
     * if the owner is NULL, the persistent level is used
     */
    ULevel* OverrideLevel;

    /** determines whether or not the actor may be spawned when running a construction script; if true spawning will fail if a construction script is being run */
    uint8 bAllowDuringConstructionScript : 1;

    /** determine whether the construction script will be run; if true, the construction script will not be run on the spawned Actor; only applicable if the Actor is being spawned from a BP */
    uint8 bDeferConstruction : 1;
};

/** globals without thread protection must be accessed only from GameThread */
static TArray<FSubsystemCollectionBase*> GlobalSubsystemCollections;

// 023 - Foundation - CreateWorld ** - FSubsystemCollectionBase
// see FSubsystemCollectionBase's member variables (goto 24)
class FSubsystemCollectionBase
{
    FSubsystemCollectionBase(UClass* InBaseType)
        : Outer(nullptr)
        , BaseType(InBaseType)
    {}

    // 031 - Foundation - CreateWorld ** - FSubsystemCollectionBase::AddAndInitializeSubsystem
    USubsystem* AddAndInitializeSubsystem(UClass* SubsystemClass)
    {
        // haker: note that it only allow ONE instance per class type
        if (!SubsystemMap.Contains(SubsystemClass))
        {
            // only add instances for non abstract Subsystems
            // haker:
            // - UClass::ClassFlags has class information
            if (SubsystemClass && !SubsystemClass->HasAllClassFlags(CLASS_Abstract))
            {
                // subsystem class should be derived from base type
                check(SubsystemClass->IsChildOf(BaseType));

                // haker:
                // - ShouldCreateSubsystem() determines whether we create subsystem or not
                //    - override this method to control the flow of subsystem creation
                // - CDO is sufficient to call ShouldCreateSubsystem()
                const USubsystem* CDO = SubsystemClass->GetDefaultObject<USubsystem>();
                // see USubsystem::ShouldCreateSubsystem (goto 32)
                if (CDO->ShouldCreateSubsystem(Outer))
                {
                    // haker: create new USubsystem by SubsystemClass (in our case, the class is UWorldSubsystem)
                    USubsystem* Subsystem = NewObject<USubsystem>(Outer, SubsystemClass);
                    SubsystemMap.Add(SubsystemClass, Subsystem);
                    Subsystem->InternalOwningSubsystem = this;
                    Subsystem->Initialize(*this);
                    return Subsystem;
                }
            }
            return nullptr;
        }
        // haker: if we already have SubsystemClass, find an existing instance from SubsystemMap
        return SubsystemMap.FindRef(SubsystemClass);
    }

    /** initialize the collection of systems; systems will be created and initialized */
    // 030 - Foundation - CreateWorld ** - FSubsystemCollectionBase::Initialize
    void Initialize(UObject* NewOuter)
    {
        // already initialized
        if (Outer)
        {
            return;
        }

        // haker: we set NewOuter as UWorld
        Outer = NewOuter;
        
        if (ensure(BaseType) && ensure(SubSystemMap.Num() == 0))
        {
            check(IsInGameThread());

            // haker: I eliminate UDynamicsSubsystem handling codes for simplicity
            // - the below code is about non-UDynamicSubsystem e.g. UWorldSubsystem
            {
                // haker: BaseType is UWorldSubsystem for UWorld::SubsystemCollection
                // - calling GetDerivedClasses() collects all classes derived from UWorldSubsystem
                TArray<UClass*> SubsystemClasses;
                GetDerivedClasses(BaseType, SubsystemClasses, true);

                // haker: looping subsystem's classes derived from UWorldSubsystem
                for (UClass* SubsystemClass : SubsystemClasses)
                {
                    // see FSubsystemCollectionBase::AddAndInitializeSubsystem (goto 31)
                    AddAndInitializeSubsystem(SubsystemClass);
                }
            }

            // update internal arrays without emptying it so that existing refs remain valid
            for (auto& Pair : SubsystemArrayMap)
            {
                // haker: note that we clear the array!
                Pair.Value.Empty();

                // haker: SubsystemMapArray persist 1:1 mapping on Subsystem, but UDynamicSubsystem could allow multiple instance
                // - iterating SubSystemMap, refill class-[multiple instances]
                // - SubsystemArrayMap contains mappings from base class to derived classes
                UpdateSubsystemArrayInternal(Pair.Key, Pair.Value);
            }

            // haker: see the definition of GlobalSubsystemCollections
            GlobalSubsystemCollections.Add(this);
        }
    }

    void FSubsystemCollectionBase::UpdateSubsystemArrayInternal(UClass* SubsystemClass, TArray<USubsystem*>& SubsystemArray) const
    {
        for (auto Iter = SubsystemMap.CreateConstIterator(); Iter; ++Iter)
        {
            UClass* KeyClass = Iter.Key();
            if (KeyClass->IsChildOf(SubsystemClass))
            {
                SubsystemArray.Add(Iter.Value());
            }
        }
    }

    const TArray<USubsystem*>& GetSubsystemArrayInternal(UClass* SubsystemClass) const
    {
        if (!SubsystemArrayMap.Contains(SubsystemClass))
        {
            TArray<USubsystem*>& NewList = SubsystemArrayMap.Add(SubsystemClass);
            UpdateSubsystemArrayInternal(SubsystemClass, NewList);
            return NewList;
        }

        const TArray<USubsystem*>& List = SubsystemArrayMap.FindChecked(SubsystemClass);
        return List;
    }

    // 024 - Foundation - CreateWorld ** - FSubsystemCollectionBase
    // haker:
    // if we are dealing with UWorldSusystem:
    // - Outer is UWorld -> will be set on Initialize() call
    // - BaseType is UWorldSubsystem
    UObject* Outer;
    UClass* BaseType;

    // haker: mapper from UClass(Subsystem's UClass) to UObject(Subsystem's instance)
    // - at initialization time of FObjectSubsystemCollection<SubsystemType>, one-to-one mappings are supported 
    TMap<TObjectPtr<UClass>, TObjectPtr<USubsystem>> SubsystemMap;

    // haker: from my guess, to support UDynamicSubsystem, it persist multiple instances for each USubsystem's class type
    // - this map is need to collect all classes derived from base class:
    //   - e.g. UWorldSubsystem -> [UWorldSubsystemA, UWorldSubsystemB, UWorldSubsystemC, ...]
    mutable TMap<UClass*, TArray<USubsystem*>> SubsystemArrayMap;
};

/** subsystem collection which delegates UObject references to its owning UObject (object needs to implement AddReferencedObjects and forward call to Collection) */
// 022 - Foundation - CreateWorld ** - FObjectSubsystemCollection
// see FSubsystemCollectionBase (goto 23)
template <typename TBaseType>
class FObjectSubsystemCollection : public FSubsystemCollectionBase
{
    /** construct a FSubsystemCollection, pass in the owning object almost certainly (this) */
    FObjectSubsystemCollection()
        : FSubsystemCollectionBase(TBaseType::StaticClass())
    {}

    template <typename TSubsystemClass>
    const TArray<TSubsystemClass*>& GetSubsystemArray(const TSubclassOf<TSubsystemClass>& SubsystemClass) const
    {
        TSubclassOf<TBaseType> SubsystemBaseClass = SubsystemClass;
        const TArray<USubsystem*>& Array = GetSubsystemArrayInternal(SubsystemBaseClass);
        const TArray<TSubsystemClass*>* SpecificArray = reinterpret_cast<const TArray<TSubsystemClass*>*>(&Array);
        return *SpecificArray;
    }
};

/**
 * subsystems are auto instanced classes that share the lifetime of certain engine constructs
 * 
 * currently supported subsystem lifetimes are:
 *  Engine		 -> inherit UEngineSubsystem
 *	Editor		 -> inherit UEditorSubsystem
 *	GameInstance -> inherit UGameInstanceSubsystem
 *	World		 -> inherit UWorldSubsystem
 *	LocalPlayer	 -> inherit ULocalPlayerSubsystem
 * 
 * normal example:
 *  class UMySystem : public UGameInstanceSubsystem
 * which can be accessed by:
 *  UGameInstance* GameInstance = ...;
 *  UMySystem* MySystem = GameInstance->GetSubsystem<UMySystem>();
 * 
 * or the following if you need protection from a null GameInstance
 *  UGameInstance* GameInstance = ...;
 *  UMyGameSubsystem* MySubsystem = UGameInstance::GetSubsystem<MyGameSubsystem>(GameInstance);
 * 
 *	You can get also define interfaces that can have multiple implementations.
 *	Interface Example :
 *      MySystemInterface
 *    With 2 concrete derivative classes:
 *      MyA : public MySystemInterface
 *      MyB : public MySystemInterface
 *
 *	Which can be accessed by:
 *		UGameInstance* GameInstance = ...;
 *		const TArray<UMyGameSubsystem*>& MySubsystems = GameInstance->GetSubsystemArray<MyGameSubsystem>();
 */
// haker:
// - USubsystem is the system follows life-time (create/destroy) of a component of the unreal engine:
// - types of subsystems:
//   1. Engine        -> inherit UEngineSubsystem
//   2. Editor        -> inherit UEditorSubsystem
//   3. GameInstance  -> inherit UGameInstanceSubsystem
//   4. World         -> inherit UWorldSubsystem
//   5. LocalPlayer   -> inherit ULocalPlayerSubsystem
// - **the care of lifetime with engine's component like UWorld** is cumbersome:
//   - if you try to use Subsytem, it will be very easy and handy!
// - read examples of how to access subsystems
class USubsystem : public UObject
{
    /**
     * override to control if the Subsystem should be created
     * for example you could only have your system created on servers
     * it is important to note that if using this is becomes very important to null check whenever getting the Subsystem
     * 
     * NOTE: this function is called on the CDO prior to instances being created!!! 
     */
    // 032 - Foundation - CreateWorld ** - ShouldCreateSubsystem
    // haker: as the comment describes, this member function is called via CDO(ClassDefaultObject)
    virtual bool ShouldCreateSubsystem(UObject* Outer) const { return true; }

    /** implement this for init/deinit of instances of the system */
    virtual void Initialize(FSubsystemCollectionBase& Collection) {}
    virtual void Deinitialize() {}

    // haker: we are interested UWorld's [FObjectSubsystemCollection<UWorldSubsystem>]
    // - each subsystem has its owner like this
    // see FObjectSubsystemCollection (goto 22)
    FSubsystemCollectionBase* InternalOwningSubsystem;
};

/** base class for auto instanced and initialized systems that share the lifetime of a UWorld */
// 021 - Foundation - CreateWorld ** - UWorldSubsystem
// see USubsystem
class UWorldSubsystem : public USubsystem
{
    /** called after world components (e.g. line batcher and all level components) have been updated */
    virtual void OnWorldComponentsUpdated(UWorld& World) {}
};

/** indicates the type of a level collection, used in FLevelCollection */
// 020 - Foundation - CreateWorld ** - ELevelCollectionType
// haker: don't sticking to its types, we are going to understand it simply like dynamic level vs. static level
enum class ELevelCollectionType : uint8
{
    /**
     * the dynamic levels that are used for normal gameplay and the source for any duplicated collections
     * will contain a world's persistent level and any streaming levels that contain dynamic or replicated gameplay actors
     */
    DynamicSourceLevels,

    /** gameplay relevant levels that have been duplicated from DynamicSourceLevels if requested by the game */
    DynamicDuplicatedLevels,

    /**
     * these levels are shared between the source levels and the duplicated levels, and should contain
     * only static geometry and other visuals that are not replicated or affected by gameplay
     * thsese will not be duplicated in order to save memory 
     */
    StaticLevels,

    MAX
};

/**
 * contains a group of levels of a particular ELevelCollectionType within a UWorld
 * and the context required to properly tick/update those levels; this object is move-only
 */
// 019 - Foundation - CreateWorld ** - FLevelCollection
// haker: FLevelCollection is collection based on ELevelCollectionType
// - when running TickFunction, it is very efficient to classify which ULevel has dynamic objects
struct FLevelCollection
{
    /** returns the set of levels in this collection */
    const TSet<TObjectPtr<ULevel>& GetLevels() const { return Levels; }

    /** gets the type of this collection */
    ELevelCollectionType GetType() const { return CollectionType; }

    /** returns this collection's PersistentLevel */
    ULevel* GetPersistentLevel() const { return PersistentLevel; }

    /** the type of this collection */
    // see ELevelCollectionType (goto 20)
    ELevelCollectionType CollectionType;

    /**
     * the persistent level associated with this collection
     * the source collection and the duplicated collection will have their own instances 
     */
    // haker: usually OwnerWorld's PersistentLevel
    TObjectPtr<class ULevel> PersistentLevel;

    /** all the levels in this collection */
    TSet<TObjectPtr<ULevel>> Levels;
};

struct FActorInitializedParams
{
    UWorld* World;
    bool bResetTime;
};

/** maximum size of name, including the null terminator */
enum {NAME_SIZE = 1024};

/**
 * generates a 102-bits actor GUID
 * - Bits 101-54 hold the user unique id
 * - Bits 53-0 hold a microseconds timestamp
 * 
 * NOTE:
 * - the timestamp is stored in microseconds for a total of 54 bits, enough to cover the next 570 years
 * - the highest 72 bits are appended to the name in hexadecimal
 * - the lowest 30 bits of the timestamp are stored in the name number:
 *   - this is to minimize the total names generated for globally unique names (string part will change every ~17 minutes for a specific actor class)
 * - the name number bit 30 is reserved to know if this is a globally unique name:
 *   - this is not 100% safe, but should cover most cases
 * - the name number bit 31 is reserved to preserve the fast path name generation (see GFastPathUniqueNameGeneration)
 * - on some environments, cooking happens without network, so we can't retrieve the MAC address for spawned actors, assign a default one in this case
 */
class FActorGUIDGenerator
{
    FActorGUIDGenerator()
        : Origin(FDataTime(2020, 1, 1))
        , Counter(0)
    {
        TArray<uint8> MACAddress = FPlatformMisc::GetMacAddress();
        FMemory::Memcpy(UniqueID, MACAddress.GetData(), 6);
    }

    FName NewActorGUID(FName BaseName)
    {
        uint8 HighPart[9];

        const FDataTime Now = FDataTime::Now();
        check(Now > Origin);

        const FTimespan Elapsed = FDataTime::Now() - Origin;
        const uint64 ElapsedUs = (uint64)Elapsed.GetTotalMilliseconds() * 1000 + (Counter++ % 1000);

        // copy 48-bits unique id
        FMemory::Memcpy(HighPart, UniqueID, 6);

        // append the high part of timestamp (will change every ~17 minutes)
        const uint64 ElapsedUsHighPart = ElapsedUs >> 30;
        FMemory::Memcpy(HighPart + 6, &ElapsedUsHighPart, 3);

        // make final name
        TStringBuilderWithBuffer<TCHAR, NAME_SIZE> StringBuilder;
        StringBuilder += BaseName.ToString();
        StringBuilder += TEXT("_UAID_");

        for (uint32 i=0; i<9; i++)
        {
            StringBuilder += NibbleToTChar(HighPart[i] >> 4);
            StringBuilder += NibbleToTChar(HighPart[i] & 15);
        }

        return FName(*StringBuilder, (ElapsedUs & 0x3fffffff) | (1 << 30));
    }

    FDataTime Origin;
    uint8 UniqueID[6];
    uint32 Counter;
};

/** checks if the TChar is a valid hex character */
inline const bool CheckTCharIsHex( const TCHAR Char )
{
	return ( Char >= TEXT('0') && Char <= TEXT('9') ) || ( Char >= TEXT('A') && Char <= TEXT('F') ) || ( Char >= TEXT('a') && Char <= TEXT('f') );
}

/** World actors spawning helper functions */
struct FActorSpawnUtils
{
    /**
     * function to generate a locally or globally unique actor name:
     * - to generate a globally unique name, we store an epoch number in the name number (while maintaining compatibility with fast path name generation)
     *   and also append an unique user id to the name 
     */
    // 002 - Foundation - SpawnActor - FActorSpawnUtils::MakeUniqueActorName
    static FName MakeUniqueActorName(ULevel* Level, const UClass* Class, FName BaseName, bool bGloballyUnique)
    {
        FName NewActorName;

        // haker: in SpawnActor, we use bGloballyUnique as false:
        // - you can understand what bGloballyUnique is for:
        //   - inspecting FActorGUIDGenerator
        // - briefly explained like this:
        //   - the UE5 supports networked actor which can be modified by multiple users (developers)
        //   - to identify this actor uniquely, it uses MACAddress for their FName's unique number
        //     - MACAddress is the phyiscal address assigned by NIC(Network Interface Card)
        // - for now, you're focus on the case (bGloballyUnqiue == 0)
        //   - the familiar code which we had seen before
        if (bGloballyUnique)
        {
            static FActorGUIDGenerator ActorGUIDGenerator;
            do
            {
                NewActorName = ActorGUIDGenerator.NewActorGUID(BaseName);
            } while (StaticFindObjectFast(nullptr, Level, NewActorName)); 
        }
        else
        {
            // haker: the detail code will be skipped
            NewActorName = MakeUniqueObjectName(Level, Class, BaseName);
        }
        return NewActorName;
    }

    /** determine if an actor name is globally unique or not */
    static bool IsGloballyUniqueName(FName Name)
    {
        if (Name.GetNumber() & (1 << 30))
        {
            const FString PlainName = Name.GetPlainString();
            const int32 PlainNameLen = PlainName.Len();

            // parse a name like this: StaticMeshActor_UAID_001122334455667788
            if (PlainNameLen >= 24)
            {
                if (!FCString::Strnicmp(*PlainName + PlainNameLen - 24, TEXT("_UAID"), 6))
                {
                    for (uint32 i = 0; i < 18; ++i)
                    {
                        if (!CheckTCharIsHex(PlainName[PlainNameLen - i - 1]))
                        {
                            return false;
                        }
                    }
                    return true;
                }
            }
        }
        return false;
    }

    /** return the base name (without any number of globally unique identifier) */
    static FName GetBasename(FName Name)
    {
        if (IsGloballyUniqueName(Name))
        {
            // chop a name like this: StaticMeshActor_UAID_001122334455667788
            return *Name.GetPlainNameString().LeftChop(24);
        }
        return *Name.GetPlainNameString();
    }
};

namespace EComponentMarkedForEndOfFrameUpdateState
{
	enum Type
	{
		Unmarked,
		Marked,
		MarkedForGameThread,
	};
}

/** utility struct to allow world direct access to UActorComponent::MarkedForEndOfFrameUpdateState without friending all of UActorComponent */
struct FMarkComponentEndOfFrameUpdateState
{
    static void Set(UActorComponent* Component, int32 ArrayIndex, const EComponentMarkedForEndOfFrameUpdateState::Type UpdateState)
    {
        Component->MarkedForEndOfFrameUpdateState = UpdateState;
        Component->MarkedForEndOfFrameArrayIndex = ArrayIndex;
    }

    static int32 GetArrayIndex(UActorComponent* Component)
    {
        return Component->MarkedForEndOfFrameUpdateArrayIndex;
    }

    static void SetMarkedForPreEndOfFrameSync(UActorComponent* Component)
    {
        Component->bMarkedForPreEndOfFrameSync = true;
    }

    static void ClearMarkedForPreEndOfFrameSync(UActorComponent* Component)
    {
        Component->bMarkedForPreEndOfFrameSync = false;
    }
};

/**
 * a helper RAII class to set the relevant context on a UWorld for a particular FLevelCollection within a scope
 * the constructor will set the PersistentLevel, GameState, NetDriver, and DemoNetDriver on the world and the destructor will restore the original values 
 */
// 002 - Foundation - TickTaskManager - FScopedLevelCollectionContextSwitch
// haker: RAII (Resource Acquisition is initialization) pattern:
// - change ActiveLevelCollection in UWorld
// - see constructor
// - see destructor
class FScopedLevelCollectionContextSwitch
{
public:
    /**
     * constructor that will save the current relevant values of InWorld
     * and set the collection's context values for InWorld 
     */
    FScopedLevelCollectionContextSwitch(int32 InLevelCollectionIndex, UWorld* const InWorld)
        : World(InWorld)
        , SavedTickingCollectionIndex(InWorld ? InWorld->GetActiveLevelCollectionIndex() : INDEX_NONE)
    {
        if (World)
        {
            // see UWorld::SetActiveLevelCollection (goto 003)
            World->SetActiveLevelCollection(InLevelCollectionIndex);
        }
    }

    /** the destructor restores the context on the world that was saved in the constructor */
    ~FScopedLevelCollectionContextSwitch()
    {
        if (World)
        {
            World->SetActiveLevelCollection(SavedTickingCollectionIndex);
        }
    }

private:
    class UWorld* World;
    int32 SavedTickingCollectionIndex;
};

typedef TArray<TWeakObjectPtr<APhysicsVolume> >::TConstIterator FConstPhysicsVolumeIterator;

/** tick function that starts the physics tick */
struct FStartPhysicsTickFunction : public FTickFunction
{
    /** world this tick function belongs to */
    UWorld* Target;
};

/** tick function that ends the physics tick */
struct FEndPhysicsTickFunction : public FTickFunction
{
    /** world this tick function belongs to */
    UWorld* Target;
};

/** types of collision shapes that are used by Trace */
namespace ECollisionShape
{
    enum Type
    {
        Line,
        Box,
        Sphere,
        Capsule,
    };

    union
    {
        struct
        {
            float HalfExtentX;
            float HalfExtentY;
            float HalfExtentZ;
        } Box;

        struct
        {
            float Radius;
        } Sphere;

        struct
        {
            float Radius;
            float HalfHeight;
        } Capsule;
    };
};

/** collision shapes that supports Sphere, Capsule, Box or Line */
struct FCollisionShape
{
    ECollisionShape::Type ShapeType;
};

/** tests shape components more efficiently than the with-adjustment case, but less efficient ppr-poly collision for meshes */
static bool ComponentEncroachesBlockingGeometry_NoAdjustment(UWorld const* World, AActor const* TestActor, UPrimitiveComponent const* PrimComp, FTransform const& TestWorldTransform, const TArray<AActor*>& IgnoreActors)
{
    float const Epsilon = CVarEncroachEpsilon.GetValueOnGameThread();
    if (World && PrimComp)
    {
        bool bFoundBlockingHit = false;

        ECollisionChannel const BlockingChannel = PrimComp->GetCollisionObjectType();
        FCollisionShape const CollisionShape = PrimComp->GetCollisionShape(-Epsilon);

        if (CollisionShape.IsBox() && (Cast<UBoxComponent>(PrimComp) == nullptr))
        {
            // we have a bounding box not for a box component, which means this was the fallback aabb
            // since we don't need the penetration info, go ahead and test the component itself for overlaps, which is more accurate
            if (PrimComp->IsRegistered())
            {
                // must be registered
                TArray<FOverlapResult> Overlaps;
                FComponentQueryParams Params(SCENE_QUERY_STAT(ComponentEncroachesBlockingGeometry_NoAdjustment), TestActor);
                FCollisionResponseParams ResponseParams;
                PrimComp->InitSweepCollisionParams(Params, ResponseParams);
                Params.AddIngoredActors(IgnoreActors);
                return World->ComponentOverlapMultiByChannel(Overlaps, PrimComp, TestWorldTransform.GetLocation(), TestWorldTransform.GetRotation(), BlockingChannel, Params);
            }
            else
            {
                return false;
            }
        }
        else
        {
            FCollisionQueryParams Params(SCENE_QUERY_STAT(ComponentEncroachesBlockingGeometry_NoAdjustment), false, TestActor);
            FCollisionResponseParams ResponseParams;
            PrimComp->InitSweepCollisionParams(Params, ResponseParams);
            Params.AddIgnoredActors(IgnoreActors);
            return World->OverlapBlockingTestByChannel(TestWorldTransform.GetLocation(), TestWorldTransform.GetRotation(), BlockingChannel, CollisionShape, Params, ResponseParams);
        }
    }

    return false;
}

static FVector CombineAdjustments(FVector CurrentAdjustment, FVector AdjustmentToAdd)
{
    // remove the part of the new adjustment that's parallel to the current adjustment
    if (CurrentAdjustment.IsZero())
    {
        return AdjustmentToAdd;
    }

    FVector Projection = AdjustmentToAdd.ProjectOnTo(CurrentAdjustment);
    Projection = Projection.GetClampedToMaxSize(CurrentAdjustment.Size());

    FVector OrthogalAdjustmentToAdd = AdjustmentToAdd - Projection;
    return CurrentAdjustment + OrthogalAdjustmentToAdd;
}

/** Tests if the given component overlaps any blocking geometry if it were placed at the given world transform, optionally returns a suggested translation to get the component away from its overlaps. */
static bool ComponentEncroachesBlockingGeometry(UWorld const* World, AActor const* TestActor, UPrimitiveComponent const* PrimComp, FTransform const& TestWorldTransform, FVector* OutProposedAdjustment, const TArray<AActor*>& IgnoreActors)
{
    // haker: we are going to look into simple version NoAdjustment
	return OutProposedAdjustment
		? ComponentEncroachesBlockingGeometry_WithAdjustment(World, TestActor, PrimComp, TestWorldTransform, *OutProposedAdjustment, IgnoreActors)
		: ComponentEncroachesBlockingGeometry_NoAdjustment(World, TestActor, PrimComp, TestWorldTransform, IgnoreActors);
}

/** 
 * the world is the top level object representing a map or a sandbox in which Actors and Components will exist and be rendered 
 * 
 * a world can be a single persistent level with an optional list of streaming levels that are loaded and unloaded via volumes and blueprint functions
 * or it can be a collection of levels organized with a World Composition (->haker: OLD COMMENT...)
 * 
 * in a standalone game, generally only a single World exists except during seamless area transition when both a destination and current world exists
 * in the editor many Worlds exist: 
 * - the level being edited
 * - each PIE instance
 * - each editor tool which has an interactive rendered viewport, and many more
 */
// 004 - Foundation - CreateWorld ** - UWorld class
// see UObject
// haker: now see World's member variables (goto 9)
class UWorld final : public UObject, public FNetworkNotify
{
    // 002 - Foundation - CreateWorld ** - UWorld::InitializationValues
    // see FWorldInitializationValues
    using InitializationValues = FWorldInitializationValues;

    using FActorsInitializedParams = ::FActorsInitializedParams;

    FConstPlayerControllerIterator GetPlayerControllerIterator() const
    {
        return PlayerControllerList.CreateConstIterator();
    }

    FConstCameraActorIterator GetAutoActivateCameraIterator() const
    {
        auto Result = AutoCameraActorList.CreateConstIterator();
        return (const FConstCameraActorIterator&)Result;
    }

    bool IsGameWorld() const
    {
        return WorldType == EWorldType::Game || WorldType == EWorldType::PIE || WorldType == EWorldType::GamePreview || WorldType == EWorldType::GameRPC;
    }

    /** returns true if this world should look at game hidden flags instead of editor hidden flags for the purposes of rendering */
    bool UsesGameHiddenFlags() const
    {
        return IsGameWorld();
    }

    /** static function that creates a new UWorld and returns a pointer to it */
    // 001 - Foundation - CreateWorld ** - UWorld::CreateWorld
    // haker: note that this function is static function
    // - when you look through parameters in a function, care about their types
    // - when we use UWorld::CreateWorld(), we only pass two parameters, so rest of parameters is not necessary to care
    static UWorld* CreateWorld(
        const EWorldType::Type InWorldType, 
        bool bInformEngineOfWorld, 
        FName WorldName = NAME_None, 
        UPackage* InWorldPackage = NULL, 
        bool bAddToRoot = true, 
        ERHIFeatureLevel::Type InFeatureLevel = ERHIFeatureLevel::Num, 
        // haker:
        // see InitializationValues
        const InitializationValues* InIVS = nullptr, 
        bool bInSkipInitWorld = false
    )
    {
        // haker: UPackage will be covered in the future, dealing with AsyncLoading
        // - for now, just think of it as describing file format for UWorld
        // - one thing to remember is that each world has 1:1 mapping on separate package
        //   - it is natural that all data in world needs to be serialized as file
        //   - in unreal engine, saving file means 'package', UPackage
        // - another thing is that UObject has OuterPrivate:
        //   - OuterPrivate infers where object is resides
        //   - normally OuterPrivate is set as UPackage: 
        //     - where object is resides == what file object resides in
        // - I'd like to explain what I understand about UPackage, don't memorize it, just say 'ah' is enough!
        UPackage* WorldPackage = InWorldPackage;
        if (!WorldPackage)
        {
            // haker: UWorld needs package as its OuterPrivate and need to be serialized
            WorldPackage = CreatePackage(nullptr);
        }

        if (InWorldType == EWorldType::PIE)
        {
            // haker: like ObjectFlags in UObjectBase, UPackage's attribute can be set by flags in similar manner
            // - we are not going to read it in detail, later we could have chance to meet again
            WorldPackage->SetPackageFlags(PKG_PlayInEditor);
        }

        // mark the package as containing a world
        // haker: what is 'Transient' in Unreal Engine?
        // - if you have property or asset which is not serialized, we mark it as 'Transient'
        // - Transient == (meta data to mark it as not-to-be-serialize)
        if (WorldPackage != GetTransientPackage())
        {
            WorldPackage->ThisContainsMap();
        }

        const FString WorldNameString = (WorldName != NAME_None) ? WorldName.ToString() : TEXT("Untitled");

        // see UWorld class (goto 4)

        // haker: we set NewWorld's outer as WorldPackage:
        // - normally, when you look into outer object, it will finally end-up-with package(asset file containing this UObject)
        UWorld* NewWorld = NewObject<UWorld>(WorldPackage, *WorldNameString);

        // haker: UObject::SetFlags -> set unreal object's attribute with flag by bit operator (AND(&), OR(|), SHIFT(>>, <<) etc...)
        // - refer to EObjectFlags and UObject::ObjectFlags
        NewWorld->SetFlags(RF_Transactional);
        NewWorld->WorldType = InWorldType;
        NewWorld->SetFeatureLevel(InFeatureLevel);

        NewWorld->InitializeNewWorld(
            InIVS ? *InIVS : UWorld::InitializationValues()
                // haker: as we saw FWorldInitializationValues, the below member functions mark the flag to refer when we create new world
                .CreatePhysicsScene(InWorldType != EWorldType::Inactive)
                .ShouldSimulatePhysics(false)
                .EnableTraceCollision(true)
                .CreateNavigation(InWorldType == EWorldType::Editor)
                .CreateAISystem(InWorldType == EWorldType::Editor)
            , bInSkipInitWorld);

        // see UWorld::InitializeNewWorld (goto 25)
        
        // clear the dirty flags set during SpawnActor and UpdateLevelComponents
        WorldPackage->SetDirtyFlag(false);

        if (bAddToRoot)
        {
            // add to root set so it doesn't get GC'd
            NewWorld->AddToRoot();
        }

        // tell the engine we are adding a world (unless we are asked not to)
        if ((GEngine) && (bInformEngineOfWorld == true))
        {
            GEngine->WorldAdded(NewWorld);
        }

        return NewWorld;

        // haker: search (goto 001)
    }

    /** set up the physics tick function if they aren't already */
    // 004 - Foundation - TickTaskManager - UWorld::SetupPhysicsTickFunctions
    void SetupPhysicsTickFunctions(float DeltaSeconds)
    {
        // haker: see UWorld's member variables (goto 005)
        StartPhysicsTickFunction.bCanEverTick = true;
        StartPhysicsTickFunction.Target = this;

        EndPhysicsTickFunction.bCanEverTick = true;
        EndPhysicsTickFunction.Target = this;

        bool bEnablePhysics = bShouldSimulatePhysics;

        // see if we need to update tick registration
        // haker: if both tick functions are NOT registered yet, register both functions
        // - note that we can define/register TickFunction in any class like this
        //   - no need to be in UObject/AActor/UActorComponent
        bool bNeedToUpdateTickRegistration = (bEnablePhysics != StartPhysicsTickFunction.IsTickFunctionRegistered())
            || (bEnablePhysics != EndPhysicsTickFunction.IsTickFunctionRegistered());

        // haker: note that if bEnablePhysics == false, we are going to unregister both tick functions
        if (bNeedToUpdateTickRegistration && PersistentLevel)
        {
            if (bEnablePhysics && !StartPhysicsTickFunction.IsTickFunctionRegistered())
            {
                StartPhysicsTickFunction.TickGroup = TG_StartPhysics;
                StartPhysicsTickFunction.RegisterTickFunction(PersistentLevel);
            }
            else if (!bEnablePhysics && StartPhysicsTickFunction.IsTickFunctionRegistered())
            {
                StartPhysicsTickFunction.UnRegisterTickFunction();
            }

            if (bEnablePhysics && !EndPhysicsTickFunction.IsTickingFunctionRegistered())
            {
                EndPhysicsTickFunction.TickGroup = TG_EndPhysics;
                EndPhysicsTickFunction.RegisterTickFunction(PersistentLevel);
                // haker: EndPhysicsTickFunction has prerequisite to StartPhysicsTickFunction:
                // - it means that EndPhysicsTickFunction can start only if StartPhysicsTickFunction is finished
                // see FTickFunction::AddPrerequisite (goto 006)
                EndPhysicsTickFunction.AddPrerequisite(this, StartPhysicsTickFunction);
            }
            else if (!bEnablePhysics && EndPhysicsTickFunction.IsTickFunctionRegistered())
            {
                // see FTickFunction::RemovePrerequisite (goto 010 : TickTaskManager)
                EndPhysicsTickFunction.RemovePrerequisite(this, StartPhysicsTickFunction);
                EndPhysicsTickFunction.UnRegisterTickFunction();
            }  
        }

        // search (goto 004)
    }

    /** run a tick group, ticking all actors and components */
    // 033 - Foundation - TickTaskManager - UWorld::RunTickGroup
    void RunTickGroup(ETickingGroup Group, bool bBlockTillComplete = true)
    {
        // haker: we try to call 'unlock' corresponding tick functions cached in [HiPri]TickTasks of TTS
        // - and we update TickGroup as next by adding 'one'
        // see FTickTaskManager::RunTickGroup (goto 034)
        FTickTaskManagerInterface::Get().RunTickGroup(Group, bBlockTillComplete);
        
        // new actors go into the next tick group because this one is already gone
        TickGroup = ETickingGroup(TickGroup + 1);
    }

    /**
     * update the level after a variable amount of time, DeltaSeconds, has passed
     * all child actors are ticked after their owners have been ticked 
     */
    // 001 - Foundation - TickTaskManager - UWorld::Tick
    void Tick(ELevelTick TickType, float DeltaSeconds)
    {
        FWorldDelegates::OnWorldTickStart.Broadcast(this, TickType, DeltaSeconds);

        bool bDoingActorTicks = 
            (TickType != LEVELTICK_TimeOnly);

        // if only the DynamicLevel collection has entries, we can skip the validation and tick all levels
        // haker: it just minor optimization to skip the code: Levels.Contains(CollectionLevel)
        bool bValidateLevelList = false;
        for (const FLevelCollection& LevelCollection : LevelCollections)
        {
            if (LevelCollection.GetType() != ELevelCollectionType::DynamicSourceLevels)
            {
                const int32 NumLevels = LevelCollection.GetLevels().Num();
                if (NumLevels != 0)
                {
                    bValidateLevelList = true;
                    break;
                }
            }
        }

        // haker: we can understand what FLevelCollection for:
        // - object can be categorized into static/dynamic
        // - dynamic or static objects are into different category (LevelCollection)
        // - each level collection ticks on different moment (dynamic -> static)
        for (int32 i = 0; i < LevelCollections.Num(); ++i)
        {
            // build a list of levels from the collection that are also in the world's level array
            // collections may contain levels that aren't loaded in the world at the moment
            TArray<ULevel*> LevelsToTick;
            for (ULevel* CollectionLevel : LevelCollections[i].GetLevels())
            {
                // haker: here!
                const bool bAddToTickList = (bValidateLevelList == false) || Levels.Contains(CollectionLevel);
                if (bAddToTickList && CollectionLevel)
                {
                    LevelsToTick.Add(CollectionLevel);
                }
            }

            // set up context on the world for this level collection
            // see FScopedLevelCollectionContextSwitch (goto 002)
            // haker: update active level collection by level collection's index
            FScopedLevelCollectionContextSwitch LevelContext(i, this);

            // if caller wants time update only, or we are paused, skip the rest
            const bool bShouldSkipTick = (LevelsToTick.Num() == 0);
            if (bDoingActorTicks && !bShouldSkipTick)
            {
                // actually tick actors now that context is set up
                // haker: understand what SetupPhysicsTickFunctions() does for us:
                // see UWorld::SetupPhysicsTickFunctions (goto 004)
                SetupPhysicsTickFunctions(DeltaSeconds);

                TickGroup = TG_PrePhysics; // reset this to the start tick group

                // see FTickTaskManager::StartFrame (goto 011)
                FTickTaskManagerInterface::Get().StartFrame(this, DeltaSeconds, TickType, LevelsToTick);

                {
                    // see UWorld::RunTickGroup (goto 033 : TickTaskManager)
                    RunTickGroup(TG_PrePhysics);
                }
                {
                    RunTickGroup(TG_StartPhysics);
                }
                {
                    // no wait here, we should run until idle though
                    // we don't care if all of the async ticks are done before we start running post-phys stuff
                    // haker: note that bBlockUntilComplete == false
                    RunTickGroup(TG_DuringPhysics, false);
                }
                {
                    // set this here so the current tick group is correct during collision notifies, though I am not sure it matters 
                    // 'cause of the false up there'
                    TickGroup = TG_EndPhysics;
                    RunTickGroup(TG_EndPhysics);
                }
                {
                    RunTickGroup(TG_PostPhysics);
                }
            }

            //...

            {
                // update camera
                // 026 - Foundation - SceneRenderer * - UWorld::Tick
                {
                    // update camera last: this needs to be done before NetUpdates, and after all actors have been ticked
                    // haker: iterate PlayerControllers and update respective PlayerCameraManager
                    for (FConstPlayerControllerIterator Iterator = GetPlayerControllerIterator(); Iterator; ++Iterator)
                    {
                        if (APlayerController* PlayerController = Iterator->Get())
                        {
                            // haker: see APlayerController::UpdateCameraManager (goto 027: SceneRenderer)
                            PlayerController->UpdateCameraManager(DeltaSeconds);
                        }
                    }
                }
            }

            //...

            // various ticks:
            // 1. LatentActionManager
            // 2. TimerManager
            // 3. TickableGameObjects
            // 4. CameraManager
            // 5. streaming volume

            if (bDoingActorTicks)
            {
                RunTickGroup(TG_PostUpdateWork);
                RunTickGroup(TG_LastDemotable);
            }

            // see FTickTaskManager::EndFrame (goto 038 : TickTaskManager)
            FTickTaskManagerInterface::Get().EndFrame();
        }

        FWorldDelegates::OnWorldTickEnd.Broadcast(this, TickType, DeltaSeconds);
    }

    /** duplicate the editor world to create the PIE world */
    // 004 - Foundation - CreateInnerProcessPIEGameInstance * - GetDuplicateWorldForPIE
    static UWorld* GetDuplicatedWorldForPIE(UWorld* InWorld, UPackage* InPIEPackage, int32 PIEInstanceID)
    {
        // haker: in SCS for SpawnActor, when we create new object with ComponentTemplate, we cover the case using StaticDuplicateObject()
        // - use EditorWorld as our source object to duplicate
        FObjectDuplicationParameters Parameters(InWorld, InPIEPackage);
        Parameters.DestName = InWorld->GetFName();
        Parameters.DestClass = InWorld->GetClass();

        // haker: note that duplication option for PIE is explicitly specified
        Parameters.DuplicateMode = EDuplicateMode::PIE;
        Parameters.PortFlags = PPF_DuplicateForPIE;

        // haker: note that duplicate UWorld, it is same as calling object duplication StaticDuplicateObjectEx()
        UWorld* DuplicatedWorld = CastChecked<UWorld>(StaticDuplicateObjectEx(Parameters));
        return DuplicatedWorld;
    }

    /** initialize all world subsystems */
    // 029 - Foundation - CreateWorld ** - UWorld::InitializeSubsystems
    void InitializeSubsystems()
    {
        // see FSubsystemCollectionBase::Initialize (goto 30)
        SubsystemCollection.Initialize(this);
    }

    /** finalize initialization of all world subsystems */
    // 037 - Foundation - CreateWorld ** - UWorld::PostInitializeSubsystems
    void PostInitializeSubsystems()
    {
        // haker: with SystemArrayMap, we can all derived classes from UWorldSubsystem
        const TArray<UWorldSubsystem*>& WorldSubsystems = SubsystemCollection.GetSubsystemArray<UWorldSubsystem>(UWorldSubsystem::StaticClass());
        for (UWorldSubsystem* WorldSubsystem : WorldSubsystems)
        {
            WorldSubsystem->PostInitialize();
        }
    }

    /** returns the AWorldSettings actor associated with this world */
    // 033 - Foundation - CreateWorld ** - UWorld::GetWorldSettings
    AWorldSettings* GetWorldSettings(bool bCheckStreamingPersistent = false, bool bChecked = true) const
    {
        AWorldSettings* WorldSettings = nullptr;
        if (PersistentLevel)
        {
            // haker: do you remember that we create AWorldSettings setting its outer as PersistentLevel?
            WorldSettings = PersistentLevel->GetWorldSettings(bChecked);
            if (bCheckStreamingPersistent)
            {
                //...
            }
        }
        return WorldSettings;
    }

    // 035 - Foundation - CreateWorld ** - UWorld::InternalGetDefaultPhysicsVolume
    APhysicsVolume* InternalGetDefaultPhysicsVolume() const
    {
        // haker: if we don't have any DefaultPhysicsVolume yet, create new one
        if (DefaultPhysicsVolume == nullptr)
        {
            // haker: we get the PhysicssVolume's class to instantiate:
            // - WorldSettings have class definitions which are needed for the world, which could change overall behavior for world
            // - you could also override the class in WorldSettings
            AWorldSettings* WorldSettings = GetWorldSettings(false, false);
            UClass* DefaultPhysicsVolumeClass = (WorldSettings ? WorldSettings->DefaultPhysicsVolumeClass : nullptr);
            if (DefaultPhysicsVolumeClass == nullptr)
            {
                DefaultPhysicsVolumeClass = ADefaultPhysicsVolume::StaticClass();
            }

            // spawn volume:
            // haker: here is FActorSpawnParameters
            FActorSpawnParameters SpawnParams;
            SpawnParams.bAllowDuringConstructionScript = true;
            {
                UWorld* MutableThis = const_cast<UWorld*>(this);
                MutableThis->DefaultPhysicsVolume = MutableThis->SpawnActor<APhysicsVolume>(DefaultPhysicsVolumeClass, SpawnParams);
                MutableThis->DefaultPhysicsVolume->Priority = -1000000;
            }
        }
        return DefaultPhysicsVolume;
    }

    /** returns the default physics volume and creates it if necessary */
    // 034 - Foundation - CreateWorld ** - UWorld::GetDefaultPhysicsVolume
    // see InternalGetDefaultPhysicsVolume (go to 35)
    APhysicsVolume* GetDefaultPhysicsVolume() const { return DefaultPhysicsVolume ? ToRawPtr(DefaultPhysicsVolume) : InternalGetDefaultPhysicsVolume(); }

    FLevelCollection* FindCollectionByType(const ELevelCollectionType InType)
    {
        for (FLevelCollection& LC : LevelCollections)
        {
            if (LC.GetType() == InType)
            {
                return &LC;
            }
        }
        return nullptr;
    }

    int32 FindOrAddCollectionByType_Index(const ELevelCollectionType InType)
    {
        const int32 FoundIndex = FindCollectionIndexByType(InType);

        if (FoundIndex != INDEX_NONE)
        {
            return FoundIndex;
        }

        // Not found, add a new one.
        FLevelCollection NewLC;
        NewLC.SetType(InType);
        return LevelCollections.Add(MoveTemp(NewLC));
    }

    /** creates the dynamic source and static level collections if they don't already exists */
    // 036 - Foundation - CreateWorld ** - UWorld::ConditionallyCreateDefaultLevelCollections
    void ConditionallyCreateDefaultLevelCollections()
    {
        LevelCollections.Reserve((int32)ELevelCollectionType::MAX);

        // create main level collection; the persistent level will always be considered dynamic
        if (!FindCollectionByType(ELevelCollectionType::DynamicSourceLevels))
        {
            // default to the dynamic source collection
            // see FindOrAddCollectionByType_Index
            ActiveLevelCollectionIndex = FindOrAddCollectionByType_Index(ELevelCollectionType::DynamicSourceLevels);

            // haker: dynamc/static level collections are set its persistent level for World's persistent level
            LevelCollections[ActiveLevelCollectionIndex].SetPersistentLevel(PersistentLevel);

            // don't add the persistent level if it is already a member of another collection
            // this may be the case if, for example, this world is the outer of a streaming level,
            // in which case the persistent level may be in one of the collections in the streaming level's OwningWorld
            if (PersistentLevel->GetCachedLevelCollection() == nullptr)
            {
                // haker: if persistent level is not set, defaultly add it to the dynamic level collection
                LevelCollections[ActiveLevelCollectionIndex].AddLevel(PersistentLevel);
            }
        }

        if (!FindCollectionByType(ELevelCollectionType::StaticLevels))
        {
            FLevelCollection& StaticCollection = FindOrAddCollectionByType(ELevelCollectionType::StaticLevels);
            StaticCollection.SetPersistentLevel(PersistentLevel);
        }
    }

    /** initializes the world, associates the persistent level and sets the proper zones */
    // 028 - Foundation - CreateWorld ** - UWorld::InitWorld
    void InitWorld(const FWorldInitializationValues IVS = FWorldInitializationValues())
    {
        // haker: CoreUObjectDelegate has delegates related to GC events -> USEFUL!
        FCoreUObjectDelegates::GetPostGarbageCollect().AddUObject(this, &UWorld::OnPostGC);

        // haker: we initialize UWorldSubsystems
        // see UWorld::InitializeSubsystems(goto 29)
        InitializeSubsystems();

        FWorldDelegates::OnPreWorldInitialization.Broadcast(this, IVS);

        // see UWorld::GetWorldSettings (goto 33)
        AWorldSettings* WorldSettings = GetWorldSettings();
        if (IVS.bInitializeScenes)
        {
            if (IVS.bCreatePhysicsScene)
            {
                // haker: we skip the detail of physics scene
                // - create physics world
                CreatePhysicsScene(WorldSettings);
            }

            bShouldSimulatePhysics = IVS.bShouldSimulatePhysics;

            // haker: create render world (== FScene)
            // - render world is FScene. we'll cover this later 
            bRequiresHitProxies = IVS.bRequiresHitProxies;
            GetRendererModule().AllocateScene(this, bRequiresHitProxies, IVS.bCreateFXSystem, GetFeatureLevel());
        }
        
        // haker: add persistent level
        // - you can think of persistent level to have world info(AWorldSettings)
        Levels.Empty(1);
        Levels.Add(PersistentLevel);

        PersistentLevel->OwningWorld = this;
        PersistentLevel->bIsVisible = true;

        // initialize DefaultPhysicsVolume for the world
        // spawned on demand by this function
        
        // see GetDefaultPhysicsVolume (goto 34)
        DefaultPhysicsVolume = GetDefaultPhysicsVolume();

        // find gravity
        if (GetPhysicsScene())
        {
            FVector Gravity = FVector( 0.f, 0.f, GetGravityZ() );
            GetPhysicsScene()->SetUpForFrame( &Gravity, 0, 0, 0, 0, 0, false);
        }

        // create physics collision handler
        if (IVS.bCreatePhysicsScene)
        {
            //...
        }

        // haker: from here, UWorld's URL is from Persistnet's URL
        URL = PersistentLevel->URL;
#if WITH_EDITORONLY_DATA || 1
	    CurrentLevel = PersistentLevel;
#endif

        // see ConditionallyCreateDefaultLevelCollections (goto 36)
        // haker: make sure all level collection types are instantiated
        ConditionallyCreateDefaultLevelCollections();

        // we're initialized now:
        bIsWorldInitialized = true;
        bHasEverBeenInitialized = true;

        FWorldDelegate::OnPostWorldInitialization.Broadcast(this, IVS);

        PersistentLevel->OnLevelLoaded();

        // haker: notify streaming manager that new level is added
        IStreamingManager::Get().AddLevel(PersistentLevel);

        // see PostInitializeSubsystems (goto 37)
        // haker: call PostInitialize() on each UWorldSubsystem
        PostInitializeSubsystems();

        BroadcastLevelsChanged();

        // haker: let's wrap it up what InitWorld() have done:
        // 1. initialize WorldSubsystemCollection
        // 2. allocate(create) render-world(FScene) and physics-world(FPhysScene)
        // search (goto 28)
    }

    /** updates world components like e.g. line batcher and all level components */
    // 038 - Foundation - CreateWorld ** - UWorld::UpdateWorldComponents
    void UpdateWorldComponents(bool bRerunConstructionScripts, bool bCurrentLevelOnly, FRegisterComponentContext* Context = nullptr)
    {
        if (!IsRunningDedicatedServer())
        {
            // haker:
            // - ULineBatchComponent is herited from UPrimitiveComponent
            // - ULineBatchComponent can be seen as UActorComponent
            // - UWorld is NOT AActor, but has UActorComponent as its dynamic object for LineBatcher
            // - LineBatchers should be registered separately
            if (!LineBatcher)
            {
                LineBatcher = NewObject<ULineBatchComponent>();
                LineBatcher->bCalculateAccurateBounds = false;
            }

            if(!LineBatcher->IsRegistered())
            {	
                // see UActorComponent::RegisterComponentWithWorld (goto 39)
                LineBatcher->RegisterComponentWithWorld(this, Context);
            }

            if(!PersistentLineBatcher)
            {
                PersistentLineBatcher = NewObject<ULineBatchComponent>();
                PersistentLineBatcher->bCalculateAccurateBounds = false;
            }

            if(!PersistentLineBatcher->IsRegistered())	
            {
                PersistentLineBatcher->RegisterComponentWithWorld(this, Context);
            }

            if(!ForegroundLineBatcher)
            {
                ForegroundLineBatcher = NewObject<ULineBatchComponent>();
                ForegroundLineBatcher->bCalculateAccurateBounds = false;
            }

            if(!ForegroundLineBatcher->IsRegistered())	
            {
                ForegroundLineBatcher->RegisterComponentWithWorld(this, Context);
            }
        }

        // 066 - Foundation - CreateWorld *
        {
            // haker: now iterating sub-levels and call UpdateLevelComponents for each Level
            for (int32 LevelIndex = 0; LevelIndex < Levels.Num(); ++LevelIndex)
            {
                ULevel* Level = Levels[LevelIndex];
                
                // haker: we skip the details how the FLevelUtils::FindStreamingLevel() works
                ULevelStreaming* StreamingLevel = FLevelUtils::FindStreamingLevel(Level);
                
                // update the level only if it is visible (or not a streamed level)
                if (!StreamingLevel || Level->bIsVisible)
                {
                    // see UpdateLevelComponents (goto 067)
                    Level->UpdateLevelComponents(bRerunConstructionScripts, Context);
                    IStreamingManager::Get().AddLevel(Level);
                }
            }
        }
        // haker: we finish all levels for registration

        // iterate world-subsystems and notify all components in UWorld are registered
        const TArray<UWorldSubsystem*>& WorldSubsystems = SubsystemCollection.GetSubsystemArray<UWorldSubsystem>(UWorldSubsystem::StaticClass());
        for (UWorldSubsystem* WorldSubsystem : WorldSubsystems)
        {
            WorldSubsystem->OnWorldComponentsUpdated(*this);
        }
    }

    /** initializes a newly created world */
    // 025 - Foundation - CreateWorld ** - UWorld::InitializeNewWorld
    void InitializeNewWorld(const InitializationValues IVS = InitializationValues(), bool bInSkipInitWorld = false)
    {
        if (!IVS.bTransactional)
        {
            // haker: clear ObjectFlags in UObjectBase (bit-wise operation)
            ClearFlags(RF_Transactional);
        }

        // haker: create default persistent level for new world
        // - note that we pass current UWorld as Outer for PersistentLevel
        // - for persistent level, OwningWorld == OuterPrivate
        PersistentLevel = NewObject<ULevel>(this, TEXT("PersistentLevel"));
        PersistentLevel->OwningWorld = this;

#if WITH_EDITORONLY_DATA || 1
        CurrentLevel = PersistentLevel;
#endif

        // create the WorldInfo actor
        // haker: we are NOT looking into AWorldSettings in detail, but try to understand it with the example
        // [*] explain AWorldSettings in the editor
        AWorldSettings* WorldSettings = nullptr;
        {
            // haker: when you spawn an Actor, you need to pass FActorSpawnParameters
            // see FActorSpawnParameters (goto 26)
            FActorSpawnParameters SpawnInfo;
            // see ESpawnActorCollisionHandlingMethod (goto 27)
            SpawnInfo.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

            // set constant name for WorldSettings to make a network replication work between new worlds on host and client
            // haker: you can override WorldSettings' class using UEngine's WorldSettingClass
            SpawnInfo.Name = GEngine->WorldSettingsClass->GetFName();

            // haker: we'll cover SpawnActor in the future (I just leave it in to-do list)
            WorldSettings = SpawnActor<AWorldSettings>(GEngine->WorldSettingsClass, SpawnInfo);
        }
        
        // allow the world creator to override the default game mode in case they do not plan to load a level
        if (IVS.DefaultGameMode)
        {
            WorldSettings->DefaultGameMode = IVS.DefaultGameMode;
        }

        // haker: persistent level creates AWorldSettings which contain world info like GameMode
        PersistentLevel->SetWorldSettings(WorldSettings);

#if WITH_EDITOR || 1
        WorldSettings->SetIsTemporaryHiddenInEditor(true);

        if (IVS.bCreateWorldParitition)
        {
            // haker: we skip world partition for now
            // - BUT, lyra is based on World-Partition
        }
#endif

        if (!bInSkipInitWorld)
        {
            // initialize the world
            // see UWorld::InitWorld (goto 28)
            InitWorld(IVS);

            // update components
            // see UWorld::UpdateWorldComponents (goto 38)
            const bool bRerunConstructionScripts = !FPlatformProperties::RequiresCookedData();
            UpdateWorldComponents(bRerunConstructionScripts, false);
        }
    }

    /** returns the owning game instance for this world */
    UGameInstance* GetGameInstance() const
    {
        return OwningGameInstance;
    }

    /** spawns GameMode for the level */
    // 022 - Foundation - CreateInnerProcessPIEGameInstance * - UGameInstance::SetGameMode
    bool SetGameMode(const FURL& InURL)
    {
        // haker: create new GameMode and cache it in UWorld::AuthorityGameMode:
        // - as we saw, in dedicated server, only server creates GameMode!
        // - see UGameInstance::CreateGameModeForURL (goto 023: CreateInnerProcessPIEGameInstance)
        AuthorityGameMode = GetGameInstance()->CreateGameModeForURL(InURL, this);
        if (AuthorityGameMode != NULL)
        {
            return true;
        }
        return false;
    }

    /** returns true if the actors have been initialized and are ready to start play */
    bool AreActorsInitialized() const
    {
        // haker: only persistent level's actors are checked
        return bActorsInitialized && PersistentLevel && PersistentLevel->Actors.Num();
    }

    /** initializes all actors and prepares them to start gameplay */
    // 024 - Foundation - CreateInnerProcessPIEGameInstance * - UWorld::InitializeActorsForPlay
    void InitializeActorsForPlay(const FURL& InURL, bool bResetTime = true, FRegisterComponentContext* Context = nullptr)
    {
        // update world and the components of all levels
        // haker: we had covered it previously
        UpdateWorldComponents(false, true, Context);

        // init level gameplay info 
        // haker: only persistent level
        // - see AreActorsInitialized() briefly
        if (!AreActorsInitialized())
        {
            // enable actor script calls
            bActorsInitialized = true;

            // init the game mode
            if (AuthorityGameMode && !AuthorityGameMode->IsActorInitialized())
            {
                // haker: we skip InitGame() for now
                AuthorityGameMode->InitGame(FPaths::GetBaseFilename(InURL.Map), Options, Error);
            }

            // route various initialization functions and set volumes
            const int32 ProcessAllRouteActorInitializationGranularity = 0;
            for (int32 LevelIndex = 0; LevelIndex < Levels.Num(); ++LevelIndex)
            {
                ULevel* const Level = Levels[LevelIndex];
                // haker: see RouteActorInitialize (goto 025: CreateInnerProcessPIEGameInstance)
                Level->RouteActorInitialize(ProcessAllRouteActorInitializationGranularity);
            }
        }

        // rearrange actors: static not net relevant actors first, then static net relevant actors and then others
        // haker: we skip this:
        // - it is related to networking
        for (int32 LevelIndex = 0; LevelIndex < Levels.Num(); ++LevelIndex)
        {
            ULevel* Level = Levels[LevelIndex];
            Level->SortActorList();
        }

        // let others know the actors are initialized
        FActorInitializedParams OnActorInitParams(this, bResetTime);
        FWorldDelegates::OnWorldInitializedActors.Broadcast(OnActorInitParams);
    }

    /**
     * returns the current GameMode instance which is always valid during gameplay on the server (or standalone)
     * this will only return a valid pointer on the server; will always return null on a client 
     */
    AGameModeBase* GetAuthGameMode() const { return AuthorityGameMode; }

    /** spawns a PlayerController and binds it to the passed in Player with the specified RemoteRole and options */
    // 028 - Foundation - CreateInnerProcessPIEGameInstance * - UWorld::SpawnPlayActor
    APlayerController* SpawnPlayActor(class UPlayer* Player, ENetRole RemoteRole, const FURL& InURL, const FUniqueNetIdRepl& UniqueId, FString& Error, uint8 InNetPlayerIndex = 0)
    {
        Error = TEXT("");

        FString Options;
        for (int32 i = 0; i < InURL.Op.Num(); ++i)
        {
            Options += TEXT("?");
            Options += InURL.Op[i];
        }

        if (AGameModeBase* const GameMode = GetAuthGameMode())
        {
            // give the GameMode a chance to accept the login
            // see AGameModeBase::Login (goto 029: CreateInnerProcessPIEGameInstance)
            APlayerController* const NewPlayerController = GameMode->Login(NewPlayer, RemoteRole, *InURL.Portal, Options, UniqueId, Error);
            if (NewPlayerController == NULL)
            {
                UE_LOG(LogSpawn, Warning, TEXT("Login failed: %s"), *Error);
                return NULL;
            }
            
            // possess the newly-spawned player
            NewPlayerController->NetPlayerIndex = InNetPlayerIndex;
            NewPlayerController->SetRole(ROLE_Authority);
            NewPlayerController->SetReplicate(RemoteRole != ROLE_None);
            if (RemoteRole == ROLE_AutonomousProxy)
            {
                // haker: we skip this function (networking)
                NewPlayerController->SetAutonomousProxy(true);
            }

            // 036 - Foundation - CreateInnerProcessPIEGameInstance * - End of Login

            // haker: see SetPlayer (goto 037: CreateInnerProcessPIEGameInstance)
            NewPlayerController->SetPlayer(NewPlayer);

            // haker: see PostLogin (goto 043: CreateInnerProcessPIEGameInstance)
            GameMode->PostLogin(NewPlayerController);
            
            return NewPlayerController;
        }

        return nullptr;
    }

    /**
     * adds the passed in actor to the special network actor list
     * this list is used to specifically single out actors that are relevant for networking without having to scan the much large list 
     */
    // 012 - Foundation - SpawnActor - AddNetworkActor
    void AddNetworkActor(AActor* Actor)
    {
        if (Actor == nullptr)
        {
            return;
        }

        if (Actor->IsPendingKillPending())
        {
            return;
        }

        if (!ContainsLevel(Actor->GetLevel()))
        {
            return;
        }

        ForEachNetDriver(GEngine, this, [Actor](UNetDriver* const Driver)
        {
            if (Driver != nullptr)
            {
                // haker: note that after spawning Actor, we add it to Driver as ActorChannel
                Driver->AddNetworkActor(Actor);
            }
        });
    }

    /** spawn Actors with given transform and SpawnParameters */
    // 001 - Foundation - SpawnActor - UWorld::SpawnActor
    AActor* SpawnActor(UClass* Class, FTransform const* UserTransformPtr, const FActorSpawnParameters& SpawnParameters = FActorSpawnParameters())
    {
        // haker: when we spawn Actor in world, Actor's outer will be persistent-level
        // - as you guess, persistent level's outer is world, so we can see Actor's outer as world
        ULevel* CurrentLevel = PersistentLevel;

        // haker: I abbreviate condition checks for clarity to see codes
#pragma region SpawnActor_Condition Check
        if (!Class->IsChildOf(AActor::StaticClass()))
        {
            return NULL;
        }
        if (Class->HasAnyClassFlags(CLASS_Deprecated))
        {
            return NULL;
        }
#pragma endregion

        // haker: determine where a new actor is spawned: to ULevel
        ULevel* LevelToSpawnIn = SpawnParameters.OverrideLevel;
        if (LevelToSpawnIn == NULL)
        {
            // spawn in the same level as the owner if we have one
            // haker: note that what level is set to CurrentLevel
            LevelToSpawnIn = (SpawnParameters.Owner != NULL) ? SpawnParameters.Owner->GetLevel() : ToRawPtr(CurrentLevel);
        }

        // use class's default actor as a template if none provided
        // haker: if SpawnParameter.Template is not provided, we use Class's CDO
        AActor* Template = SpawnParameters.Template ? SpawnParameters.Template : Class->GetDefaultObject<AActor>();
        check(Template);

        bool bNeedGloballyUniqueName = false;
        FName NewActorName = SpawnParameters.Name;
        if (NewActorName.IsNone())
        {
            // if we are using a template object and haven't specified a name, create a name relative to the template, otherwise let the default object naming behavior in Stat
            // haker: depending on Template, choose the base name of Actor's name:
            // - if Template is from CDO, use Class's name
            // - otherwise (Template is specified), use Template's name
            const FName BaseName = Template->HasAnyFlags(RF_ClassDefaultObject) ? Class->GetFName() : *Template->GetFName().GetPlainNameString();

            // haker: see FActorSpawnUtils::MakeUniqueActorName (goto 002: SpawnActor)
            // - we use bNeedGloballyUniqueName as 'false'
            NewActorName = FActorSpawnUtils::MakeUniqueActorName(LevelToSpawnIn, Template->GetClass(), BaseName, bNeedGloballyUniqueName);
        }
        else if (StaticFindObjectFast(nullptr, LevelToSpawnIn, NewActorName) 
            || (bNeedGloballyUniqueName != FActorSpawnUtils::IsGloballyUniqueName(NewActorName))
        {
            // haker: if the name of actor is overlapped, create new name
            NewActorName = FActorSpawnUtils::MakeUniqueActorName(LevelToSpawnIn, Template->GetClass(), FActorSpawnutils::GetBaseName(NewActorName), bNeedGloballyUniqueName);
        }

#if WITH_EDITOR || 1
        UPackage* ExternalPackage = nullptr;
        if (bNeedGloballyUniqueName && !ExternalPackage)
        {
            TStringBuilderWithBuffer<TCHAR, NAME_SIZE> ActorPath;
            ActorPath += LevelToSpawnIn->GetPathName();
            ActorPath += TEXT(".");
            ActorPath += NewActorName.ToString();

            // haker: note that Actor is stored separate package file for supporting the OFPA(One File Per Actor)
            // Diagram:
            //                                                          
            //  [Level-Actor]                   [External Actor Files]  
            //                        ┌─┐                               
            //                        │ │                               
            //      Level0            │ │                               
            //       │                │ │                               
            //       ├──Actor0 ───────┼─┼───────► External_Actor0       
            //       │                │ │                               
            //       ├──Actor1 ───────┼─┼───────► External_Actor1       
            //       │                │ │                               
            //       └──Actor2 ───────┼─┼───────► External_Actor2       
            //                        │ │                               
            //                        │ │                               
            //                        │ │                               
            //                        │ │                               
            //      Level1            │ │                               
            //       │                │ │                               
            //       ├──Actor3 ───────┼─┼───────► External_Actor3       
            //       │                │ │                               
            //       ├──Actor4 ───────┼─┼───────► External_Actor4       
            //       │                │ │                               
            //       └──Actor5 ───────┼─┼───────► External_Actor5       
            //                        │ │                               
            //                        └─┘                               
            //                 OFPA(One-File-Per-Actor)                 
            //   
            // Why do we need the OFPA?
            // - it can break dependencies between level and actors
            // - by separating actors and levels, multiple users can modify same level at the same time
            // - see the diagram:
            //                                                                                                                                                   
            //  [Previous]                                                              [Current]                                                                
            //                                                                                                                                                   
            //    ┌─Level0──────┐                                                          ┌─Level0──────┐                                                       
            //    │             │                                                          │             │                                                       
            //    │ ┌──────┐    │                                                          │ ┌──────┐    │    ┌───────────────┐                                  
            //    │ │Actor0│ ◄──┼──────────── User0                                        │ │Actor0├────┼────┤External Actor0│◄───────User0                     
            //    │ └──────┘    │                                                          │ └──────┘    │    └───────────────┘                                  
            //    │             │                                                          │             │                                                       
            //    │ ┌──────┐    │                                                          │ ┌──────┐    │    ┌───────────────┐                                  
            //    │ │Actor1│ ◄──┼───[X]────── User1                                        │ │Actor1├────┼────┤External Actor1│◄───────User1                     
            //    │ └──────┘    │    │                                                     │ └──────┘    │    └───────────────┘                                  
            //    │             │    │                                                     │             │                                                       
            //    └─────────────┘    │                                                     └─────────────┘            ***Using OFPA, User0 and User1 can modify  
            //                     User1 can NOT modify Level0                                                                                                   
            //                                                                                                                                                   
            //                                                                                                                                                   
            //                   │                                                                                                                               
            //                   │                                                                                                                               
            //   To modify Actor0 and Actor1 by separate users (User0 and User1)                                                                                 
            //                   │                                                                                                                               
            //                   │                                                                                                                               
            //                   ▼                                                                                                                               
            //                                                                                                                                                   
            //    ┌─Level0──────┐                                                                                                                                
            //    │             │                                                                                                                                
            //    │ ┌──────┐    │                                                                                                                                
            //    │ │Actor0│◄───┼─────────── User0                                                                                                               
            //    │ └──────┘    │                                                                                                                                
            //    │             │                                                                                                                                
            //    └─────────────┘                                                                                                                                
            //                               ***Now User0 and User1 can modify Actor0 and Actor1                                                                 
            //    ┌─Level1──────┐                by separting Actors to Level0 and Level1                                                                        
            //    │             │                                                                                                                                
            //    │ ┌──────┐    │                                                                                                                                
            //    │ │Actor1│◄───┼─────────── User1                                                                                                               
            //    │ └──────┘    │                                                                                                                                
            //    │             │                                                                                                                                
            //    └─────────────┘                                                                                                                                
            //                                                                                                                                                               
            ExternalPackage = ULevel::CreateActorPackage(LevelToSpawnIn->GetPackage(), LevelToSpawnIn->GetActorPackagingScheme(), *ActorPath);
        }
#endif

        FTransform const UserTransform = UserTransformPtr ? *UserTransformPtr : FTransform::Identity;

        // haker: do you remember ESpawnActorCollisionHandlingMethod?
        ESpawnActorCollisionHandlingMethod CollisionHandlingOverride = SpawnParameters.SpawnCollisionHandlingOverride;

        // use override if set, else fall back to actor's preference
        // haker: if CollisionHandlingOverride is 'Undefined', try to use Template's SpawnCollisionHandlingMethod
        ESpawnActorCollisionHandlingMethod const CollisionHandlingMethod = (CollisionHandlingOverride == ESpawnActorCollisionHandlingMethod::Undefined) ? Template->SpawnCollisionHandlingMethod : CollisionHandlingOverride;

        // see if we can avoid spawning altogether by checking native components
        // note: we can't handle all cases here, since we don't know the full component hiearchy until after the actor is spawned
        if (CollisionHandlingMethod == ESpawnActorCollisionHandlingMethod::DontSpawnIfColliding)
        {
            // haker: skip the detail
            //...
        }

        // haker: as we saw it in ExecutConstruction(), with CDO, we use NewObject, not StaticDuplicateObject()
        EObjectFLags ActorFlags = SpawnParameters.ObjectFlags;
        AActor* const Actor = NewObject<AActor>(LevelToSpawnIn, Class, NewActorName, ActorFlags, Template, false/*bCopyTransientsFromClassDefaults*/, nullptr/*InInstanceGraph*/, ExternalPackage);
        check(Actor);
        check(Actor->GetLevel() == LevelToSpawnIn);

        // haker: Actor is belonged to ULevel and also mark it as GCed object by inserting ActorsForGC
        LevelToSpawnIn->Actors.Add(Actor);
        LevelToSpawnIn->ActorsForGC.Add(Actor);

        // tell the actor what method to use, in case it was overriden
        Actor->SpawnCollisionHandlingMethod = CollisionHandlingMethod;

        // haker: see PostSpawnInitialize (goto 003: SpawnActor)
        Actor->PostSpawnInitialize(UserTransform, SpawnParameters.Owner, SpawnParameter.Instigator, SpawnParameters.IsRemoteOwned(), SpawnParameters.bNoFail, SpawnParameters.bDeferConstruction, SpawnParameters.TransformScaleMethod);
    
        // if we are spawning an external actor, mark this package dirty
        if (ExternalPackage)
        {
            ExternalPackage->MarkPackageDirty();
        }

        // broadcast notification of spawn
        OnActorsSpawned.Broadcast(Actor);

        // add this newly spawned actor to the network actor list: 
        // - do this after PostSpawnInitialize so that actor has "finished" spawning
        // see AddNetworkActor (goto 012: SpawnActor)
        AddNetworkActor(Actor);

        return Actor;
    }

    /**
     * mark a component as needing an end of frame update 
     */
    // 004 - Foundation - SceneRenderer * - UWorld::MarkActorComponentForNeededEndOfFrameUpdate
    // haker: this function register a component to do 'EOFUpdate':
    // - what is the benefit of marking the 'needed-end-of-frame-update'?
    //   ┌─[Before]───────────────────────────────────────────────────────────────────────────────────────────────────────────┐
    //   │                                                                                                                    │
    //   │                       ┌──────────┐ ┌──────────┐ ┌──────────┐                                                       │
    //   │                       │Component0│ │Component1│ │Component2│                                                       │
    //   │                       └─┬────────┘ └────┬─────┘ └───┬──────┘                                                       │
    //   │                         │               │           │                                                              │
    //   │  RecreateRenderState    │               │           │                                                              │
    //   │  SendRenderTransform0   └─────────┬─┬─┐ ├─┬─┐ ┌─┬───┘                                                              │
    //   │  SendRenderTransform1             │ │ │ │ │ │ │ │                                                                  │
    //   │                                 ┌─▼─▼─▼─▼─▼─▼─▼─▼┐                                                                 │
    //   │                                 │ Command Queue  │◄─────────Too many RenderCommands are created:                   │
    //   │                                 └─┬─┬─┬─┬─┬─┬─┬─┬┘          *** what if we reduce total count of RenderCommands?   │
    //   │                                   │ │ │ │ │ │ │ │               ────────────────────────────────────────────────   │
    //   │                                   │ │ │ │ │ │ │ │                                                                  │
    //   │                                   ▼ ▼ ▼ ▼ ▼ ▼ ▼ ▼                                                                  │
    //   │                                 ┌────────────────┐                                                                 │
    //   │                                 │  RenderThread  │                                                                 │
    //   │                                 └────────────────┘                                                                 │
    //   │                                                                                                                    │
    //   └────────────────────────────────────────────────────────────────────────────────────────────────────────────────────┘
    //                                                                                                                         
    //                                                                                                                         
    //   ┌─[After]-───────────────────────────────────────────────────────────────────────────────────────────────────────────┐
    //   │                                                                                                                    │
    //   │                       ┌──────────┐ ┌──────────┐ ┌──────────┐                                                       │
    //   │                       │Component0│ │Component1│ │Component2│                                                       │
    //   │                       └────┬─────┘ └────┬─────┘ └────┬─────┘                                                       │
    //   │                            │            │            │                                                             │
    //   │                            │            │            │                                                             │
    //   │                            └────────┐   │   ┌────────┘                                                             │
    //   │                                     │   │   │                                                                      │
    //   │                                 ┌───▼───▼───▼────┐                                                                 │
    //   │                                 │ Command Queue  │◄──────Reduce the count of RenderComands by MarkEndOfFrameUpdate │
    //   │                                 └───┬───┬───┬────┘       *** we can relieve the burden of processing RenderCommands│
    //   │                                     │   │   │                                                                      │
    //   │                                     │   │   │                                                                      │
    //   │                                     │   │   │                                                                      │
    //   │                                 ┌───▼───▼───▼────┐                                                                 │
    //   │                                 │  RenderThread  │                                                                 │
    //   │                                 └────────────────┘                                                                 │
    //   │                                                                                                                    │
    //   └────────────────────────────────────────────────────────────────────────────────────────────────────────────────────┘
    void MarkActorComponentForNeededEndOfFrameUpdate(UActorComponent* Component, bool bForceGameThread)
    {
        // haker: CurrentState is based on 'MarkedForEndOfFrameUpdateState' in UActorComponent
        uint32 CurrentState = Component->GetMarkedForEndOfFrameUpdateState();

        // force game thread can be turned on later, but we are not concerned about that, those are only cvars and constants; if those are changed during a frame, they won't fully kick in till next frame
        // haker: State is changing from Marked to MarkedForGameThread
        // - nullify 'ComponentsThatNeedEndOfFrameUpdate' and reset it to Unmarked
        if (CurrentState == EComponentMarkedForEndOfFrameUpdateState::Marked && bForceGameThread)
        {
            const int32 ArrayIndex = FMarkComponentEndOfFrameUpdateState::GetArrayIndex(Component);
            ComponentsThatNeedEndOfFrameUpdate[ArrayIndex] = nullptr;
            CurrentState = EComponentMarkedForEndOfFrameUpdateState::Unmarked;
        }
        // it is totally ok if it is currently marked for the gamethread but now they are not forcing game thread it will run on the game thread this frame
        
        if (CurrentState == EComponentMarkedForEndOfFrameUpdateState::Unmarked)
        {
            // when there is not rendering thread force all updates on game thread to avoid modifying scene structures from multiple task threads
            // haker: determine whether we force to be GameThread
            bForceGameThread = bForceGameThread || !GIsThreadedRendering || !FApp::ShouldUseThreadingForPerformance();
            if (!bForceGameThread)
            {
                bForceGameThread = !CVarAllowAsyncRenderThreadUpdates.GetValueOnAnyThread();
            }

            // haker: we have two cases:
            // 1. bForceGameThread:
            //    - append 'Component' to 'ComponentsThatNeedEndOfFrameUpdate_OnGameThread'
            //    - mark it as 'MarkedForGameThread'
            // 2. Running in Parllel:
            //    - append 'Component' to 'ComponentsThatNeedEndOfFrameUpdate'
            //    - mark it as 'Marked'
            if (bForceGameThread)
            {
                FMarkComponentEndOfFrameUpdateState::Set(Component, ComponentsThatNeedEndOfFrameUpdate_OnGameThread.Num(), EComponentMarkedForEndOfFrameUpdateState::MarkedForGameThread);
                ComponentsThatNeedEndOfFrameUpdate_OnGameThread.Add(Component);
            }
            else
            {
                FMarkComponentEndOfFrameUpdateState::Set(Component, ComponentsThatNeedEndOfFrameUpdate.Num(), EComponentMarkedForEndOfFrameUpdateState::Marked);
                ComponentsThatNeedEndOfFrameUpdate.Add(Component);
            }

            // if the component might have outstanding tasks when we get to EOF updates, we will need to call the sync function
            // haker: special case to pre-events for EOFUpdate(EndOfFrameUpdate)
            if (Component->RequiresPreEndOfFrameSync())
            {
                FMarkComponentEndOfFrameUpdateState::SetMarkedForPreEndOfFrameSync(Component);
                ComponentsThatNeedPreEndOfFrameSync.Add(Component);
            }
        }
    }

    /** get the count of all PhysicsVolumes in the world that are not a DefaultPhysicsVolume. */
    int32 GetNonDefaultPhysicsVolumeCount() const
    {
        return NonDefaultPhysicsVolumeList.Num();
    }

    /** get an iterator for all PhysicsVolumes in the world that are not a DefaultPhysicsVolume */
    FConstPhysicsVolumeIterator GetNonDefaultPhysicsVolumeIterator() const
    {
        auto Result = NonDefaultPhysicsVolumeList.CreateConstIterator();
        return (const FConstPhysicsVolumeIterator&)Result;
    }

    /**
     * test collision of the supplied component at the supplied location/rotation using a specific channel, and determine the set of components that it overlaps 
     * @note: the overload taking rotation as an FQuat is slightly faster than the version using FRotator (which will be converted to an FQuat)
     */
    bool ComponentOverlapMultiByChannel(TArray<struct FOverlapResult>& OutOverlaps, const class UPrimitiveComponent* PrimComp, const FVector& Pos, const FQuat& Rot,    ECollisionChannel TraceChannel, const FComponentQueryParams& Params = FComponentQueryParams::DefaultComponentQueryParams, const FCollisionObjectQueryParams& ObjectQueryParams=FCollisionObjectQueryParams::DefaultObjectQueryParam) const
    {
        //...
    }

    /** test collision of a shape at the supplied location using specific channel, and determine the set of components that it overlaps */
    bool OverlapMultiByChannel(TArray<struct FOverlapResult>& OutOverlaps, const FVector& Pos, const FQuat& Rot, ECollisionChannel TraceChannel, const FCollisionShape& CollisionShape, const FCollisionQueryParams& Params = FCollisionQueryParams::DefaultQueryParam, const FCollisionResponseParams& ResponseParam = FCollisionResponseParams::DefaultResponseParam) const
    {
        //...
    }

    /**
     * returns the index of the level collection which currently has its context set on this world. may be INDEX_NONE
     * if not INDEX_NONE, this implies that execution is currently within the scope of an FScopeLevelCollectionContextSwitch for this world 
     */
    int32 GetActiveLevelCollectionIndex() { return ActiveLevelCollectionIndex; }

    /**
     * returns the level collection which currently has its context set on this world. may be null.
     * if non-null, this implies that execution is currently within the scope of an FScopedLevelCollectionContextSwitch for this world.
     */
    const FLevelCollection* GetActiveLevelCollection() const
    {
        if (LevelCollections.IsValidIndex(ActiveLevelCollectionIndex))
        {
            return &LevelCollections[ActiveLevelCollectionIndex];
        }
        return nullptr;
    }

    /** sets the level collection and its context on this world. should only be called by FScopedLevelCollectionContextSwitch */
    // 003 - Foundation - TickTaskManager - UWorld::SetActiveLevelCollection
    void SetActiveLevelCollection(int32 LevelCollectionIndex)
    {
        ActiveLevelCollectionIndex = LevelCollectionIndex;

        // see GetActiveLevelCollection()
        const FLevelCollection* const ActiveLevelCollection = GetActiveLevelCollection();

        // haker: if ActiveLevelCollectionIndex is INDEX_NONE, it will have nullptr
        if (ActiveLevelCollection == nullptr)
        {
            return;
        }

        // haker: as we saw last week, all level collection has same persistent level right?
        PersistentLevel = ActiveLevelCollection->GetPersistentLevel();
        if (IsGameWorld())
        {
            SetCurrentLevel(ActiveLevelCollection->GetPersistentLevel());
        }

        // haker: it actually overrides NetDriver and etc.
    }

    /** inserts the passed in controller at the front of the linked list of controllers */
    void AddController(AController* Controller)
    {
        ControllerList.AddUnique(Controller);
        if (APlayerController* PlayerController = Cast<APlayerController>(Controller))
        {
            if (!PlayerController.Contains(PlayerController))
            {
                PlayerControllerList.Add(PlayerController);
            }
        }
    }

    /** start gameplay: this will cause the game mode to transition to the correct state and call BeginPlay on all actors */
    // 063 - Foundation - CreateInnerProcessPIEGameInstance - UWorld::BeginPlay()
    void BeginPlay()
    {
        //...

        // haker: before calling actual BeginPlay(), we notify WorldSubsystems to that the world's BeginPlay() is being called
        const TArray<UWorldSubsystem*>& WorldSubsystems = SubsystemCollection.GetSubsystemArray<UWorldSubsystem>(UWorldSubsystem::StaticClass());
        for (UWorldSubsystem* WorldSubsystem : WorldSubsystems)
        {
            WorldSubsystem->OnWorldBeginPlay(*this);
        }

        // haker: we need to iterator all actors to call BeingPlay() in the world:
        // - is it in the GameMode?...
        // - see AGameModeBase::StartPlay (goto 064: CreateInnerProcessPIEGameInstance)
        AGameModeBase* const GameMode = GetAuthGameMode();
        if (GameMode)
        {
            GameMode->StartPlay();
        }

        // haker: world's BeginPlay() calls are hidden quite deeply:
        // AGameModeBase::StartPlay() -> AGameStateBase::HandleBeginPlay() -> AWorldSettings::NotifyBeginPlay()
        // - remember that world's actual BeginPlay() is called by AWorldSettings::NotifyBeginPlay()~ :)
    }

    /** 
     * return true if Actor would encroach at TestLocation on something that blocks it 
     * returns a ProposedAdjustment that might result in an unblocked TestLocation
     */
    bool EncroachingBlockingGeometry(const AActor* TestActor, FVector TestLocation, FRotator TestRotation, FVector* ProposedAdjustment = NULL)
    {
        if (TestActor == nullptr)
        {
            return false;
        }

        USceneComponent* const RootComponent = TestActor->GetRootComponent();
        if (RootComponent == nullptr)
        {
            return false;
        }

        bool bFoundEncroacher = false;

        FVector TotalAdjustment(0.f);
        FTransform const TestRootToWorld = FTransform(TestRotation, TestLocation);
        FTransform const WorldToOldRoot = RootComponent->GetComponentToWorld().Inverse();

        UMovementComponent* const MoveComponent = TestActor->FindComponentByClass<UMovementComponent>();
        if (MoveComponent && MoveComponent->UpdatedPrimitive)
        {
            // this actor has a movement component, which we interpret to mean that this actor has a primary component being swept around the world
            // , and that component is the only one we care about encroaching (since the movement code will happily embedding other components in the world during movement updates)
            UPrimitiveComponent* const MovedPrimComp = MoveComponent->UpdatedPrimitive;
            if (MovedPrimComp->IsQueryCollisionEnabled())
            {
                // might not be the root, so we need to compute the transform
                FTransform const CompToRoot = MovedPrimComp->GetComponentToWorld() * WorldToOldRoot;
                FTransform const CompToNewWorld = CompToRoot * TestRootToWorld;

                TArray<AActor*> ChildActors;
                TestActor->GetAllChildActors(ChildActors);

                if (ComponentEncroachesBlockingGeometry(this, TestActor, MovedPrimComp, CompToNewWorld, ProposedAdjustment, ChildActors))
                {
                    if (ProposedAdjustment == nullptr)
                    {
                        // don't need an adjustment and we know we are overlapping, so we can be done
                        return true;
                    }
                    else
                    {
                        TotalAdjustment = *ProposedAdjustment;
                    }

                    bFoundEncroacher = true;
                }
            }
        }
        else
        {
            bool bFetchedChildActors = false;
            TArray<AActor*> ChildActors;

            // this actor does not have a movement component, so we'll assume all components are potentially important to keep out of the world
            UPrimitiveComponent* const RootPrimComp = Cast<UPrimitiveComponent>(RootComponent);
            if (RootPrimComp && RootPrimComp->IsQueryCollisionEnabled())
            {
                TestActor->GetAllChildActors(ChildActors);
                bFetchedChildActors = true;

                if (ComponentEncroachesBlockingGeometry(this, TestActor, RootPrimComp, TestRootToWorld, ProposedAdjustment, ChildActors))
                {
                    if (ProposedAdjustment == nullptr)
                    {
                        // don't need an adjustment and we know we are overlapping, so we can be done
                        return true;
                    }
                    else
                    {
                        TotalAdjustment = *ProposedAdjustment;
                    }

                    bFoundEncroacher = true;
                }
            }

            // now test all colliding children for encroachment
            TArray<USceneComponent*> Children;
            RootComponent->GetChildrenComponents(true, Children);

            for (USceneComponent* Child : Children)
            {
                if (Child->IsQueryCollisionEnabled())
                {
                    UPrimitiveComponent* const PrimComp = Cast<UPrimitiveComponent>(Child);
                    if (PrimComp)
                    {
                        FTransform const CompToRoot = Child->GetComponentToWorld() * WorldToOldRoot;
                        FTransform const CompToNewWorld = CompToRoot * TestRootToWorld;
                        if (!bFetchedChildActors)
                        {
                            TestActor->GetAllChildActors(ChildActors);
                            bFetchedChildActors = true;
                        
                        if (ComponentEncroachesBlockingGeometry(this, TestActor, PrimComp, CompToNewWorld, ProposedAdjustment, ChildActors))
                        {
                            if (ProposedAdjustment == nullptr)
                            {
                                // don't need an adjustment and we know we are overlapping, so we can be done
                                return true;
                        
                            TotalAdjustment = CombineAdjustments(TotalAdjustment, *ProposedAdjustment);
                            bFoundEncroacher = true;
                        }
                    }
                }
            }
        }

        // copy over total adjustment
        if (ProposedAdjustment)
        {
            *ProposedAdjustment = TotalAdjustment;
        }

        return bFoundEncroacher;
    }

    /**
     * try to find an acceptable non-colliding locaton to place TestActor as close to possible to PlaceLocation expects PlaceLocation to be a valid location inside the level
     * - returns true if a location without blocking collision is found, in which case PlaceLocation is overwritten with the new clear location
     * - returns false if no suitable location could be found, in which case PlaceLocation is unmodified
     */
    bool FindTeleportSpot(const AActor* TestActor, FVector& PlaceLocation, FRotator PlaceRotation)
    {
        if (!TestActor || !TestActor->GetRootComponent())
        {
            return true;
        }

        FVector Adjust(0.f);
        const FVector OriginalTestLocation = TestLocation;

        // check if fits at desired location
        if (!EncroachingBlockingGeometry(TestActor, TestLocation, TestRotation, &Adjust))
        {
            return true;
        }

        if (Adjust.IsNearlyZero())
        {
            //...
        }

        //...
    }

    /** sets the current GameState instance on this world and the game state's level collection */
    void SetGameState(AGameStateBase* NewGameState)
    {
        if (NewGameState == GameState)
        {
            return;
        }

        GameState = NewGameState;

        //...
    }

    /** send all render updates to the rendering thread */
    // 005 - Foundation - SceneRenderer * - SendAllEndOfFrameUpdates
    // haker: all pending updates stored in ComponentsThatNeedEndOfFrameUpdate[_OnGameThread] are processed in SendAllEndOfFrameUpdates
    // - this function takes the role of updating world's change to scene(render-world):
    //  ┌───────┐                                                                   ┌────────────────────┐  
    //  │ World │                                                                   │ Scene(RenderWorld) │  
    //  └──┬────┘                                                                   └────────────────┬───┘  
    //     │                                                                                         │      
    //     ├──Actor0                                                            PrimitiveSceneInfo0──┤      
    //     │   │                                                                                │    │      
    //     │   └──PrimitiveComponent0 ───────┐            ┌────────────►PrimivitiveSceneProxy0──┘    │      
    //     │                                 │            │                                          │      
    //     │                                 ├─────▲──────┤                                          │      
    //     └──Actor1                         │     │      │                     PrimitiveSceneInfo1──┘      
    //         │                             │     │      │                                     │           
    //         └──PrimitiveComponent1 ───────┘     │      └────────────►PrimivitiveSceneProxy1──┘           
    //                                             │                                                        
    //                                             │                                                        
    //                                             │                                                        
    //                                             │                                                        
    //                           SendAllEndOfFrameUpdate()                                                  
    //                           :Apply any updates on World                                                
    //                            -PrimitiveComponent────►PrimitiveSceneProxy                               
    //                                                    (PrimitiveSceneInfo)                              
    //                                                                                                      
    void SendAllEndOfFrameUpdates()
    {
        //...

        // haker: cache ComponentsThatNeedEndOfFrameUpdate to local variable, 'LocalComponentsThatNeedEndOfFrameUpdate'
        static TArray<UActorComponent*> LocalComponentsThatNeedEndOfFrameUpdate;
        {
            LocalComponentsThatNeedEndOfFrameUpdate.Append(ComponentsThatNeedEndOfFrameUpdate);
        }

        // haker: iterate LocalComponentsThatNeedEndOfFrameUpdate' Components:
        // 1. call DoDeferredRenderUpdates_Concurrent()
        // 2. unmark ActorComponent's state
        auto ParallelWork = [IsUsingParallelNotifyEvents](int32 Index)
        {
            UActorComponent* NextComponent = LocalComponentsThatNeedEndOfFrameUpdate[Index];
            if (NextComponent)
            {
                if (IsValid(NextComponent) && NextComponent->IsRegistered() && !NextComponent->IsTemplate())
                {
                    // haker: call UActorComponent::DoDeferredRenderUpdates_Concurrent (goto 006: SceneRenderer)
                    NextComponent->DoDeferredRenderUpdates_Concurrent();
                }

                // haker: we exeute a necessary 'DoDeferredRenderUpdates_Concurrent()' then we 'unmark' EOFUpdateState
                FMarkComponentEndOfFrameUpdateState::Set(NextComponent, INDEX_NONE, EComponentMarkedForEndOfFrameUpdateState::Unmarked);
            }
        };

        auto GTWork = [this]()
        {
            // to avoid any problems in case of reeentrancy during the deferred update pass, we gather everything and clears the buffers first
            // reentrancy can occur if a render update need to force wait on an async resource and a progress bar ticks tha game-thread during that time
            // haker: I abbreviate the logic as simple as possible:
            // - overall logic is similar to ParallelWork:
            //   - some 'PrimitiveComponents' need to process in GameThread, in that case, we run as GTWork in GameThread
            TArray<UActorComponent*> DeferredUpdates;
            DeferredUpdates.Reserve(ComponentsThatNeedEndOfFrameUpdate_OnGameThread.Num());

            for (UActorComponent* Component : ComponentsThatNeedEndOfFrameUpdate_OnGameThread)
            {
                if (Component)
                {
                    if (Component->IsRegistered() && !Component->IsTemplate() && IsValid(Component))
                    {
                        DeferredUpdates.Add(Component);
                    }

                    FMarkComponentEndOfFrameUpdateState::Set(Component, INDEX_NONE, EComponentMarkedForEndOfFrameUpdateState::Unmarked);
                }
            }

            ComponentsThatNeedEndOfFrameUpdate_OnGameThread.Reset();
            ComponentsThatNeedEndOfFrameUpdate.Reset();

            for (UActorComponent* Component : DeferredUpdates)
            {
                Component->DoDeferredRenderUpdates_Concurrent();
            }
        };

        // haker: GTWork and ParallelWork is expected to be done in the following order:
        //                                                             
        //  ┌─────────┐     ┌──────────────┐                           
        //  │ GTWork  ├────►│ ParallelWork │                           
        //  └─────────┘     └──┬───────────┘                           
        //                     │                ─┐                     
        //                     ├──Work[Index0]   │                     
        //                     │                 │                     
        //                     ├──Work[Index1]   │Running in Parallel  
        //                     │                 │                     
        //                     ├──Work[Index2]   │                     
        //                     │                 │                     
        //                     └──...           ─┘                     
        //                                                             
        ParallelForWithPreWork(LocalComponentsThatNeedEndOfFrameUpdate.Num(), ParallelWork, GTWork);

        LocalComponentsThatNeedEndOfFrameUpdate.Reset();
    }

    // 009 - Foundation - CreateWorld ** - UWorld's member variables

    /** the URL that was used when loading this World */
    // haker: think of the URL as package path:
    // - e.g. Game\Map\Seoul\Seoul.umap
    FURL URL;

    /** the current ticking group */
    TEnumAsByte<ETickingGroup> TickGroup;

    /** the type of world this is. Describes the context in which it is being used (Editor, Game, Preview etc.) */
    // haker: we already seen EWorldType
    // - TEnumAsByte is helper wrapper class to support bit operation on enum type
    // - I recommend to read it how it is implemented
    // ADVICE: as C++ programmer, it is VERY important **to manipulate bit operations freely!**
    TEnumAsByte<EWorldType::Type> WorldType;

    /** data structure for holding the tick functions that are associated with the world (line batcher, etc) */
    FTickTaskLevel* TickTaskLevel;

    /** persistent level containing the world info, default brush and actors pawned during gameplay among other things */
    // see ULevel (goto 10)
    // hacker: short explanation about world info (== WorldSettings)
    TObjectPtr<class ULevel> PersistentLevel;

#if WITH_EDITORONLY_DATA || 1
    /** pointer to the current level being edited; level has to be in the levels array and == PersistentLevel in the game */
    TObjectPtr<class ULevel> CurrentLevel;
#endif

    /** array of levels currently in this world; NOT serialized to disk to avoid hard references */
    // haker: for now, skip how UWorld contains sub-levels
    UPROPERTY(Transient)
    TArray<TObjectPtr<class ULevel>> Levels;

    /** array of level collections currently in this world */
    // haker: UWorld has the classified collections of level we have covered in ULevel
    TArray<FLevelCollection> LevelCollections;

    /** index of the level collection that's currently ticking */
    int32 ActiveLevelCollectionIndex;

    /** DefaultPhysicsVolume used for whole game */
    // haker: you can think of physics volume as 3d range of physics engine works (== physics world covers)
    TObjectPtr<APhysicsVolume> DefaultPhysicsVolume;

    /** physics scene for this world */
    FPhysScene* PhysicsScene;

    /** the interface to the scene manager for this world */
    // haker: FScene
    class FSceneInterface* Scene;

    // see UWorldSubsystem (goto 21)
    FObjectSubsystemCollection<UWorldSubsystem> SubsystemCollection;

    /** line batchers: */
    // haker: debug lines
    // - ULineBatchComponents are resided in UWorld's subobjects
    TObjectPtr<class ULineBatchComponent> LineBatcher;
    TObjectPtr<class ULineBatchComponent> PersistentLineBatcher;
    TObjectPtr<class ULineBatchComponent> ForegroundLineBatcher;

    // haker: let's wrap up what we have looked through classes:
    //                                                                                ┌───WorldSubsystem0       
    //                                                        ┌────────────────────┐  │                         
    //                                                 World──┤SubsystemCollections├──┼───WorldSubsystem1       
    //                                                   │    └────────────────────┘  │                         
    //                                                   │                            └───WorldSubsystem2       
    //             ┌─────────────────────────────────────┴────┐                                                 
    //             │                                          │                                                 
    //           Level0                                     Level1                                              
    //             │                                          │                                                 
    //         ┌───┴────┐                                 ┌───┴────┐                                            
    //         │ Actor0 ├────Component0(RootComponent)    │ Actor0 ├─────Component0(RootComponent)              
    //         ├────────┤     │                           ├────────┤      │                                     
    //         │ Actor1 │     ├─Component1                │ Actor1 │      │   ┌──────┐                          
    //         ├────────┤     │                           ├────────┤      └───┤Actor2├──RootComponent           
    //         │ Actor2 │     └─Component2                │ Actor2 │          └──────┘   │                      
    //         ├────────┤                                 ├────────┤                     ├──Component0          
    //         │ Actor3 │                                 │ Actor3 │                     │                      
    //         └────────┘                                 └────────┘                     ├──Component1          
    //                                                                                   │   │                  
    //                                                                                   │   └──Component2      
    //                                                                                   │                      
    //                                                                                   └──Component3          
    // search 'goto 4'

    TObjectPtr<class UGameInstance> OwningGameInstance;

    /** whether actors have been initialized for play */
    uint8 bActorsInitialized : 1;

    /**
     * if true, this world will tick physics to simulate. this isn't same as having physics scene
     * you need physics scene if you'd like to trace. this flag changed ticking 
     */
    uint8 bShouldSimulatePhysics : 1;

    /** whether BeginPlay has been called on actors */
    uint8 bBegunWorld : 1;

    /** array of components that need to wait on tasks before end of frame updates */
    TSet<TObjectPtr<UActorComponent>> ComponentsThatNeedPreEndOfFrameSync;

    /** array of components that need updates at the end of the frame */
    TArray<TObjectPtr<UActorComponent>> ComponentsThatNeedEndOfFrameUpdate;

    /** array of components that need game thread updates at the end of the frame */
    TArray<TObjectPtr<UActorComponent>> ComponentsThatNeedEndOfFrameUpdate_OnGameThread;

    /** list of all physics volumes in the world: does NOT include the DefaultPhysicsVolume */
    TArray<TWeakObjectPtr<APhysicsVolume>> NonDefaultPhysicsVolumeList;

    // 005 - Foundation - TickTaskManager
    // see briefly FStartPhysicsTickFunction and FEndPhysicsTickFunction

    /** tick function for starting physics */
    FStartPhysicsTickFunction StartPhysicsTickFunction;
    /** tick function for ending physics */
    FEndPhysicsTickFunction EndPhysicsTickFunction;

    /** list of all the controllers in the world */
    TArray<TWeakObjectPtr<class AController>> ControllerList;

    /** list of all the player controllers in the world */
    TArray<TWeakObjectPtr<class APlayerController>> PlayerControllerList;

    /** list of all the cameras in the world that acto-activate for players */
    TArray<TWeakObjectPtr<ACameraActor>> AutoCameraActorList;

    /** the replicated actor which contains game state information that can be accessible to clients: direct access is not allowed, use GetGameState<>() */
    TObjectPtr<class AGameStateBase> GameState;

    /** the current GameMode, valid only on the server */
    // haker: standalone game is client as well as server
    TObjectPtr<class AGameModeBase> AuthorityGameMode;
};