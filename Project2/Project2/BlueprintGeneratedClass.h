
struct FComponentKey
{
    // 017 - Foundation - ExecuteConstruction * - FComponentKey::FComponentKey
    FComponentKey(const USCS_Node* SCSNode) : OwnerClass(nullptr)
    {
        if (SCSNode)
        {
            // haker: see briefly USCS_Node::GetSCS()
            const USimpleConstructionScript* ParentSCS = SCSNode->GetSCS();
            OwnerClass = ParentSCS ? ParentSCS->GetOwnerClass() : nullptr;
            AssociatedGuid = SCSNode->VariableGuid;
            // haker: GetVariableName() returns InternalVariableName
            SCSVariableName = SCSNode->GetVariableName();
        }
    }

    TObjectPtr<UClass> OwnerClass;
    FName SCSVariableName;
    FGuid AssociatedGuid;

    bool Match(const FComponentKey& OtherKey) const
    {
        return (OwnerClass == OtherKey.OwnerClass) && (AssociatedGuid == OtherKey.AssociatedGuid);
    }
};

// 005 - Foundation - ExecuteConstruction * - FComponentOverrideRecord
// haker: see FComponentOverrideRecord's member variables
struct FComponentOverrideRecord
{
    // haker: it contains ComponentClass and its template(archetype), ComponentTemplate
    TObjectPtr<UClass> ComponentClass;
    TObjectPtr<UActorComponent> ComponentTemplate;

    // haker: see FComponentKey briefly
    FComponentKey ComponentKey;
};

// 004 - Foundation - ExecuteConstruction * - UInheritableComponentHandler
// haker: first let's see the member variable, then I'll explain what UInheritableComponentHandler do
class UInheritableComponentHandler : public UObject
{
    const FComponentOverrideRecord* FindRecord(const FComponentKey& Key) const
    {
        for (const FComponentOverrideRecord& Record : Records)
        {
            if (Record.ComponentKey.Match(Key))
            {
                return &Record;
            }
        }
        return nullptr;
    }

    UActorComponent* GetOverridenComponentTemplate(const FComponentKey& Key) const
    {
        const FComponentOverrideRecord* Record = FindRecord(Key);
        return Record ? Record->ComponentTemplate : nullptr;
    }

    /** all component records */
    // haker: see FComponentOverrideRecord (goto 005: ExecuteConstruction)
    TArray<FComponentOverrideRecord> Records;

    // haker: let's understand UInheritableComponentHandler conceptually:
    // Diagram:
    //                                                                                                                                         
    // BP0                                                                                                                                 
    //  │                                                                                                                                  
    //  ├──SCS(SimpleConstructionScript)      Component4 is attached to Component3                                                         
    //  │   │                                 ──────────────────────►                1.Look up InheritableComponentHandler                 
    //  │   └──Component1                                                               whether any mapping exists for Component4          
    //  │       │                                                                                                                          
    //  │       ├─Component2                                                            InheritableComponentHandler                        
    //  │       │                                                                        │                                                 
    //  │       └─Component3                                                             └──Component4:Component5◄────*** mapping exists!  
    //  │                                                                                                                                  
    //  │                                                                                                                                  
    //  └──InheritableComponentHandler                                                         │                                           
    //      │                                                                                  │                                           
    //      └──Component4:Component5                                                           │                                           
    //                                                                                         │                                           
    //                                                                                         │                                           
    //                                                                                         ▼                                           
    //                                                                               2.Replace Template from Component4 to Component5      
    //                                                                                 then add 'Component5' to BP0 on Component3          
    //                                                                                                                                     
    //                                                                                    BP0                                              
    //                                                                                     │                                               
    //                                                                                     ├──SCS(SimpleConstructionScript)                
    //                                                                                     │   │                                           
    //                                                                                     │   └──Component1                               
    //                                                                                     │       │                                       
    //                                                                                     │       ├─Component2                            
    //                                                                                     │       │                                       
    //                                                                                     │       └─Component3                            
    //                                                                                     │          │                                    
    //                                                                                     │          └──Component5                        
    //                                                                                     │                                               
    //                                                                                     └──InheritableComponentHandler                  
    //                                                                                         │                                           
    //                                                                                         └──Component4:Component5                    
                                                                                                                                      
};

