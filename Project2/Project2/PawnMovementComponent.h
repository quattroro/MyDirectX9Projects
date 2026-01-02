
/**
 * MovementComponent is an abstract component class that defines functionality for moving a PrimitiveComponent (our UpdatedComponent) each tick
 * Base functionality includes:
 * - restricting movement to a plane or axis
 * - utility functions for special handling of collision results (SlideAlongSurface(), ComputeSlideVector(), TwoWallAdjust())
 * - utility functions for moving when there may be initial penetration (SafeMoveUpdatedComponent(), ResolvePenetration())
 * - automatically registering the component tick and finding a component to move on the owning Actor
 * Normally the root component of the owning Actor is moved, however another component may be selected (see SetUpdatedComponent())
 * During swept (non-teleporting) movement only collision of UpdatedComponent is considered, attached components will teleport to the end location ignoring collision
 */
class UMovementComponent : public UActorComponent
{
    /**
     * the component we move and update
     * if this is null at startup and bAutoRegisterUpdatedComponent is true, the owning Actor's root component will automatically be set as our UpdatedComponent at startup 
     */
    TObjectPtr<USceneComponent> UpdatedComponent;

    /** UpdatedComponent, casted as a UPrimitiveComponent: may be invalid if UpdatedComponent was null or not a UPrimitiveComponent */
    TObjectPtr<UPrimitiveComponent> UpdatedPrimitive;
};

/**
 * NavMovementComponent defines base functionality for MovementComponents that move any 'agent' that may be involved in AI pathfinding 
 */
class UNavMovementComponent : public UMovementComponent
{

};

/**
 * PawnMovementComponent can be used to update movement for an associated Pawn
 * it also provides ways to accumulate and read directional input in a generic way (with AddInputVector(), ConsumeInputVector(), etc)
 */
class UPawnMovementComponent : public UNavMovementComponent
{
    virtual FVector ConsumeInputVector()
    {
        return PawnOwner ? PawnOwner->Internal_ConsumeMovementInputVector() : FVector::ZeroVector;
    }

    /** pawn that owns this component */
    TObjectPtr<class APawn> PawnOwner;
};