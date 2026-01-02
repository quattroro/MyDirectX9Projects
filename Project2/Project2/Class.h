/** base class of reflection data objects */
class UField : public UObject
{

};

/** base class for all UObject types that contain fields */
class UStruct : public UField
{
    /** struct this inherits from, may be null */
    UStruct* GetSuperStruct() const
    {
        return SuperStruct;
    }

    bool IsChildOf(const UStruct* SomeBase) const
    {
        bool bOldResult = false;
        for (const UStruct* TempStruct=this; TempStruct; TempStruct=TempStruct->GetSuperStruct())
        {
            if (TempStruct == SomeBase)
            {
                bOldResult = true;
                break;
            }
        }
        return bOldResult;
    }

    TObjectPtr<UStruct> SuperStruct;
};

/**
 * flags describing a class 
 */
enum EClassFlags
{
    /** class is abstract and can't be instantiated directly */
    CLASS_Abstract            = 0x00000001u, 
};

/** information about an interface a class implements */
struct FImplementedInterface
{
    /** the interface class */
    TObjectPtr<UClass> Class;
    /** the pointer offset of the interface's vtable */
    int32 PointerOffset;
    /** whether or not this interface has been implemented via K2 */
    bool bImplementedByK2;
};

class UClass : public UStruct
{
    /** returns parent class, the parent of a Class is always another class */
    UClass* GetSuperClass() const
    {
        return (UClass*)GetSuperStruct();
    }

    /** this will return whether or not this class implements the passed in class/interface */
    bool ImplementsInterface(const class UClass* SomeInterface) const
    {
        if (SomeInterface != NULL && SomeInterface->HasAnyClassFlags(CLASS_Interface) && SomeInterface != UInterface::StaticClass())
        {
            for (const UClass* CurrentClass = this; CurrentClass; CurrentClass = CurrentClass->GetSuperClass())
            {
                // SomeInterface might be a base interface of our implemented interface
                for (TArray<FImplementedInterface>::TConstIterator It(CurrentClass->Interfaces); It; ++It)
                {
                    const UClass* InterfaceClass = It->Class;
                    if (InterfaceClass && InterfaceClass->IsChildOf(SomeInterface))
                    {
                        return true;
                    }
                }
            }
        }
    }

    /** class flags: see EClassFlags for more information */
    EClassFlags ClassFlags;

    /**
     * the list of interfaces which this class implements, along with the pointer property that is located at the offset of the interface's vtable
     * if the interface class isn't native, the property will be null
     */
    TArray<FImplementedInterface> Interfaces;
};
