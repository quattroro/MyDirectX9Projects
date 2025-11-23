/**
 * a SceneComponent has a transform and supports attachment, but has no rendering or collision capabilities
 * useful as a 'dummy' component in hierarchy to offset others 
 */
// 17 - Foundation - CreateWorld - USceneComponent
// haker: as we covered, USceneComponent supports scene-graph:
// - what is scene-graph for?
//   - supports hierarichy:
//     - representative example is 'transforms'
// see USceneComponent's member variables (goto 18)
class USceneComponent : public UActorComponent
{
    /** get the SceneComponent we are attached to */
    USceneComponent* GetAttachParent() const
    {
        return AttachParent;
    }

    // 18 - Foundation - CreateWorld - USceneComponent's member variables
    // haker: with AttachParent and AttachChildren, it supports tree-structure for scene-graph

    /** what we are currently attached to. if valid, RelativeLocation etc. are used relative to this object */
    UPROPERTY(ReplicatedUsing=OnRep_AttachParent)
    TObjectPtr<USceneComponent> AttachParent;

    /** list of child SceneComponents that are attached to us. */
    UPROPERTY(ReplicatedUsing = OnRep_AttachChildren, Transient)
    TArray<TObjectPtr<USceneComponent>> AttachChildren;
};