// 007 - Foundation - ExecuteConstruction * - USCS_Node
// haker: USCS_Node is USimpleConstructionScript_Node:
// - USCS_Node is wrapper class maintaining tree-structure in SCS
// - see USCS_Node's member variables (goto 008: ExecuteConstruction)
class USCS_Node : public UObject
{
    /** get the SCS that owns this node */
    const USimpleConstructionScript* GetSCS() const
    {
        return CastChecked<USimpleConstructionScript>(GetOuter());
    }

    /** return the actual component template used in the BPGC. the template can be overriden in a child */
    // 016 - Foundation - ExecuteConstruction * - USCS_Node::GetActualComponentTemplate
    // haker: get the ComponentTemplate to create new component
    UActorComponent* GetActualComponentTemplate(class UBlueprintGeneratedClass* ActualBPGC) const
    {
        // haker: we had covered what the InheritableComponentHandler is:
        // - using InheritableComponentHandler, try to override ComponentTemplate
        UActorComponent* OverridenComponentTemplate = nullptr;

        // GConfig->GetBool(Section, Key, bValue, Filename);
        static const FBoolConfigValueHelper EnableInheritableComponents(TEXT("Kismet"), TEXT("bEnableInheritableComponents"), GEngineIni);
        if (EnableInheritableComponents)
        {
            const USimpleConstructionScript* SCS = GetSCS();
            if (SCS != ActualBPGC->SimpleConstructionScript)
            {
                // see FComponentKey::FComponentKey (goto 017: ExecuteConstruction)
                const FComponentKey ComponentKey(this);

                // haker: by iterating parent's BP, look-up InheritableComponentHandler and find the matching ComponentTemplate
                do
                {
                    UInheritableComponentHandler* InheritableComponentHandler = ActualBPGC->GetInheritableComponentHandler();
                    if (InheritableComponentHandler)
                    {
                        // haker: see GetOverridenComponentTemplate briefly
                        OverridComponentTemplate = InheritableComponentHandler->GetOverridenComponentTemplate(ComponentKey);
                    }
                    ActualBPGC = Cast<UBlueprintGeneratedClass>(ActualBPGC->GetSuperClass());
                } while (!OverridenComponentTemplate && ActualBPGC && SCS != ActualBPGC->SimpleConstructionScript);
            }
        }
        return OverridenComponentTemplate ? OverridenComponentTemplate : ToRawPtr(ComponentTemplate);
    }

