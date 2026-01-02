/** MeshComponent is an abstract base for any component that is an instance of a renderable collection of triangles */
class UMeshComponent : public UPrimitiveComponent
{

};

// 015 - Foundation - UpdateComponentToWorld - USkeletalMeshSocket
// haker: see USkeletalMeshSocket's member variables
class USkeletalMeshSocket : public UObject
{
public:
    /** returns FTransform of Socket local transform */
    // 017 - Foundation - UpdateComponentToWorld - USkeletalMeshSocket::GetSocketLocalTransform
    FTransform GetSocketLocalTransform() const
    {
        return FTransform(RelativeRotation, RelativeLocation, RelativeScale);
    }

    /**
     * defines a named attachment location on the SkeletalMesh
     * - These are set up in editor and used as a shortcut instead of specifying
     *   everything explicitly to AttachComponent in the SkeletalMeshComponent
     * - the outer of a SkeletalMeshSocket should always be the USkeletalMesh
     */
    // haker: what is the outer of USkeletalMeshSocket?
    // - USkeletalMesh is the outer object of USkeletalMeshSocket

    // haker: as we covered socket is defined by name    
    FName SocketName;

    // haker: where is the socket based on?
    FName BoneName;
    
    // haker: offset related to bone space
    FVector RelativeLocation;
    FRotator RelativeRotation;
    FVector RelativeScale;

    // search (goto 012: UpdateComponentToWorld)
};

class USkinnedAsset : public UStreamableRenderAsset
{

};

/**
 * SkeletalMesh is geometry bound to hierarchical skeleton of bones which can be animated for the purpose of deforming the mesh
 * Skeletal Meshes are built up of two parts; a set of polygons composed make up the surface of the mesh, and a hierarchical skeleton which can be used to animate the polygons
 * the 3D models, rigging, and animations are created in an external modeling and animation application (3DSMax, Maya, Softimage, etc) 
 */
// 014 - Foundation - UpdateComponentToWorld - USkeletalMesh
// haker: can you explain what is USkeletalMesh and USkeletalMeshComponent?
// - you can think of relationship between UObject and UClass
//   - UClass == USkeletalMesh
//   - UObject == SkinnedMeshComponent or SkeletalMeshComponent  
// - Diagram:                                                                                                  
//                                                                                                    
//                                                             Instanced(World)                       
//                                                                     ┌──────►SkeletalMeshComponent0 
//                                                                     │                              
//                                                                     │                              
//  NPC Model(Mesh,Animation,Rigging)            SkeletalMesh ─────────┼──────►SkeletalMeshComponent1 
//    │                                           │                    │                              
//    ├───3dmax                        ───────►   └──NPC_Monster       │                              
//    │                                                                └──────►SkeletalMeshComponent2 
//    └───maya                       Imported                                                         
//                                     into Unreal                                                    
//                                                                                                  
// - see USkeletalMesh's member variables
class USkeletalMesh : public USkinnedAsset
{
    /**
     * find a socket object and associated info in this SkeletalMesh by name
     * - entering NAME_None will return NULL 
     *   - if there are multiple sockets with the same name, will return the first one
     * - also returns the index for the socket allowing for future fast access via GetSocketByIndex()
     * - also returns the socket local transform and the bone index (if any)
     */
    // 016 - Foundation - UpdateComponentToWorld - USkeletalMesh::FindSocketInfo
    // haker: note that its output parameters (OutTransform, OutBoneIndex and OutIndex) not only for return type
    virtual USkeletalMeshSocket* FindSocketInfo(FName InSocketName, FTransform& OutTransform, int32& OutBoneIndex, int32& OutIndex) const override
    {
        OutIndex = INDEX_NONE;
        OutTransform = FTransform::Identity;
        OutBoneIndex = INDEX_NONE;

        if (InSocketName == NAME_None)
        {
            return nullptr;
        }

        // haker: 1. SkeletalMesh' Sockets
        // - firstly iterate Sockets in USkeletalMesh
        for (int32 i = 0; i < Sockets.Num(); i++)
        {
            USkeletalMeshSocket* Socket = Sockets[i];
            if (Socket && Socket->SocketName == InSocketName)
            {
                OutIndex = i;
                // see GetSocketLocalTransform (goto 017: UpdateComponentToWorld)
                OutTransform = Socket->GetSocketLocalTransform();
                
                // haker: we are not going to cover Skeleton in this lecture
                // - what is the relationship between skeleton and skeletal mesh?
                //   - different mesh can share same skelelton
                //     - male human skeleton is shared between player and npc or different npcs or players who has equipped different parts(armor)
                // - what is benefit to sharing skeleton?
                //   - we can share animation clips!
                //   - also sockets! what if skeleton have skeletal mesh socket? (we'll see sooner or later)
                OutBoneIndex = GetRefSkeleton().FindBoneIndex(Socket->BoneName);
                return Socket;
            }
        }

        // if the socket isn't on the mesh, try to find it on the skeleton
        if (GetSkeleton())
        {
            // haker: 2. Skeleton's Sockets
            // - Can you explain why skeleton has separate sockets?
            //   - like human npc or player, share same skeleton
            //   - we don't need to duplicate same sockets which are based on skeleton
            //   - in this case, it is beneficial to have sockets in skeleton rather than skeletal mesh
            USkeletalMeshSocket* SkeletonSocket = GetSkeleton()->FindSocketAndIndex(InSocketName, OutIndex);
            if (SkeletonSocket != nullptr)
            {
                // haker: note that for skelton socket index is offseted by USkeletalMesh's Sockets' Num()
                // e.g.
                // USkeletalMesh Sockets == [A, B, C, D]
                // USkeleton Sockets == [E, F]
                // - what is index of E?
                //   - 4!
                OutIndex += Sockets.Num();
                OutTransform = SkeletonSocket->GetSocketLocalTransform();
                OutBoneIndex = GetRefSkeleton().FindBoneIndex(SkeletonSocket->BoneName);
            }
            return SkeletonSocket;
        }

        return nullptr;
    }

