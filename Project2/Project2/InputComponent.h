
enum EInputEvent : int
{
	IE_Pressed              =0,
	IE_Released             =1,
	IE_Repeat               =2,
	IE_DoubleClick          =3,
	IE_Axis                 =4,
	IE_MAX                  =5,
};

/** an input chord is a key and the modifier keys that are to be held with it */
struct FInputChord
{
    /** the key is the core of the chord */
    FKey Key;

    uint32 bShift:1;
    uint32 bCtrl:1;
    uint32 bAlt:1;
    uint32 bCmd:1;
};

/** binds a delegate to a key chord */
struct FInputKeyBinding : public FInputBinding
{
    /** key event to bind it to (e.g. pressed, released, double click) */
    EInputEvent KeyEvent;

    /** Input Chord to bind to */
    FInputChord Chord;

    /** The delegate bound to the key chord */
    FInputActionUnifiedDelegate KeyDelegate;
};

/**
 * implement an Actor component for input bindings
 * 
 * An input component is a transient component that enables an Actor to bind various forms of input events to delegate functions
 * Input components are processed from a stack managed by the PlayerController and processed by the PlayerInput
 * Each binding can consume the input event preventing other components on the input stack from processing the input 
 */
class UInputComponent : public UActorComponent
{
    void UInputComponent::ClearBindingValues()
    {
        for (FInputAxisBinding& AxisBinding : AxisBindings)
        {
            AxisBinding.AxisValue = 0.f;
        }

        //...
    }

    /** the collection of key bindings */
    TArray<FInputKeyBinding> KeyBindings;

    /** The collection of axis bindings. */
    TArray<FInputAxisBinding> AxisBindings;
};