    /** create the specified component on the actor, then call action on children */
    // 015 - Foundation - ExecuteConstruction * - USCS_Node::ExecuteNodeOnActor
    // haker: ExecuteNodeOnActor is the function that we expect:
    // - it create its component and recursivly call ExecuteNodeOnActor on its children
    // - from here, we can see the glimpse of what the archetype is
    UActorComponent* ExecuteNodeOnActor(AActor* Actor, USceneComponent* ParentComponent, const FTransform* RootTransform, const struct FRotationConversionCache* RootRelativeRotationCache, bool bIsDefaultTransform, ESpawnActorScaleMethod TransformScaleMethod=ESpawnActorScaleMethod::OverrideRootScale)
    {
        // create a new component instance based on the template
        UActorComponent* NewActorComp = nullptr;
        UBlueprintGeneratedClass* ActualBPGC = Cast<UBlueprintGeneratedClass>(Actor->GetClass());

        // haker: see USCS_Node::GetActualComponentTemplate (goto 016: ExecuteConstruction)
        if (UActorComponent* ActualComponentTemplate = GetActualComponentTemplate(ActualBPGC))
        {
            // haker: see CreateComponentFromTemplate (goto 018: ExecuteConstruction)
            NewActorComp = Actor->CreateComponentFromTemplate(ActualComponentTemplate, InternalVariableName);
        }

        if (NewActorComp)
        {
            // haker: as I said, it overrides CreateMethod as 'SimpleConstructionScript' here
            NewActorComp->CreationMethod = EComponentCreationMethod::SimpleConstructionScript;

            if (!NewActorComp->HasBeenCreated())
            {
                // call function to notify component it has been created
                NewActorComp->OnComponentCreated();
            }

            // special handling for scene components
            USceneComponent* NewSceneComp = Cast<USceneComponent>(NewActorComp);
            if (NewSceneComp)
            {
                // only register scene components if the world is initialized
                UWorld* World = Actor->GetWorld();
                bool bRegisterComponent = World && World->bIsWorldInitialized;

                // If NULL is passed in, we are the root, so set transform and assign as RootComponent on Actor, similarly if the
                // NewSceneComp is the ParentComponent then we are the root component:
                // - this happens when the root component is recycled by StaticAllocateObject

                // haker: ParentComponent is passed as nullptr initially (when SCS is initialized at the first time)
                // - for now, I can't guess the case when ParentComponent == NewSceneComp
                //   - in above comment, the case, 'ParentComponent == NewSceneComp' happens when StaticAllocateObject recycles a root component
                if (!IsValid(ParentComponent) || ParentComponent == NewSceneComp)
                {
                    // haker: when ParentComponent == nullptr, this function is called on RootNode:
                    // - in that case, RootTransform has previous transform of RootComponent (now becomes nullptr)
                    // - but, the value of RootTransform depends on where ExecuteNodeOnActor() is called
                    FTransform WorldTransform = *RootTransform;
                    switch (TransformScaleMethod)
                    {
                    // haker: recommend to skim the code of dealing with ESpawnActorScaleMethod briefly: not that important
                    case ESpawnActorScaleMethod::OverrideRootScale:
                    case ESpawnActorScaleMethod::SelectDefaultAtRuntime:
                        // use the provided transform and ignore the root component
                        break;
                    case ESpawnActorScaleMethod::MultiplyWithRoot:
                        WorldTransform = NewSceneComp->GetRelativeTransform() * WorldTransform;
                        break;
                    }

                    if (RootRelativeRotationCache)
                    {
                        // enforce using the same rotator as much as possible
                        NewSceneComp->SetRelativeRotationCache(*RootRelativeRotationCache);
                    }

                    // haker: if we are going to create the component which will be assigned to RootComponent, set world-transform and and set root-component
                    NewSceneComp->SetWorldTransform(WorldTransform);
                    Actor->SetRootComponent(NewSceneComp);

                    // this will be true if we deferred the RegisterAllComponents() call at spawn time
                    // - in this case, we can call it now since we have set a scene root
                    if (Actor->HasDeferredComponentRegistration() && bRegisterComponent)
                    {
                        // register the root component along with any components whose registration may have been deferred pending SCS execution in order to establish a root
                        // haker: if the world is already 'BeginPlay' and the Actor is finished to register components, call RegisterAllComponents here! 
                        // - we call this function in run-time
                        Actor->RegisterAllComponents();
                    }
                }
                else
                {
                    // otherwise, attach to parent component passed in
                    // haker: usually we'll reach here:
                    // - when ExecuteNodeOnActor() is called recursively on its children, 'ParentComponent' exists!
                    NewSceneComp->SetupAttachment(ParentComponent, AttachToName);
                }

                // register SCS scene components now (if necessary):
                // - Non-scene SCS component registration is deferred until after SCS execution, as there can be dependencies on the scene hierarchy
                // haker: see briefly 'bRegisterComponent'
                if (bRegisterComponent)
                {
                    // haker: handling RootComponent similarly, we try to call register newly created component
                    // - we'll skip the detail of FStaticMeshComponentBulkReregisterContext
                    FStaticMeshComponentBulkReregisterContext* ReregisterContext = Cast<USimpleConstructionScript>(GetOuter())->GetReregisterContext();
                    if (ReregisterContext)
                    {
                        ReregisterContext->AddConstructedComponent(NewSceneComp);
                    }

                    // haker: see RegisterInstancedComponent (goto 022: ExecuteConstruction)
                    USimpleConstructionScript::RegisterInstancedComponent(NewSceneComp);
                }
            }

            // if we want to save this to a property, do it here
            // haker: update variable name in AActor
            FName VarName = InternalVariableName;
            if (VarName != NAME_None)
            {
                UClass* ActorClass = Actor->GetClass();
                if (FObjectPropertyBase* Prop = FindFProperty<FObjectPropertyBase>(ActorClass, VarName))
                {
                    // if it is null we don't really know what's going on, but make it behave as it did before the bug fix
                    if (Prop->PropertyClass == nullptr || NewActorComp->IsA(Prop->PropertyClass))
                    {
                        Prop->SetObjectPropertyValue_InContainer(Actor, NewActorComp);
                    }
                }
            }

            // determine the parent component for our children (it's still our parent if we're a non-scene component)
            USceneComponent* ParentSceneComponentOfChildren = (NewSceneComp != nullptr) ? NewSceneComp : ParentComponent;

            // if we made a component, go ahead and process our children
            // haker: recursively call children on SCS
            for (int32 NodeIdx = 0; NodeIdx < ChildNodes.Num(); NodeIdx++)
            {
                USCS_Node* Node = ChildNodes[NodeIdx];
                Node->ExecuteNodeOnActor(Actor, ParentSceneComponentOfChildren, nullptr, nullptr, false);
            }
        }

        return NewActorComp;
    }