    /**
     * array of named socket location, set up in editor and used as a shortcut instead of specifying
     * everything explicitly to AttachComponent in the SkeletalMeshComponent 
     */
    // haker: see USkeletalMeshSocket (goto 015: UpdateComponentToWorld)
    TArray<TObjectPtr<class USkeletalMeshSocket>> Sockets;
};

/** skinned mesh component that supports bone skinned mesh rendering */
class USkinnedMeshComponent : public UMeshComponent
{
    USkinnedAsset* GetSkinnedAsset() const
    {
        return SkinnedAsset;
    }

    // 012 - Foundation - UpdateComponentToWorld - USkinnedMeshComponent::GetSocketInfoByName
    USkeletalMeshSocket const* GetSocketInfoByName(FName InSocketName, FTransform& OutTransform, int32& OutBoneIndex) const
    {
        // haker: using SocketOverrideLookup, you can redirect socket name to different socket name
        // - see SocketOverrideLookup
        const FName* OverrideSocket = SocketOverrideLookup.Find(InSocketName);
        const FName* OverrideSocketName = OverrideSocket ? *OverrideSocket : InSocketName;

        // haker: for skeletal mesh, socket type is USkeletalMeshSocket
        // see USkinnedMeshComponent's member variables (goto 013)
        USkeletalMeshSocket const* Socket = nullptr;
        if (GetSkinnedAsset())
        {
            int32 SocketIndex;
            // see USkeletalMesh::FindSocketInfo (goto 016)
            Socket = GetSkinnedAsset()->FindSocketInfo(OverrideSocketName, OutTransform, OutBoneIndex, SocketIndex);
        }
        return Socket;
    }

