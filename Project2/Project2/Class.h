/** base class of reflection data objects */
class UField : public UObject
{

};

/** base class for all UObject types that contain fields */
class UStruct : public UField
{

};

/**
 * flags describing a class 
 */
enum EClassFlags
{
    /** class is abstract and can't be instantiated directly */
    CLASS_Abstract            = 0x00000001u, 
};

class UClass : public UStruct
{
    /** class flags: see EClassFlags for more information */
};