
// 005 - Foundation - AttachToComponent - FUObjectThreadContext
class FUObjectThreadContext : public TThreadSingleton<FUObjectThreadContext>
{
    /** Global flag so that FObjectFinders know if they are called from inside the UObject constructors or not */
    // haker: using TLS(thread-local storage), we can acknowledge whether we are in the constructor
    int32 IsInConstructor;
};

/** flags describing an object instance */
// 008 - Foundation - CreateWorld ** - EObjectFlags
// [*] see the unreal code
enum EObjectFlags
{
    RF_Standalone               =0x00000002,	///< Keep object around for editing even if unreferenced.
    RF_Transactional			=0x00000008,	///< Object is transactional.
};

/** low level implementation of UObject, should not be used directly in game code */
// 007 - Foundation - CreateWorld ** - UObjectBase
// haker: this class is the most base class for UObject
// - look through its member variables
class UObjectBase
{
    /** returns the unique ID of the object... these are reused so it is only unique while the object is alive */
    uint32 GetUniqueID() const
    {
        return (uint32)InternalIndex;
    }
    
    /**
     * Flags used to track and report various object states
     * this needs to be 8 byte aligned on 32-bit platforms to reduce memory waste 
     */
    // haker: bit flags to define UObject's behavior or attribute as meta-data format
    // see EObjectFlags
    EObjectFlags ObjectFlags;

    /** object this object resides in */
    // haker: as we said previously, it is written as UPackage
    // - note that as times went by, the unreal supports lots of features to support reduce dependency on assets:
    //   - OFPA (One File Per Actor) is one of representative example
    //   - in the past, AActor resides in ULevel and its UPackage is just level asset file, which is straight-forward
    //   - but, after introducing OFPA, an indirection is added, no more each AActor is stored in ULevel file, it is stored in separate file Exteral path
    // - I'd like to say that overall pattern is maintained, but as engine evolves, it adds indirection and complexity to understand its actual behavior
    // - anyway for now, you just try to understand OuterPrivate will be set as UPackage normally, it is enought for now!
    UObject* OuterPrivate;

    /** index into GObjectArray... very private */
    int32 InternalIndex;
};

/** provides utility function for UObject, this class should not be used directly */
// 006 - Foundation - CreateWorld ** - UObjectBaseUtility
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
    // 054 - Foundation - CreateWorld * - UObjectBaseUtility::IsTemplate()
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

    /** adds this objects to a GC cluster that already exists */
    void AddToCluster(UObjectBaseUtility* ClusterRootOrObjectFromCluster, bool bAddAsMutableObject = false)
    {
        //...
    }

    void MarkPendingKillOnlyInternal()
    {
        AtomicallySetFlags(RF_InternalPendingKill);
        GUObjectArray.IndexToObject(InternalIndex)->SetPendingKill();
    }
    void ClearPendingKillOnlyInternal()
    {
        AtomicallyClearFlags(RF_InternalPendingKill);
        GUObjectArray.IndexToObject(InternalIndex)->ClearPendingKill();
    }
    void MarkAsGarbageOnlyInternal()
    {
        AtomicallySetFlags(RF_InternalGarbage);
        GUObjectArray.IndexToObject(InternalIndex)->ThisThreadAtomicallySetFlag(EInternalObjectFlags::Garbage);
    }
    void ClearGarbageOnlyInternal()
    {
        AtomicallyClearFlags(RF_InternalGarbage);
        GUObjectArray.IndexToObject(InternalIndex)->ThisThreadAtomicallyClearedFlag(EInternalObjectFlags::Garbage);
    }

    /** Marks this object as Garbage */
	void MarkAsGarbage()
	{
		check(!IsRooted());
		if (bPendingKillDisabled)
		{
			MarkAsGarbageOnlyInternal();
		}
		else
		{
			MarkPendingKillOnlyInternal();
		}
	}

	/** Unmarks this object as Garbage. */
	void ClearGarbage()
	{
		if (bPendingKillDisabled)
		{
			ClearGarbageOnlyInternal();
		}
		else
		{
			ClearPendingKillOnlyInternal();
		}
	}
};

/**
 * the base class of all UE objects. the type of an object is defined by its UClass
 * this provides support functions for creating and using objects, and virtual functions that should be overriden in child classes
 */
// 005 - Foundation - CreateWorld ** - UObject
// see UObjectBaseUtility
class UObject : public UObjectBaseUtility
{
    
};