    /** get parent bone of the input bone */
    FName GetParentBone(FName BoneName) const
    {
        FName Result = NAME_None;
        int32 BoneIndex = GetBoneIndex(BoneName);
        
        // this checks that this bone is not just the root (i.e. no parent), and that BoneIndex != INDEX_NONE (i.e. bone name was found)
        if ((BoneIndex != INDEX_NONE) && (BoneIndex > 0))
        {
            Result = GetSkinnedAsset()->GetRefSkeleton().GetBoneName(GetSkinnedAsset()->GetRefSkeleton().GetParentIndex(BoneIndex));
        }
        return Result;
    }

    // 011 - Foundation - UpdateComponentToWorld - FSkinnedMeshComponent::GetSocketTransform
    virtual FTransform GetSocketTransform(FName InSocketName, ERelativeTransformSpace TransformSpace = RTS_World) const override
    {
        FTransform OutSocketTransform = GetComponentTransform();
        // haker: how to understand what the socket is?
        //                                                                                                                         
        //      [Visual Look]                                                [Conceptual Structure]                                
        //                                                                                                                         
        //                  Head                                                           Root                                    
        //                    │                                                             ▲                                      
        //                    ▼           R_Arm_Socket[R_Arm,Offset]                        │                                      
        //             ┌───►Spine◄────┐  ┌─┐                                               Pelvis                                  
        //             │      │       │  └─┘                                                ▲                                      
        //          L_Arm     │      R_Arm  R_Hand_Socket[R_Hand,Offset]                    │                                      
        //           │        │       ▲    ┌─┐                                    ┌─────────┼─────────┐                            
        //           ▼        ▼       │    └─┘                                    │         │         │                            
        //       L_Hand  ┌─►Pelvis◄┐  R_Hand                                     L_Leg     Spine     R_Leg                         
        //               │    │    │                                              ▲          ▲        ▲                            
        //               │    │    │                                              │          │        │                            
        //             L_Leg  │  R_Leg                                          L_Foot       │       R_Root                        
        //              ▲     │     ▲                                                   ┌────┼──────┐                              
        //              │     ▼     │                                                   │    │      │                              
        //          L_Foot   Root   R_Foot                                              │    │      │                              
        //                                                                            L_Arm  Head  R_Arm                           
        //                                                                              ▲            ▲ │                           
        //                                                                              │            │ └─────R_Arm_Socket          
        //                                                                           L_Hand        R_Hand                          
        //                                                                                           │                             
        //                                                                                           └─────R_Hand_Socket           
        //
        // - you can think of socket as attachment space:
        //   - e.g. when you equip the sword on your spine with certain offset and rotation -> use socket (spine-bone, offset-transform)
        //   - e.g. when you hold portion on right hand -> use socket with R_Hand_Socket (R_Hand, offset-transform)
        // - simply, geometrically, you can think socket as offset(or another local space) based on bone space
        if (InSocketName != NAME_None)
        {
            // haker: we want to get bone's transform and socket local transform based on bone space
            // - use GetSocketInfoByName() and get the needed data
            // see USkinnedMeshComponent's member variables (goto 013: UpdateComponentToWorld)
            int32 SocketBoneIndex;
            FTransform SocketLocalTransform;
            USkeletalMeshSocket const* const Socket = GetSocketInfoByName(InSocketName, SocketLocalTransform, SocketBoneIndex);
            
            // apply the socket transform first if we find a matching socket
            if (Socket)
            {
                if (TransformSpace == RTS_ParentBoneSpace)
                {
                    // we are done just return now
                    // haker: as we can see in GetSocketInfoByName(), SocketLocalTrasform is already calculated
                    return SocketLocalTransform;
                }

                if (SocketBoneIndex != INDEX_NONE)
                {
                    // haker: we skip the detail of GetBoneTransform():
                    // - for now, using 'ComponentSpaceTransformArray and retrieve bone transform with bone index
                    // - it applies ComponentToWorld, so global-space
                    FTransform BoneTransform = GetBoneTransform(SocketBoneIndex);
                    OutSocketTransform = SocketLocalTransform * BoneTransform;
                }
            }
            else
            {
                // haker: this is why we want to handle SkinnedMeshComponent's GetSocketTransform briefly!
                // - even though, there is no socket matching socket name, it return bone's name matching socket name
                // - when we enter here, you can think of socket which has identity matrix based on bone name
                int32 BoneIndex = GetBoneIndex(InSocketName);
                if (BoneIndex != INDEX_NONE)
                {
                    OutSocketTransform = GetBoneTransform(BoneIndex);

                    // haker: if our TransformSpace is based on RTS_ParentBoneSpace, we need to calculate relative transform from parent-bone
                    if (TransformSpace == RTS_ParentBoneSpace)
                    {
                        FName ParentBone = GetParentBone(InSocketName);
                        int32 ParentIndex = GetBoneIndex(ParentBone);
                        if (ParentIndex != INDEX_NONE)
                        {
                            // haker: you can think of A.GetRelativeTransform(B) as A/B
                            return OutSocketTransform.GetRelativeTransform(GetBoneTransform(ParentIndex));
                        }
                        return OutSocketTransform.GetRelativeTransform(GetComponentTransform());
                    }
                }
            }
        }

        // haker: here, OutSocketTransform is based on global-space(World-space)
        switch (TransformSpace)
        {
            case RTS_Actor:
            {
                if (AActor* Actor = GetOwner())
                {
                    return OutSocketTransform.GetRelativeTransform(Actor->GetTransform());
                }
                break;
            }
            case RTS_Component:
            {
                return OutSocketTransform.GetRelativeTransform(GetComponentTransform());
            }
        }

        return OutSocketTransform;

        // haker: can you answer the following questions? 
        // - can you understand how socket in skeletal mesh works?
        // - can you understand difference between bone and socket?
    }