    // 008 - Foundation - ExecuteConstruction * - USCS_Node's member variables

    // haker: as we saw in UInheritableComponentHandler, VariableName, ComponentClass and ComponentTemplate is the unit of SCS, 'USCS_Node'

    /** component class */
    TObjectPtr<UClass> ComponentClass;

    /** template for the component to create */
    TObjectPtr<class UActorComponent> ComponentTemplate;

    /**
     * Internal variable name: this is used for:
     * a) generating the component template (archetype) object name
     * b) A FObjectProperty in the generated Blueprint class:
     *    - This holds a reference to the component instance created at Actor Construction Time
     * c) Archetype lookup through the generated Blueprint class:
     *    - All instances route back to the archetype through the variable name (i.e. not the template name)
     */
    // haker: skimming what InternalVariableName do from above comment:
    // - you can think of it as just variable in SCS
    FName InternalVariableName;

    /** component template or variable that Node might be parented to */
    // haker: it is usually NAME_None
    FName ParentComponentOrVariableName;

    /** if the node is parented, this indicates whehter or not the template is found in the CDO's Components array */
    bool bIsParentComponentNative;

    /** socket/bone that Node might attach to */
    // haker: think of it as like 'AttachSocketName'
    FName AttachToName;

    /** set of child nodes */
    // haker: ChildNodes constructs tree-structure in SCS
    TArray<TObjectPtr<class USCS_Node>> ChildNodes;

    // haker: let's understand SCS with USCS_Node conceptually:
    //                                                                                                 
    //   BP0                                                                                           
    //    │                                                                                            
    //    └──SCS(SimpleConstructionScript)                                                             
    //        │                                                                                        
    //        ├──SCS_Node0───ComponentTemplate0:Var0                                                   
    //        │                                                                                        
    //        └──SCS_Node1───ComponentTemplate1:Var1                                                   
    //           │                                                                                     
    //           ├──SCS_Node2───ComponentTemplate0:Var2◄───***ComponentTemplate can be shared          
    //           │                                            :by VariableName,distinguish instances   
    //           └──SCS_Node3───ComponentTemplate2:Var3                                                
    //                                                                                                 
};

