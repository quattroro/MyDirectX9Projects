/** struct containing a collection of optional parameters for initialization of a world */
// 3 - Foundation - CreateWorld - FWorldInitializationValues
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
// 27 - Foundation - CreateWorld - ESpawnActorCollisionHandlingMethod:
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
    //...
};

/** struct of optional parameters passed to SpawnActor function(s) */
// 26 - Foundation - CreateWorld - FActorSpawnParameters
// haker: I reduce its member variables drastically
struct FActorSpawnParameters
{
    /** a name to assign as the Name of the Actor being spawned; if no value is specified, the name of the spawned Actor will be automatically generated using the form [Class]_[Number] */
    // haker: default format of FName is [ClassName]_[Number]:
    // - e.g. WorldSettings_0
    FName Name;

    /** method for resolving collisions at the spawn point; undefined means no override, use the actor's setting */
    // see ESpawnActorCollisionHandlingMethod (goto 27)
    ESpawnActorCollisionHandlingMethod SpawnCollisionHandlingOverride;

    /** determines whether or not the actor may be spawned when running a construction script; if true spawning will fail if a construction script is being run */
    uint8 bAllowDuringConstructionScript : 1;
};

/** globals without thread protection must be accessed only from GameThread */
static TArray<FSubsystemCollectionBase*> GlobalSubsystemCollections;

// 23 - Foundation - CreateWorld - FSubsystemCollectionBase
// see FSubsystemCollectionBase's member variables (goto 24)
class FSubsystemCollectionBase
{
    FSubsystemCollectionBase(UClass* InBaseType)
        : Outer(nullptr)
        , BaseType(InBaseType)
    {}

    // 31 - Foundation - CreateWorld - FSubsystemCollectionBase::AddAndInitializeSubsystem
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
    // 30 - Foundation - CreateWorld - FSubsystemCollectionBase::Initialize
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

    // 24 - Foundation - CreateWorld - FSubsystemCollectionBase
    // haker:
    // if we are dealing with UWorldSusystem:
    // - Outer is UWorld
    // - BaseType is UWorldSubsystem
    UObject* Outer;
    UClass* BaseType;

    // haker: mapper from UClass(Subsystem's UClass) to UObject(Subsystem's instance)
    // - at initialization time of FObjectSubsystemCollection<SubsystemType>, one-to-one mappings are supported 
    TMap<TObjectPtr<UClass>, TObjectPtr<USubsystem>> SubsystemMap;

    // haker: from my guess, to support UDynamicSubsystem, it persist multiple instances for each USubsystem's class type
    mutable TMap<UClass*, TArray<USubsystem*>> SubsystemArrayMap;
};

/** subsystem collection which delegates UObject references to its owning UObject (object needs to implement AddReferencedObjects and forward call to Collection) */
// 22 - Foundation - CreateWorld - FObjectSubsystemCollection
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
// 21 - Foundation - CreateWorld - USubsystem
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

// 내가 만드는 시스템이 다른 시스템과 같은 라이프타임을 가지게 하고 싶을때 USubsystem을 상속받도록 작업하면 된다. 라이프타임을 관리할 필요가 없어진다.
// USubsystem을 상속받는 클래스를 찾아서 분석해보기
// 
class USubsystem : public UObject
{
    /**
     * override to control if the Subsystem should be created
     * for example you could only have your system created on servers
     * it is important to note that if using this is becomes very important to null check whenever getting the Subsystem
     * 
     * NOTE: this function is called on the CDO prior to instances being created!!! 
     */
    // 32 - Foundation - CreateWorld - ShouldCreateSubsystem
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
// 21 - Foundation - CreateWorld - UWorldSubsystem
// see USubsystem (goto 22)
class UWorldSubsystem : public USubsystem
{
    /** called after world components (e.g. line batcher and all level components) have been updated */
    virtual void OnWorldComponentsUpdated(UWorld& World) {}
};

/** indicates the type of a level collection, used in FLevelCollection */
// 20 - Foundation - CreateWorld - ELevelCollectionType
// haker: don't sticking to its types, we are going to understand it simply like dynamic level vs. static level
enum class ELevelCollectionType : uint8
{
    /**
     * the dynamic levels that are used for normal gameplay and the source for any duplicated collections
     * will contain a world's persistent level and any streaming levels that contain dynamic or replicated gameplay actors
     */
     /**
     * 일반적인 게임 플레이에 사용되는 동적 레벨과 중복된 컬렉션의 출처
     * 세계의 지속적인 레벨과 동적 또는 복제된 게임 플레이 배우를 포함하는 모든 스트리밍 레벨을 포함할 것입니다
     */
    DynamicSourceLevels,

    /** gameplay relevant levels that have been duplicated from DynamicSourceLevels if requested by the game */
    DynamicDuplicatedLevels,

