/** flags describing an object instance */
// 8 - Foundation - CreateWorld - EObjectFlags
// [ ] see the unreal code
enum EObjectFlags
{
    RF_Transactional			=0x00000008,	///< Object is transactional. (객체는 트랜잭션입니다.)
};

/** low level implementation of UObject, should not be used directly in game code */
// 7 - Foundation - CreateWorld - UObjectBase
// haker: this class is the most base class for UObject
// - look through its member variables
class UObjectBase
{
    /**
     * Flags used to track and report various object states
     * this needs to be 8 byte aligned on 32-bit platforms to reduce memory waste 
     */
    // haker: bit flags to define UObject's behavior or attribute as meta-data format
    // see EObjectFlags
    /**
    * 다양한 객체 상태를 추적하고 보고하는 데 사용되는 플래그
    * 메모리 낭비를 줄이기 위해 32비트 플랫폼에서 8바이트 정렬이 필요합니다
    */
    // haker: 비트 플래그를 통해 UObject의 동작 또는 속성을 메타 데이터 형식으로 정의합니다
    EObjectFlags ObjectFlags;

    /** object this object resides in */
    // haker: as we said previously, it is written as UPackage
    // - note that as times went by, the unreal supports lots of features to support reduce dependency on assets:
    //   - OFPA (One File Per Actor) is one of representative example
    //   - in the past, AActor resides in ULevel and its UPackage is just level asset file, which is straight-forward
    //   - but, after introducing OFPA, an indirection is added, no more each AActor is stored in ULevel file, it is stored in separate file Exteral path
    // - I'd like to say that overall pattern is maintained, but as engine evolves, it adds indirection and complexity to understand its actual behavior
    // - anyway for now, you just try to understand OuterPrivate will be set as UPackage normally, it is enought for now!

    /** 이 객체는 에 위치합니다*/
        // 하커: 앞서 말했듯이 UPackage라고 적혀 있습니다
        // - 시간이 지남에 따라 언리얼은 자산 의존도를 낮추기 위해 많은 기능을 지원한다는 점에 유의하세요:
        // - OFPA(행위자당 하나의 파일)는 대표적인 예 중 하나입니다
        // - 과거에 AActor는 ULevel에 위치해 있으며, UPackage는 단순한 레벨 자산 파일로, 간단합니다
        // - 그러나 OFPA를 도입한 후, 각 AActor가 더 이상 ULevel 파일에 저장되지 않고 별도의 파일에 저장됩니다. 외부 경로
        // - 전반적인 패턴은 유지되지만 엔진이 발전함에 따라 실제 동작을 이해하기 위해 간접적이고 복잡성이 더해진다고 말씀드리고 싶습니다
        // - 어쨌든 지금은 OuterPrivate가 정상적으로 UPackage로 설정될 것이라는 점을 이해하려고 노력하시면 됩니다. 지금은 충분합니다!
    UObject* OuterPrivate; // 해당 오브젝트가 어떤 파일에 저장되어있는지의 정보를 담고있는 프로퍼티 이다.
};

/** provides utility function for UObject, this class should not be used directly */
// 6 - Foundation - CreateWorld - UObjectBaseUtility
// see UObjectBase
// haker: later we'll cover UObjectBaseUtility's member functions below
class UObjectBaseUtility : public UObjectBase
{
    UObject* GetTypedOuter(UClass* Target) const
    {
        UObject* Result = NULL;
        for (UObject* NextOuter = GetOuter(); Result == NULL && NextOuter != NULL; NextOuter = NextOuter->GetOuter())
        {
            // haker: we are not getting into IsA(), which is out-of-scope cuz it is related to Reflection System
            if (NextOuter->IsA(Target))
            {
                Result = NextOuter;
            }
        }
        return Result;
    }

    /** traverses the outer chain searching for the next object of a certain type (T must be derived from UObject) */
    template <typename T>
    T* GetTypedOuter() const
    {
        return (T*)GetTypedOuter(T::StaticClass());
    }

    /** determine whether this object is a template object */
    // 54 - Foundation - CreateWorld - UObjectBaseUtility::IsTemplate()
    // haker: 
    // - I have been look through the unreal engine source code for long time, but I still can't explain what is archetype object with specific example
    // - you just think of it as CDO, class default object
    //   - for CDO, what I understand is like **initialization list** as default UObject instance
    bool IsTemplate(EObjectFlags TemplateTypes = RF_ArchetypeObject|RF_ClassDefaultObject) const
    {
        // haker: note that if one of outer is template, the object is template
        for (const UObjectBaseUtility* TestOuter = this; TestOuter; TestOuter = TestOuter->GetOuter())
        {
            if (TestOuter->HasAnyFlags(TemplateTypes))
                return true;
        }
        return false;
    }
};

/**
 * the base class of all UE objects. the type of an object is defined by its UClass
 * this provides support functions for creating and using objects, and virtual functions that should be overriden in child classes
 */
// 5 - Foundation - CreateWorld - UObject
// see UObjectBaseUtility
class UObject : public UObjectBaseUtility
{
    
};