/**
 * FStaticMeshComponentBulkReregisterContext:
 * - more efficient handles bulk reregistering of static mesh components by removing and adding scene and physics debug render stat in bulk render commands rather than one at a time 
 * - a significant fraction of the cost of reregistering components is the synchronization cost of issuing commands to render thread
 * - bulk render commands means you only pay this cost once, rather than per component, potentially providing up to a 4x speed up
 * 
 * - when a context is active, the bBulkReregister flag is set on the primitive component:
 *   - this disables calls to SendRenderDebugPhysics, AddPrimitive, RemovePrimitive, and ReleasePrimitive, where they would otherwise occur during re-registration, with the assumption that the constructor and destructor of the context handles those tasks
 * - to allow the optimization to be applied to re-created components, "AddSimpleConstructionScript" can be called to register simple construction scripts, so any components they create get added to the context as well
 * - it's not a problem if re-created components are missed by the context, they'll just lose performance by going through the slower individual component code path
 */
enum class EBulkReregister
{
    Component,    // reregistering components
    RenderState,  // updating render state only -- limits reregistration to components with bRenderStateDirty set, and skips ReleasePrimitives
};

class FStaticMeshComponentBulkReregisterContext
{
    FSceneInterface* Scene;
    TArray<UPrimitiveComponent*> StaticMeshComponents;
    TArray<USimpleConstructionScript*> SCSs;
};

// 006 - Foundation - ExecuteConstruction * - USimpleConstructionScript
// haker: see USimpleConstructionScript's member variables
class USimpleConstructionScript : public UObject
{
    FStaticMeshComponentBulkReregisterContext* GetReregisterContext() const { return ReregisterContext; }

    /** helper method to reregister instanced components post-construction */
    // 022 - Foundation - ExecuteConstruction * - USimpleConstructionScript::RegisterInstancedComponent
    static void RegisterInstancedComponent(UActorComponent* InstancedComponent)
    {
        // if this is a scene component, recursively register parent attachments within the actor's scene hierarchy first
        // haker: if AttachParent is not registered yet, firstly register parent component
        if (USceneComponent* SceneComponent = Cast<USceneComponent>(InstancedComponent))
        {
            USceneComponent* ParentComponent = SceneComponent->GetAttachParent();
            if (ParentComponent != nullptr 
                && ParentComponent->GetOwner() == SceneComponent->GetOwner()
                && !ParentComponent->IsRegistered())
            {
                RegisterInstancedComponent(ParentComponent);
            }
        }

        if (IsValid(InstancedComponent) && !InstancedComponent->IsRegistered() && InstancedComponent->bAutoRegister)
        {
            InstancedComponent->RegisterComponent();
        }
    }