    /**
     * these levels are shared between the source levels and the duplicated levels, and should contain
     * only static geometry and other visuals that are not replicated or affected by gameplay
     * thsese will not be duplicated in order to save memory 
     */
     /**
     * 이러한 레벨은 소스 레벨과 중복된 레벨 간에 공유되며 다음을 포함해야 합니다
     * 복제되거나 게임 플레이에 영향을 받지 않는 정적 지오메트리 및 기타 시각 자료만 제공됩니다
     * 메모리를 절약하기 위해 이 항목들은 중복되지 않습니다
     */
    //레벨이 변하지 않는것 (맵 등등)
    StaticLevels,

    MAX
};

/**
 * contains a group of levels of a particular ELevelCollectionType within a UWorld
 * and the context required to properly tick/update those levels; this object is move-only
 */
// 19 - Foundation - CreateWorld - FLevelCollection
// haker: FLevelCollection is collection based on ELevelCollectionType

//레벨의 CollectionType을 정해준다.(레벨들은 해당 CollectionType에 따라서 분류된다.) 그로인해 파티클들믄 들어있는 레벨, 맵만 들어있는 레벨 등등의 엑터의 분류가 가능해진다.
struct FLevelCollection
{
    /** the type of this collection */
    // see ELevelCollectionType (goto 20)
    ELevelCollectionType CollectionType;

    /**
     * the persistent level associated with this collection
     * the source collection and the duplicated collection will have their own instances 
     */
    // haker: usually OwnerWorld's PersistentLevel
    /**
    * 이 컬렉션과 관련된 지속적인 수준
    * 소스 컬렉션과 복제된 컬렉션에는 고유한 인스턴스가 있습니다
    */
    // 하커: 보통 OwnerWorld의 지속적인 레벨
    TObjectPtr<class ULevel> PersistentLevel;

    /** all the levels in this collection */
    TSet<TObjectPtr<ULevel>> Levels;
};

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
// 4 - Foundation - CreateWorld - UWorld class
// see UObject
// haker: now see World's member variables (goto 9)
/**
* 세계는 행위자와 구성 요소가 존재하고 렌더링될 지도나 샌드박스를 나타내는 최상위 객체입니다
*
* 볼륨 및 청사진 함수를 통해 로드 및 언로드되는 스트리밍 레벨 목록을 선택할 수 있는 하나의 영구 레벨이 될 수 있습니다
* 또는 월드 컴포지션으로 구성된 레벨 모음일 수도 있습니다(->haker: Old Comment...)
*
* 독립형 게임에서는 일반적으로 목적지와 현재 세계가 모두 존재하는 원활한 영역 전환을 제외하고는 단일 세계만 존재합니다
* 편집자에게는 많은 세계가 존재합니다:
* - 편집 중인 레벨
* - 각 PIE 인스턴스
* - 대화형 렌더링 뷰포트가 있는 각 편집기 도구 등
*/
// 4 - 재단 - 크리에이트월드 - UWorld 클래스
// see UObject
// 하커: 이제 월드의 멤버 변수를 확인하세요 (9로 이동)
class UWorld final : public UObject, public FNetworkNotify
{
    // 2 - Foundation - CreateWorld - UWorld::InitializationValues
    // see FWorldInitializationValues
    using InitializationValues = FWorldInitializationValues;

    /** static function that creates a new UWorld and returns a pointer to it */
    // 1 - Foundation - CreateWorld - UWorld::CreateWorld
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
        //   - OuterPrivate infers where object is resides in
        //   - normally OuterPrivate is set as UPackage: 
        //     - where object is resides in == what file object resides in
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
        // 패키지에 월드가 포함된 것으로 표시
        // 하커: 언리얼 엔진에서 'Transient'이란 무엇인가요?
        // - 직렬화되지 않은 자산이나 자산이 있는 경우 'Transient'으로 표시합니다
        // - Transient == (serial화되지 않도록 표시하기 위한 데이터 meta)
        if (WorldPackage != GetTransientPackage())
        {
            WorldPackage->ThisContainsMap();
        }

        const FString WorldNameString = (WorldName != NAME_None) ? WorldName.ToString() : TEXT("Untitled");

        // see UWorld class (goto 4)

        // haker: we set NewWorld's outer as WorldPackage:
        // - normally, when you look into outer object, it will finally end-up-with package(asset file containing this UObject)
        // 하커: 우리는 NewWorld의 외부를 WorldPackage로 설정했습니다:
        // - 일반적으로 외부 객체를 들여다보면 결국 패키지(이 UObject가 포함된 자산 파일)로 마무리됩니다
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
    }

    /** initialize all world subsystems */
    // 29 - Foundation - CreateWorld - UWorld::InitializeSubsystems
    void InitializeSubsystems()
    {
        // see FSubsystemCollectionBase::Initialize (goto 30)
        SubsystemCollection.Initialize(this);
    }

    /** finalize initialization of all world subsystems */
    // 37 - Foundation - CreateWorld - UWorld::PostInitializeSubsystems
    void PostInitializeSubsystems()
    {
        const TArray<UWorldSubsystem*>& WorldSubsystems = SubsystemCollection.GetSubsystemArray<UWorldSubsystem>(UWorldSubsystem::StaticClass());
        for (UWorldSubsystem* WorldSubsystem : WorldSubsystems)
        {
            WorldSubsystem->PostInitialize();
        }
    }

