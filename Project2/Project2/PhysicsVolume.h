
class ABrush : public AActor
{

};

/** an editable 3D volume placed in a level; different types of volumes perform different functions */
// 008 - Foundation - UpdateOverlaps - AVolume
// haker: you can make your own version of AVolume using implementation in PhysicsVolume e.g. IsOverlapInVolume()
// - rather using APhysicsVolume, in big project, engineers make their own Volume Manager rather using existing one like PhysicsVolume
class AVolume : public ABrush
{

};

/**
 * A bounding volume which affects actor physics
 * Each AActor is affected at any time by one PhysicsVolume 
 */
// 007 - Foundation - UpdateOverlaps - APhysicsVolume
// see AVolume (goto 008: UpdateOverlaps)
// see APhysicsVolume member variables (009: UpdateOverlaps)
class APhysicsVolume : public AVolume
{
    /** called when actor enteres a volume */
    virtual void ActorEnteredVolume(class AActor* Other) {}

    /** called when actor leaves a volume, other can be NULL */
    virtual void ActorLeavingVolume(class AActor* Other) {}

    // 009 - Foundation - UpdateOverlaps - APhysicsVolume member variables

    /** determine which PhysicsVolume takes precedence if they overlap (higher number = higher priority) */
    // haker: priority is the order value:
    //                                              
    //                      Volume0(Pri:100)        
    //                 ┌──────────┐                 
    //  Volume2(Pri:40)│          │                 
    //           ┌─────┼─────┐    │                 
    //           │     │     │    │                 
    //           │   ┌─┼─────┼────┼─────┐           
    //           │   │ │  +  │    │     │           
    //           │   │ └──▲──┼────┘     │           
    //           │   │    │  │          │           
    //           └───┼────┼──┘          │           
    //               │    │             │           
    //               └────┼─────────────┘           
    //                    │        Volume1(Pri:200) 
    //                    │                         
    //                    │                         
    //                    │                         
    //              Volume1 is selected             
    //               (200 > 100 > 30)               
    //                 ▲                            
    //                 │                            
    //               Volume1                        
    int32 Priority;
};