    /** execute this script on the supplied actor, creating components */
    // 014 - Foundation - ExecuteConstruction * - USimpleConstructionScript::ExecuteScriptOnActor
    void ExecuteScriptOnActor(AActor* Actor, const TInlineComponentArray<USceneComponent*>& NativeSceneComponents, const FTransform& RootTransform, const FRotationConversionCache* RootRelativeRotationCache, bool bIsDefaultTransform, ESpawnActorScaleMethod TransformScaleMethod=ESpawnActorScaleMethod::OverrideRootScale);
    {
        // haker: see RootNodes briefly in SCS
        if (RootNodes.Num() > 0)
        {
            // get the given actor's root component (can be NULL)
            USceneComponent* RootComponent = Actor->GetRootComponent();

            for (USCS_Node* RootNode : RootNodes)
            {
                // if the node is a default scene root and the actor already has a root component, skip it
                // haker: you can think of two cases:
                // 1. if RootComponent is not nullptr, we can think of the SCS is already constructed
                // 2. if RootNode is new overriden root node rather than DefaultSceneRootNode:
                //    [ ] experiment it in the editor
                // 
                // in this case, you can not guarantee that how this condition works:
                // - you should experiment it by yourself to understand it further!
                if (RootNode && ((RootNode != DefaultSceneRootNode) || (RootComponent == nullptr)))
                {
                    // if the root node specifies that it has a parent
                    USceneComponent* ParentComponent = nullptr;

                    // haker: if RootNode is DefaultSceneRootNode, 'ParentComponrntOrVariableName' is NAME_None
                    // [ ] in Lyra project, Character_Default, BP, the RootNode is not DefaultSceneRootNode, it is 'CollisionCylinder'
                    if (RootNode->ParentComponentOrVariableName != NAME_None)
                    {
                        // get the Actor class object
                        UClass* ActorClass = Actor->GetClass();
                        check(ActorClass != nullptr);

                        // if the root node is parented to a "native" component (i.e. in the 'NativeSceneComponents' array)
                        // haker: here is why we need NativeSceneComponents, to find RootNode's component
                        // [ ] in Lyra project, Character_Default has its RootNode's component as NativeComponent:
                        //     - 'CollisionCylinder'
                        //     - 'CollisionCylinder' component is set in ACharacter: see the tooltip in BP Editor
                        //       - ACharacter::ACharacter() { ... RootComponent = CapsuleComponent; }
                        if (RootNode->bIsParentComponentNative)
                        {
                            for (USceneComponent* NativeSceneComponent : NativeSceneComponents)
                            {
                                // if we found a match, remember it
                                if (NativeSceneComponent->GetFName() == RootNode->ParentComponentOrVariableName)
                                {
                                    // haker: in Lyra project, we can see ParentComponentOrVariableName as 'CollisionCylinder'
                                    // - any Actor who inherits from ACharacter, will have exact same process
                                    ParentComponent = NativeSceneComponent;
                                    break;
                                }
                            }
                        }
                        else
                        {
                            // haker: it is hard to find the case, so skip it for now:
                            // - just try to understand the code itself

                            // in the non-native case, the SCS node's variable name property is used as the parent identifier
                            FObjectPropertyBase* Property = FindProperty<FObjectPropertyBase>(ActorClass, RootNode->ParentComponentOrVariableName);
                            if (Property)
                            {
                                // if we found a matching property, grab its value and use that as the parent for this node
                                ParentComponent = Cast<USceneComponent>(Property->GetObjectPropertyValue_InContainer(Actor));
                            }
                        }
                    }

                    // create the new component instance and any child components it may have
                    // haker: see USCS_Node::ExecuteNodeOnActor (goto 015: ExecuteConstruction)
                    RootNode->ExecuteNodeOnActor(Actor, ParentComponent != nullptr ? ParentComponent : RootComponent, &RootTransform, RootRelativeRotationCache, bIsDefaultTransform, TransformScaleMethod);
                }
            }
        }
        // must have a root component at the end of SCS, so if we don't have one already (from base class), create a SceneComponent now
        // haker: in this case, we don't have any RootNodes here, so create new one
        else if (Actor->GetRootComponent() == nullptr)
        {
            USceneComponent* SceneComp = NewObject<USceneComponent>(Actor);
            SceneComp->SetFlags(RF_Transactional);
            SceneComp->CreationMethod = EComponentCreationMethod::SimpleConstructionScript;
            if (RootRelativeRotationCache)
            {
                // enforce using the same rotator as much as possible
                SceneComp->SetRelativeRotationCache(*RootRelativeRotationCache);
            }
            SceneComp->SetWorldTransform(RootTransform);
            Actor->SetRootComponent(SceneComp);
            SceneComp->RegisterComponent();
        }
    }

    /** root nodes of the construction script */
    // haker: see USCS_Node (goto 007: ExecuteConstruction)
    TArray<TObjectPtr<class USCS_Node>> RootNodes;

    /** default scene root node; used when no other nodes are available to use as the root */
    // haker: it caches DefaultSceneRootNode to compare RootNodes
    TObjectPtr<class USCS_Node> DefaultSceneRootNode;

    /** reregister context which allows bulk handling of render commands */
    // haker: this is optimization technique, we'll skip it:
    // - briefly, it reduces the overehad of unregister/register updates toward rendering thread
    FStaticMeshComponentBulkReregisterContext* ReregisterContext;
};