    // 013 - Foundation - UpdateComponentToWorld - USkinnedMeshComponent's member variables

    /** mapping for socket overrides, key as the Source socket name and the value is the override socket name */
    TSortedMap<FName, FName> SocketOverrideLookup;

    /** temporary array of component-space bone matrices, update each frame and used for rendering the mesh */
    // haker: it is reader/writer (producer/consumer concept) by having two arrays
    // - skeletal mesh consist of bones
    // - each bone has unique id (as index)
    // - using bone index, you can access bone-to-world transform in ComponentSpaceTransformArray
    //                                                                                   
    //                          [Visual Look]                                            
    //                                                                                   
    //                                      Head                                         
    //                                        │                                          
    //                                        ▼           R_Arm_Socket[R_Arm,Offset]     
    //                                 ┌───►Spine◄────┐  ┌─┐                             
    //                                 │      │       │  └─┘                             
    //                              L_Arm     │      R_Arm  R_Hand_Socket[R_Hand,Offset] 
    //                               │        │       ▲    ┌─┐                           
    //                               ▼        ▼       │    └─┘                           
    //                           L_Hand  ┌─►Pelvis◄┐  R_Hand                             
    //                                   │    │    │                                     
    //                                   │    │    │                                     
    //                                 L_Leg  │  R_Leg                                   
    //                                  ▲     │     ▲                                    
    //                                  │     ▼     │                                    
    //                              L_Foot   Root   R_Foot                               
    //                                                                                   
    //                                                                                   
    //                          Bone: [Root, Pelvis, Spine, L_Leg, R_Leg, ...]           
    //                                                                                   
    //                     BoneIndex: [0   , 1     , 2    , 3    , 4    , ...]           
    //                                                                                   
    //  ComponentSpaceTransformArray: [Tr0 , Tr1   , Tr2  , Tr3  , Tr4  , ...]           
    //                                                                                   
    //----------------------------------------------------------------------------------------------
    //                      1. What is transform (bone->world) of Pelvis? Tr1            
    //                      2. What is transform (bone->world) of R_Leg?  Tr4            
                                                                                       
    TArray<FTransform> ComponentSpaceTransformArray[2];

    /** the skinned asset used by this component */
    // haker: you can think this(SkinnedAsset) as SkeletalMesh
    // see USkeletalMesh (goto 014: UpdateComponentToWorld)
    TObjectPtr<class USkinnedAsset> SkinnedAsset;
};