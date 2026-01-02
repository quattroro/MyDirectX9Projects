// 016 - Foundation - AttachToComponent - ECollisionTraceFlag
// haker: you need to understand what the difference between simple and complex is:
//                                                                 
//  Simple Collision                 Complex Collision             
//   : using Shape Type               : using Polygon              
//   │                                 │                           
//   ├────Sphere                       └───Static Mesh's triangles 
//   │                                                             
//   ├────Box                                                      
//   │                                                             
//   ├────Capsule                                                  
//   │                                                             
//   ├────Convex(e.g. K-dops)                                      
//   │                                                             
//   └────...                                                      
//     
// - Basically, complex and simple has separate collision channel:
//   1. regular(simple) scene queries
//   2. complex scene queries                                                            
enum ECollisionTraceFlag : int32
{
    /** use project physics settings (DefaultShapeComplexity) */
    CTF_UseDefault,
    /** 
     * create both simple and complex shapes: 
     * - simple shapes are used for regular scene queries and collision tests 
     * - complex shape is used for complex scene queries
     */
    CTF_UseSimpleAndComplex,
    /**
     * create only simple shapes:
     * - use simple shapes for all scene queries and collision tests 
     */
    // haker: complex scene queries can deal with simple collision
    CTF_UseSimpleAsComplex,
    /**
     * create only complex shapes (per-poly)
     * - use complex shapes for all scene queries and collision tests
     * - can be used in simulation for static shapes only (i.e. can be collided against but not moved through forces or velocity) 
     */
    // haker: regular scene queries can deal with complex collision
    CTF_UseComplexAsSimple,
    CTF_MAX,
};

// 015 - Foundation - AttachToComponent - UBodySetupCore
// haker: you can think of 'UBodySetupCore' as 'StaticMesh'
// - FBodyInstance is like 'StaticMeshComponent'
struct UBodySetupCore : public UObject
{
    ECollisionTraceFlag GetCollisionTraceFlag() const
    {
        ECollisionTraceFlag DefaultFlag = UPhysicsSettingsCore::Get()->DefaultShapeComplexity;
        return CollisionTraceFlag == ECollisionTraceFlag::CTF_UseDefault ? DefaultFlag : CollisionTraceFlag;
    }

    /** collision trace behavior - by default, it will keep simple(convex)/complex(per-poly) separate */
    // see ECollisionTraceFlag (goto 016: ECollisionTraceFlag)
    ECollisionTraceFlag CollisionTraceFlag;
};

// 014 - Foundation - AttachToComponent - FBodyInstanceCore
// haker: see FBodyInstanceCore's member variables
struct FBodyInstanceCore
{
    /** should simulate physics */
    // 019 - Foundation - AttachToComponent - FBodyInstanceCore::ShouldInstanceSimulatingPhysics
    bool ShouldInstanceSimulatingPhysics() const
    {
        // haker: note that if our body setup's collision trace flag is ComplexAsSimple, it doesn't simulate physics!
        // - what does it mean?
        //   - if you have implemented physics engine, it is natural that physics simulation is based on simple shape e.g. sphere, capsule, box and etc, NOT in traingle mesh!
        //   - when the primitive component's body setup uses CTF_UseComplexAsSimple, the primitive component is used as blocker (not simulating object) e.g. wall, house, etc...
        // - what about destruction simulation?
        //   - it is based triangle(polygon) simulation, but it runs in separate physics engine e.g. Apex Engine
        // see GetCollisionTraceFlag() breifly
        return bSimulatePhysics && BodySetup.IsValid() && BodySetup->GetCollisionTraceFlag() != ECollisionTraceFlag::CTF_UseComplexAsSimple;
    }

    /** BodySetupCore pointer that this instance is initialized from */
    // haker: see UBodySetupCore (goto 015: AttachToComponent)
    TWeakObjectPtr<UBodySetupCore> BodySetup;

    /**
     * - if true, this body will use simulation:
     *   - if false, will be 'fixed' (i.e. kinematic) and move where it is told
     * 
     * - For a SkeletalMeshComponent, simulating requires a physics asset setup and assigned on the Skeletalmesh asset
     * - For a StaticMeshComponent, simulating requires simple collision to be setup on the StaticMesh asset  
     */
    // haker: 'bSimulatePhysics' is important to note!
    // - for now, just try to understand this flag as trigger to turn on/off physics effects
    // - in details, for now it is too early to understand this further...
    uint8 bSimulatePhysics : 1;
};

/** container for a physics representation of an object */
// 013 - Foundation - AttachToComponent - FBodyInstance
// see FBodyInstanceCore (goto 014: AttachToComponent)
struct FBodyInstance : public FBodyInstanceCore
{
    /** takes a welded body and unwelds it: this function does not create the new body, it only removes the old one */
    void UnWeld(FBodyInstance* Body)
    {
        //...
    }

    /** the parent body that we are welded to */
    // haker: as we covered, multiple components share same rigid-body(FBodyInstance):
    //                                                        
    // Component0:BodyInstance0                               
    // ┌───────────┐                                          
    // │           │                                          
    // └───────┬───┤                                          
    //         │   │    Component2:WeldParent=BodyInstance0   
    //         │   ├─────────┐                                
    //         │   │         │                                
    //         │   ├─────────┘                                
    //         │   │                                          
    //         └───┘                                          
    //          Component1:WeldParent=BodyInstance0           
    //                                                        
    //                                                        
    //                                                        
    //        Component0:AttachChildren[Component1]           
    //            ▲      WeldChildren[Component1,Component2]  
    //            │                                           
    //        Component1:AttachChildren[Component2]           
    //            ▲                                           
    //            │                                           
    //        Component2                                      
    //                                                        
    FBodyInstance* WeldParent;
};