// 002 - Foundation - ExecuteConstruction * - UBlueprintGeneratedClass
// haker: try to understand differences between UClass and UBlueprintGeneratedClass
// Diagram:
//                                                                                      
//  ┌──────────┐   With UCLASS() macro           ┌────────┐                             
//  │ Cpp Code ├─────────────────────────────────► UClass │                             
//  └──────────┘   UHT(UnrealHeaderTool)         └────────┘                             
//                 creates UClass instance                                              
//                                                                                      
//                                                                                      
//  ┌───────────┐                                ┌────────────────────────────┐         
//  │Asset      ├────────────────────────────────► UBlueprintGeneneratedClass │         
//  │(Blueprint)│    UBlueprint generates        └──▲─────────────────────────┘         
//  └───────────┘    UBlueprintGeneratedClass       │                                   
//                                                  ├───UAnimBlueprintGeneratedClass    
//                                                  │                                   
//                                                  └───UWidgetBlueprintGeneratedClass  
//                                                                                      
// see UBlueprintGeneratedClass member variables (goto 003: ExecuteConstruction)
class UBlueprintGeneratedClass : public UClass
{
    /** 
     * iterate over all BPGCs used to generate this class and its parents, calling function InFunc on them
     * first element in the BPGC used to generate InClass
     */
    // 010 - Foundation - ExecuteConstruction * - ForEachGeneratedClassHierarchy
    static bool ForEachGeneratedClassHierarchy(const UClass* InClass, TFunctionRef<bool(const UBlueprintGeneratedClass*)> InFunc)
    {
        // haker: iterating ParentClass, filtering BlueprintGeneratedClass
        bool bNoErrors = true;
        while (const UBlueprintGeneratedClass* BPGClass = Cast<const UBlueprintGeneratedClass>(InClass))
        {
            if (!InFunc(BPGClass))
            {
                return bNoErrors;
            }

            InClass = BPGClass->GetSuperClass();
        }
        return bNoErrors;
    }

    /** gets an array of all BPGeneratedClasses (including InClass as 0th element) parents of given generated class */
    // 009 - Foundation - ExecuteConstruction * - GetGeneratedClassesHierarchy
    static bool GetGeneratedClassesHierarchy(const UClass* InClass, TArray<const UBlueprintGeneratedClass*>& OutBPGClasses)
    {
        OutBPGClasses.Empty();
        // see ForEachGeneratedClassHierarchy (goto 010: ExecuteConstruction)
        return ForEachGeneratedClassHierarchy(InClass, [&OutBPGClasses](const UBlueprintGeneratedClass* BPGClass)
        {
            OutBPGClasses.Add(BPGClass);
            return true;
        });
    }

    UInheritableComponentHandler* GetInheritableComponentHandler(const bool bCreateIfNecessary=false)
    {
        static const FBoolConfigValueHelper EnableInheritableComponents(TEXT("Kismet"), TEXT("bEnableInheritableComponents"), GEngineIni);
        if (!EnableInheritableComponents)
        {
            return nullptr;
        }

        if (!InheritableComponentHandler && bCreateIfNecessary)
        {
            InheritableComponentHandler = NewObject<UInheritableComponentHandler>(this, FName(TEXT("InheritableComponentHandler")));
        }

        return InheritableComponentHandler;
    }

    // 003 - Foundation - ExecuteConstruction * - UBlueprintGeneratedClass's member variables

    /** stores data to override (in children classes) components (created by SCS) from parent classes */
    // see UInheritableComponentHandler (goto 004: ExecuteConstruction)
    TObjectPtr<class UInheritableComponentHandler> InheritableComponentHandler;

    /** 'simple' construction script - graph of components to instance */
    // haker: see USimpleConstructionScript (goto 006: ExecuteConstruction)
    TObjectPtr<class USimpleConstructionScript> SimpleConstructionScript;

    /** array of templates for timelines that should be created */
    TArray<TObjectPtr<class UTimelineTemplate>> Timelines;
};