    /** returns the AWorldSettings actor associated with this world */
    // 33 - Foundation - CreateWorld - UWorld::GetWorldSettings
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

    // 35 - Foundation - CreateWorld - UWorld::InternalGetDefaultPhysicsVolume
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
    // 34 - Foundation - CreateWorld - UWorld::GetDefaultPhysicsVolume
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
    // 36 - Foundation - CreateWorld - UWorld::ConditionallyCreateDefaultLevelCollections
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
    // 28 - Foundation - CreateWorld - UWorld::InitWorld
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
                // haker: we skip the detail of phsics scene
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
    // 38 - Foundation - CreateWorld - UWorld::UpdateWorldComponents
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

        // 66 - Foundation - CreateWorld - 2-week
        {
            for (int32 LevelIndex = 0; LevelIndex < Levels.Num(); ++LevelIndex)
            {
                ULevel* Level = Levels[LevelIndex];
                ULevelStreaming* StreamingLevel = FLevelUtils::FindStreamingLevel(Level);
                
                // update the level only if it is visible (or not a streamed level)
                if (!StreamingLevel || Level->bIsVisible)
                {
                    Level->UpdateLevelComponents(bRerunConstructionScripts, Context);
                    IStreamingManager::Get().AddLevel(Level);
                }
            }
        }

        const TArray<UWorldSubsystem*>& WorldSubsystems = SubsystemCollection.GetSubsystemArray<UWorldSubsystem>(UWorldSubsystem::StaticClass());
        for (UWorldSubsystem* WorldSubsystem : WorldSubsystems)
        {
            WorldSubsystem->OnWorldComponentsUpdated(*this);
        }
    }

    /** initializes a newly created world */
    // 25 - Foundation - CreateWorld - UWorld::InitializeNewWorld
    void InitializeNewWorld(const InitializationValues IVS = InitializationValues(), bool bInSkipInitWorld = false)
    {
        if (!IVS.bTransactional)
        {
            // haker: clear ObjectFlags in UObjectBase (bit-wise operation)
            ClearFlags(RF_Transactional);
        }

        // haker: create default persistent level for new world
        PersistentLevel = NewObject<ULevel>(this, TEXT("PersistentLevel"));
        PersistentLevel->OwningWorld = this;

#if WITH_EDITORONLY_DATA || 1
        CurrentLevel = PersistentLevel;
#endif

        // create the WorldInfo actor
        // haker: we are NOT looking into AWorldSettings in detail, but try to understand it with the example
        // [ ] explain AWorldSettings in the editor
        AWorldSettings* WorldSettings = nullptr;
        {
            // haker: when you spawn an Actor, you need to pass FActorSpawnParameters
            // see FActorSpawnParameters (goto 26)
            FActorSpawnParameters SpawnInfo;
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

    // 9 - Foundation - CreateWorld - UWorld's member variables

    /** the URL that was used when loading this World */
    // haker: think of the URL as package path:
    // - e.g. Game\Map\Seoul\Seoul.umap

    /** 이 월드를 로드할 때 사용된 URL */
    // 하커: URL을 패키지 경로로 생각해 보세요:
    // - 예: 게임\\Map\\Seoul.umap
    FURL URL;

    /** the type of world this is. Describes the context in which it is being used (Editor, Game, Preview etc.) */
    // haker: we already seen EWorldType
    // - TEnumAsByte is helper wrapper class to support bit operation on enum type
    // - I recommend to read it how it is implemented
    // ADVICE: as C++ programmer, it is VERY important **to manipulate bit operations freely!**
    /** 이 세계의 유형입니다. 이 세계가 사용되고 있는 맥락을 설명합니다 (편집자, 게임, 미리보기 등) */
    // 하커: 우리는 이미 EWorldType를 보았습니다
    // - TEnumAsByte는 enum 타입에서 비트 연산을 지원하는 헬퍼 래퍼 클래스입니다
    // - 구현 방식을 읽어보시기를 권장합니다
    // 조언: C++ 프로그래머로서 비트 연산을 자유롭게 조작하는 것은 매우 중요합니다!**!
    TEnumAsByte<EWorldType::Type> WorldType;

    /** persistent level containing the world info, default brush and actors pawned during gameplay among other things */
    // see ULevel (goto 10)
    // hacker: short explanation about world info
    TObjectPtr<class ULevel> PersistentLevel; // world info 정보를 가지고있는 level이 persistentlevel 이고 가지고있지 않은 레벨은 썸 레벨이다.

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
    // 피직스 월드에서 일정 범위에만 피직스를 적용하고 싶은 경우를 위해서 존재
    TObjectPtr<APhysicsVolume> DefaultPhysicsVolume;

    /** physics scene for this world */
    FPhysScene* PhysicsScene;

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
};