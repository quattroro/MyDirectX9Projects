#nullable enable

using System.Text;

namespace UnrealCli.Protocol;

public enum CliCommandGroup
{
    EditorControl,
    AssetWorkflows,
    LevelWorkflows,
    BlueprintWorkflows,
    AnimationWorkflows,
    MaterialWorkflows,
    BehaviorTreeWorkflows,
    InstanceManagement,
    Diagnostics,
    PluginManagement,
}

public enum ForceRule
{
    None,
    OnOverwrite,
    OnDestructiveOp,
    Always,
}

public sealed class CliCommandDescriptor
{
    public CliCommandDescriptor(
        string command,
        string synopsis,
        string summary,
        CliCommandGroup group,
        string? protocolCommand,
        bool canUseLocal,
        bool canUseLive,
        bool isAllowedWhileBusy,
        ForceRule forceRule = ForceRule.None,
        int? defaultLiveTimeoutMs = null,
        string[]? notes = null)
    {
        Command = command;
        Synopsis = synopsis;
        Summary = summary;
        Group = group;
        ProtocolCommand = protocolCommand;
        CanUseLocal = canUseLocal;
        CanUseLive = canUseLive;
        IsAllowedWhileBusy = isAllowedWhileBusy;
        ForceRule = forceRule;
        DefaultLiveTimeoutMs = defaultLiveTimeoutMs;
        Notes = notes ?? Array.Empty<string>();
    }

    public string Command { get; }
    public string Synopsis { get; }
    public string Summary { get; }
    public CliCommandGroup Group { get; }
    public string? ProtocolCommand { get; }
    public bool CanUseLocal { get; }
    public bool CanUseLive { get; }
    public bool IsAllowedWhileBusy { get; }
    public ForceRule ForceRule { get; }
    public int? DefaultLiveTimeoutMs { get; }
    public string[] Notes { get; }
}

public static class CliCommandCatalog
{
    private static readonly CliCommandDescriptor[] Commands =
    [
        // Editor control
        new("status",
            "status",
            "Reports the selected project and live editor state when the bridge is reachable.",
            CliCommandGroup.EditorControl,
            ProtocolConstants.CommandStatus,
            canUseLocal: true, canUseLive: true, isAllowedWhileBusy: true),

        new("play",
            "play",
            "Starts Play-In-Editor (PIE) in the running editor.",
            CliCommandGroup.EditorControl,
            ProtocolConstants.CommandPlay,
            canUseLocal: false, canUseLive: true, isAllowedWhileBusy: false),

        new("pause",
            "pause",
            "Pauses PIE in the running editor.",
            CliCommandGroup.EditorControl,
            ProtocolConstants.CommandPause,
            canUseLocal: false, canUseLive: true, isAllowedWhileBusy: false),

        new("stop",
            "stop",
            "Stops PIE in the running editor.",
            CliCommandGroup.EditorControl,
            ProtocolConstants.CommandStop,
            canUseLocal: false, canUseLive: true, isAllowedWhileBusy: false),

        new("compile",
            "compile [--wait]",
            "Triggers a Hot Reload compile in the running editor.",
            CliCommandGroup.EditorControl,
            ProtocolConstants.CommandCompile,
            canUseLocal: false, canUseLive: true, isAllowedWhileBusy: false,
            defaultLiveTimeoutMs: ProtocolConstants.DefaultCompileTimeoutMs),

        new("refresh",
            "refresh [--wait]",
            "Reimports modified assets in the running editor.",
            CliCommandGroup.EditorControl,
            ProtocolConstants.CommandRefresh,
            canUseLocal: false, canUseLive: true, isAllowedWhileBusy: false),

        new("read-log",
            "read-log [--limit N] [--type log|warning|error]",
            "Reads recent Output Log entries from the running editor.",
            CliCommandGroup.EditorControl,
            ProtocolConstants.CommandReadLog,
            canUseLocal: false, canUseLive: true, isAllowedWhileBusy: true),

        new("screenshot",
            "screenshot [--viewport game|editor] [--path <output.png>] [--width N] [--height N]",
            "Captures a screenshot from the Game or Editor viewport.",
            CliCommandGroup.EditorControl,
            ProtocolConstants.CommandScreenshot,
            canUseLocal: false, canUseLive: true, isAllowedWhileBusy: false),

        new("execute-menu",
            "execute-menu (--path \"Menu/Item\" | --list \"Prefix\")",
            "Executes an Unreal menu item or lists registered menu items matching a prefix.",
            CliCommandGroup.EditorControl,
            ProtocolConstants.CommandExecuteMenu,
            canUseLocal: false, canUseLive: true, isAllowedWhileBusy: false),

        new("execute",
            "execute (--code <python> | --file <path.py>) [--args <json>] --force",
            "Executes arbitrary Python code in the running editor via the Python Script Plugin. Always requires --force.",
            CliCommandGroup.EditorControl,
            ProtocolConstants.CommandExecuteCode,
            canUseLocal: false, canUseLive: true, isAllowedWhileBusy: false,
            forceRule: ForceRule.Always,
            notes: ["Requires the Python Script Plugin to be enabled in the project.",
                    "--args JSON is exposed as the `args` variable in Python code.",
                    "Assign to the `result` variable to return structured data to the caller."]),

        // Asset workflows
        new("asset find",
            "asset find [--name <term>] [--type <type>] [--folder /Game/...] [--limit N]",
            "Finds assets by name and/or type, with an optional folder filter.",
            CliCommandGroup.AssetWorkflows,
            ProtocolConstants.CommandAssetFind,
            canUseLocal: false, canUseLive: true, isAllowedWhileBusy: true),

        new("asset info",
            "asset info --path /Game/...",
            "Reads asset metadata by content path.",
            CliCommandGroup.AssetWorkflows,
            ProtocolConstants.CommandAssetInfo,
            canUseLocal: false, canUseLive: true, isAllowedWhileBusy: true),

        new("asset move",
            "asset move --from /Game/... --to /Game/... [--force]",
            "Moves an asset to a new path; overwriting the destination requires --force.",
            CliCommandGroup.AssetWorkflows,
            ProtocolConstants.CommandAssetMove,
            canUseLocal: false, canUseLive: true, isAllowedWhileBusy: false,
            forceRule: ForceRule.OnOverwrite),

        new("asset rename",
            "asset rename --path /Game/... --name <newName> [--force]",
            "Renames an asset in place; overwriting the destination requires --force.",
            CliCommandGroup.AssetWorkflows,
            ProtocolConstants.CommandAssetRename,
            canUseLocal: false, canUseLive: true, isAllowedWhileBusy: false,
            forceRule: ForceRule.OnOverwrite),

        new("asset delete",
            "asset delete --path /Game/... --force",
            "Deletes an asset and always requires --force.",
            CliCommandGroup.AssetWorkflows,
            ProtocolConstants.CommandAssetDelete,
            canUseLocal: false, canUseLive: true, isAllowedWhileBusy: false,
            forceRule: ForceRule.Always),

        new("asset create",
            "asset create --type <kind> --path /Game/... [--force]",
            "Creates a new asset of the specified type; overwriting requires --force.",
            CliCommandGroup.AssetWorkflows,
            ProtocolConstants.CommandAssetCreate,
            canUseLocal: false, canUseLive: true, isAllowedWhileBusy: false,
            forceRule: ForceRule.OnOverwrite,
            notes: ["Types: material, texture, staticmesh, blueprint, dataasset, curve, soundcue"]),

        new("asset mkdir",
            "asset mkdir --path /Game/...",
            "Creates missing content folders.",
            CliCommandGroup.AssetWorkflows,
            ProtocolConstants.CommandAssetMkdir,
            canUseLocal: false, canUseLive: true, isAllowedWhileBusy: false),

        // Level workflows (equivalent to Scene in Unity)
        new("level open",
            "level open --path /Game/... [--force]",
            "Opens a level asset; use --force to discard unsaved changes.",
            CliCommandGroup.LevelWorkflows,
            ProtocolConstants.CommandLevelOpen,
            canUseLocal: false, canUseLive: true, isAllowedWhileBusy: false),

        new("level inspect",
            "level inspect [--path /Game/...] [--with-values] [--max-depth N]",
            "Inspects the level actor hierarchy. Omit --path to use the currently loaded level.",
            CliCommandGroup.LevelWorkflows,
            ProtocolConstants.CommandLevelInspect,
            canUseLocal: false, canUseLive: true, isAllowedWhileBusy: false),

        new("level add-actor",
            "level add-actor --class <ClassName> [--name <label>] [--location x,y,z] [--rotation p,y,r] [--scale x,y,z]",
            "Spawns a new actor in the current level.",
            CliCommandGroup.LevelWorkflows,
            ProtocolConstants.CommandLevelAddActor,
            canUseLocal: false, canUseLive: true, isAllowedWhileBusy: false,
            notes: ["Common classes: StaticMeshActor, PointLight, DirectionalLight, CameraActor, TriggerBox"]),

        new("level set-transform",
            "level set-transform --actor <label> [--location x,y,z] [--rotation p,y,r] [--scale x,y,z]",
            "Sets the transform of an actor in the current level.",
            CliCommandGroup.LevelWorkflows,
            ProtocolConstants.CommandLevelSetTransform,
            canUseLocal: false, canUseLive: true, isAllowedWhileBusy: false),

        new("level delete-actor",
            "level delete-actor --actor <label> --force",
            "Deletes an actor from the current level. Always requires --force.",
            CliCommandGroup.LevelWorkflows,
            ProtocolConstants.CommandLevelDeleteActor,
            canUseLocal: false, canUseLive: true, isAllowedWhileBusy: false,
            forceRule: ForceRule.Always),

        new("level list-components",
            "level list-components --actor <label>",
            "Lists all components attached to an actor in the current level.",
            CliCommandGroup.LevelWorkflows,
            ProtocolConstants.CommandLevelListComponents,
            canUseLocal: false, canUseLive: true, isAllowedWhileBusy: true),

        new("level add-component",
            "level add-component --actor <label> --type <ComponentType> [--values <json>]",
            "Adds a component to an actor in the current level.",
            CliCommandGroup.LevelWorkflows,
            ProtocolConstants.CommandLevelAddComponent,
            canUseLocal: false, canUseLive: true, isAllowedWhileBusy: false),

        new("level remove-component",
            "level remove-component --actor <label> --type <ComponentType> [--index N] --force",
            "Removes a component from an actor in the current level. Always requires --force.",
            CliCommandGroup.LevelWorkflows,
            ProtocolConstants.CommandLevelRemoveComponent,
            canUseLocal: false, canUseLive: true, isAllowedWhileBusy: false,
            forceRule: ForceRule.Always),

        new("level assign-material",
            "level assign-material --actor <label> --material /Game/... [--slot N]",
            "Assigns a material to a mesh component on an actor.",
            CliCommandGroup.LevelWorkflows,
            ProtocolConstants.CommandLevelAssignMaterial,
            canUseLocal: false, canUseLive: true, isAllowedWhileBusy: false),

        // Blueprint workflows (equivalent to Prefab in Unity)
        new("blueprint inspect",
            "blueprint inspect --path /Game/... [--with-values] [--max-depth N]",
            "Inspects a Blueprint asset's component hierarchy and default property values.",
            CliCommandGroup.BlueprintWorkflows,
            ProtocolConstants.CommandBlueprintInspect,
            canUseLocal: false, canUseLive: true, isAllowedWhileBusy: true),

        new("blueprint set-property",
            "blueprint set-property --path /Game/... --property <name> --value <json>",
            "Sets a default property value on a Blueprint Class Default Object.",
            CliCommandGroup.BlueprintWorkflows,
            ProtocolConstants.CommandBlueprintSetProperty,
            canUseLocal: false, canUseLive: true, isAllowedWhileBusy: false),

        // Animation workflows
        new("anim create-abp",
            "anim create-abp --skeleton /Game/... --path /Game/... [--force]",
            "Creates an Animation Blueprint asset for the given Skeleton.",
            CliCommandGroup.AnimationWorkflows,
            ProtocolConstants.CommandAnimCreateAbp,
            canUseLocal: false, canUseLive: true, isAllowedWhileBusy: false,
            forceRule: ForceRule.OnOverwrite),

        new("anim assign-abp",
            "anim assign-abp --bp /Game/... --anim-bp /Game/... [--component <name>]",
            "Assigns an Animation Blueprint to the SkeletalMeshComponent of a Blueprint.",
            CliCommandGroup.AnimationWorkflows,
            ProtocolConstants.CommandAnimAssignAbp,
            canUseLocal: false, canUseLive: true, isAllowedWhileBusy: false),

        new("anim list-states",
            "anim list-states --path /Game/...",
            "Lists state machines, states, and variables of an Animation Blueprint.",
            CliCommandGroup.AnimationWorkflows,
            ProtocolConstants.CommandAnimListStates,
            canUseLocal: false, canUseLive: true, isAllowedWhileBusy: true),

        new("anim add-variable",
            "anim add-variable --path /Game/... --name <varName> --type float|bool|int",
            "Adds a variable to an Animation Blueprint and recompiles it.",
            CliCommandGroup.AnimationWorkflows,
            ProtocolConstants.CommandAnimAddVariable,
            canUseLocal: false, canUseLive: true, isAllowedWhileBusy: false),

        new("anim play-montage",
            "anim play-montage --actor <label> --montage /Game/... [--rate <float>]",
            "Plays an Animation Montage on a live PIE actor.",
            CliCommandGroup.AnimationWorkflows,
            ProtocolConstants.CommandAnimPlayMontage,
            canUseLocal: false, canUseLive: true, isAllowedWhileBusy: false,
            notes: ["Requires an active PIE session."]),

        new("anim setup-statemachine",
            "anim setup-statemachine --path /Game/... --idle-anim /Game/... --walk-anim /Game/... [--walk-threshold <float>] [--force]",
            "Builds an Idle/Walk state machine in an Animation Blueprint, wired to a Speed variable, and recompiles it.",
            CliCommandGroup.AnimationWorkflows,
            ProtocolConstants.CommandAnimSetupStateMachine,
            canUseLocal: false, canUseLive: true, isAllowedWhileBusy: false,
            forceRule: ForceRule.OnOverwrite,
            notes: ["Adds a float 'Speed' variable if the Animation Blueprint doesn't already have one.",
                    "--walk-threshold is the Speed value the Idle<->Walk transitions compare against (default 10).",
                    "Recreating a state machine on an Animation Blueprint that already has one requires --force."]),

        // Material / shader workflows
        new("material create",
            "material create --path /Game/... [--domain <d>] [--blend <b>] [--shading <s>] [--two-sided] [--force] [--save]",
            "Creates a Material asset, optionally presetting domain, blend mode and shading model.",
            CliCommandGroup.MaterialWorkflows,
            ProtocolConstants.CommandMaterialCreate,
            canUseLocal: false, canUseLive: true, isAllowedWhileBusy: false,
            forceRule: ForceRule.OnOverwrite,
            notes: ["--domain: surface, deferreddecal, lightfunction, volume, postprocess, ui",
                    "--blend: opaque, masked, translucent, additive, modulate, alphacomposite, alphaholdout",
                    "--shading: unlit, defaultlit, subsurface, clearcoat, hair, cloth, eye, thintranslucent, ..."]),

        new("material inspect",
            "material inspect --path /Game/... [--with-values]",
            "Dumps the material's node graph: every node, its pin connections, and the material output pins.",
            CliCommandGroup.MaterialWorkflows,
            ProtocolConstants.CommandMaterialInspect,
            canUseLocal: false, canUseLive: true, isAllowedWhileBusy: true),

        new("material list-node-types",
            "material list-node-types [--filter <term>] [--limit N]",
            "Lists the material expression node types available in this engine build.",
            CliCommandGroup.MaterialWorkflows,
            ProtocolConstants.CommandMaterialListNodeTypes,
            canUseLocal: false, canUseLive: true, isAllowedWhileBusy: true),

        new("material add-node",
            "material add-node --path /Game/... --type <NodeType> [--name <id>] [--pos x,y] [--values <json>] [--no-compile] [--save]",
            "Adds one expression node to a material graph.",
            CliCommandGroup.MaterialWorkflows,
            ProtocolConstants.CommandMaterialAddNode,
            canUseLocal: false, canUseLive: true, isAllowedWhileBusy: false,
            notes: ["--name becomes the node's id (stored as its graph comment) for later connect/set-node calls.",
                    "--values sets node properties by name, e.g. {\"Texture\":\"/Game/T_Rock\",\"ParameterName\":\"Tint\"}"]),

        new("material set-node",
            "material set-node --path /Game/... --node <id> --values <json> [--pos x,y] [--no-compile] [--save]",
            "Sets properties on an existing node in a material graph.",
            CliCommandGroup.MaterialWorkflows,
            ProtocolConstants.CommandMaterialSetNode,
            canUseLocal: false, canUseLive: true, isAllowedWhileBusy: false),

        new("material connect",
            "material connect --path /Game/... --from <id> [--from-output <pin>] (--to <id> [--to-input <pin>] | --property <MaterialOutput>) [--no-compile] [--save]",
            "Connects a node output to another node's input, or to a material output pin such as BaseColor.",
            CliCommandGroup.MaterialWorkflows,
            ProtocolConstants.CommandMaterialConnect,
            canUseLocal: false, canUseLive: true, isAllowedWhileBusy: false,
            notes: ["--property: BaseColor, Metallic, Specular, Roughness, EmissiveColor, Opacity, OpacityMask, Normal, WorldPositionOffset, AmbientOcclusion, Refraction"]),

        new("material disconnect",
            "material disconnect --path /Game/... (--to <id> [--to-input <pin>] | --property <MaterialOutput>) [--no-compile] [--save]",
            "Clears whatever is plugged into a node input or a material output pin.",
            CliCommandGroup.MaterialWorkflows,
            ProtocolConstants.CommandMaterialDisconnect,
            canUseLocal: false, canUseLive: true, isAllowedWhileBusy: false),

        new("material delete-node",
            "material delete-node --path /Game/... --node <id> --force [--no-compile] [--save]",
            "Deletes a node from a material graph. Always requires --force.",
            CliCommandGroup.MaterialWorkflows,
            ProtocolConstants.CommandMaterialDeleteNode,
            canUseLocal: false, canUseLive: true, isAllowedWhileBusy: false,
            forceRule: ForceRule.Always),

        new("material set-property",
            "material set-property --path /Game/... (--property <name> --value <v> | --values <json> | --domain/--blend/--shading/--two-sided) [--save]",
            "Sets material-level settings (blend mode, shading model, two-sided, or any UMaterial property by name).",
            CliCommandGroup.MaterialWorkflows,
            ProtocolConstants.CommandMaterialSetProperty,
            canUseLocal: false, canUseLive: true, isAllowedWhileBusy: false),

        new("material apply-graph",
            "material apply-graph --path /Game/... (--graph <json> | --graph-file <path.json>) [--clear] [--layout] [--force] [--save]",
            "Builds a whole material graph in one call from a JSON description of nodes, connections and outputs.",
            CliCommandGroup.MaterialWorkflows,
            ProtocolConstants.CommandMaterialApplyGraph,
            canUseLocal: false, canUseLive: true, isAllowedWhileBusy: false,
            forceRule: ForceRule.OnOverwrite,
            notes: ["Graph JSON: {\"settings\":{...},\"nodes\":[{\"id\":\"tex\",\"type\":\"TextureSample\",\"values\":{...}}],"
                    + "\"connections\":[{\"from\":\"tex\",\"fromOutput\":\"RGB\",\"to\":\"mul\",\"toInput\":\"A\"}],"
                    + "\"outputs\":[{\"from\":\"mul\",\"property\":\"BaseColor\"}]}",
                    "--clear rebuilds the material from scratch; without it, applying to a non-empty material needs --force.",
                    "Node positions are laid out automatically unless nodes carry an explicit \"pos\"."]),

        new("material compile",
            "material compile --path /Game/... [--layout] [--save]",
            "Recompiles a material and optionally re-lays out its graph.",
            CliCommandGroup.MaterialWorkflows,
            ProtocolConstants.CommandMaterialCompile,
            canUseLocal: false, canUseLive: true, isAllowedWhileBusy: false,
            defaultLiveTimeoutMs: ProtocolConstants.DefaultCompileTimeoutMs),

        new("material create-instance",
            "material create-instance --path /Game/... --parent /Game/... [--force] [--save]",
            "Creates a Material Instance Constant asset with the given parent material.",
            CliCommandGroup.MaterialWorkflows,
            ProtocolConstants.CommandMaterialCreateInstance,
            canUseLocal: false, canUseLive: true, isAllowedWhileBusy: false,
            forceRule: ForceRule.OnOverwrite),

        new("material set-instance-param",
            "material set-instance-param --path /Game/... --name <param> --type scalar|vector|texture|switch --value <v> [--save]",
            "Overrides a parameter on a Material Instance.",
            CliCommandGroup.MaterialWorkflows,
            ProtocolConstants.CommandMaterialSetInstanceParam,
            canUseLocal: false, canUseLive: true, isAllowedWhileBusy: false,
            notes: ["vector values take [r,g,b,a]; texture values take a content path."]),

        // Behavior tree workflows
        new("bt create",
            "bt create --path /Game/... [--blackboard /Game/...] [--force] [--save]",
            "Creates a Behavior Tree asset, optionally bound to a Blackboard.",
            CliCommandGroup.BehaviorTreeWorkflows,
            ProtocolConstants.CommandBtCreate,
            canUseLocal: false, canUseLive: true, isAllowedWhileBusy: false,
            forceRule: ForceRule.OnOverwrite),

        new("bt inspect",
            "bt inspect --path /Game/... [--with-values]",
            "Dumps a behavior tree as nested JSON: nodes, decorators, services, children in execution order.",
            CliCommandGroup.BehaviorTreeWorkflows,
            ProtocolConstants.CommandBtInspect,
            canUseLocal: false, canUseLive: true, isAllowedWhileBusy: true,
            notes: ["The output is the same shape apply-graph takes, so a dump can be replayed onto another tree.",
                    "Nodes listed under \"detached\" sit in the graph but are not wired to the root, so they never run."]),

        new("bt list-node-types",
            "bt list-node-types [--kind task|composite|decorator|service] [--filter <term>] [--limit N]",
            "Searches the node classes a behavior tree can use, including Blueprint-derived ones.",
            CliCommandGroup.BehaviorTreeWorkflows,
            ProtocolConstants.CommandBtListNodeTypes,
            canUseLocal: false, canUseLive: true, isAllowedWhileBusy: true),

        new("bt add-node",
            "bt add-node --path /Game/... --type <Type> [--id <id>] [--parent <id>] [--index N] [--as main-task|background] [--values <json>] [--pos x,y] [--comment <text>] [--layout] [--save]",
            "Adds a composite or task node, optionally attaching it under a parent.",
            CliCommandGroup.BehaviorTreeWorkflows,
            ProtocolConstants.CommandBtAddNode,
            canUseLocal: false, canUseLive: true, isAllowedWhileBusy: false,
            notes: ["--id becomes the node's Node Name, which is how later commands refer to it.",
                    "--index picks the position among its siblings; sibling order is execution order.",
                    "Without --parent the node is created detached and will not run until you `bt connect` it.",
                    "--as picks the branch of a simple parallel: main-task or background."]),

        new("bt add-decorator",
            "bt add-decorator --path /Game/... --node <id> --type <Type> [--id <id>] [--index N] [--values <json>] [--save]",
            "Attaches a decorator to a node.",
            CliCommandGroup.BehaviorTreeWorkflows,
            ProtocolConstants.CommandBtAddDecorator,
            canUseLocal: false, canUseLive: true, isAllowedWhileBusy: false,
            notes: ["A blackboard key is given by name, e.g. --values {\"BlackboardKey\":\"TargetActor\"}."]),

        new("bt add-service",
            "bt add-service --path /Game/... --node <id> --type <Type> [--id <id>] [--index N] [--values <json>] [--save]",
            "Attaches a service to a composite node.",
            CliCommandGroup.BehaviorTreeWorkflows,
            ProtocolConstants.CommandBtAddService,
            canUseLocal: false, canUseLive: true, isAllowedWhileBusy: false),

        new("bt set-node",
            "bt set-node --path /Game/... --node <id> [--values <json>] [--id <newId>] [--pos x,y] [--comment <text>] [--save]",
            "Sets properties, renames, moves or comments an existing node.",
            CliCommandGroup.BehaviorTreeWorkflows,
            ProtocolConstants.CommandBtSetNode,
            canUseLocal: false, canUseLive: true, isAllowedWhileBusy: false),

        new("bt connect",
            "bt connect --path /Game/... --from <parentId> --to <childId> [--index N] [--as main-task|background] [--save]",
            "Makes one node the child of another, replacing whatever parent it had.",
            CliCommandGroup.BehaviorTreeWorkflows,
            ProtocolConstants.CommandBtConnect,
            canUseLocal: false, canUseLive: true, isAllowedWhileBusy: false,
            notes: ["Use --from Root to attach the top of the tree."]),

        new("bt disconnect",
            "bt disconnect --path /Game/... --to <childId> [--force] [--save]",
            "Detaches a node from its parent, taking its whole subtree out of the compiled tree.",
            CliCommandGroup.BehaviorTreeWorkflows,
            ProtocolConstants.CommandBtDisconnect,
            canUseLocal: false, canUseLive: true, isAllowedWhileBusy: false,
            forceRule: ForceRule.OnDestructiveOp),

        new("bt delete-node",
            "bt delete-node --path /Game/... --node <id> [--keep-children] --force [--save]",
            "Deletes a node, decorator or service.",
            CliCommandGroup.BehaviorTreeWorkflows,
            ProtocolConstants.CommandBtDeleteNode,
            canUseLocal: false, canUseLive: true, isAllowedWhileBusy: false,
            forceRule: ForceRule.Always,
            notes: ["--keep-children reattaches its children to its parent instead of leaving them detached."]),

        new("bt apply-graph",
            "bt apply-graph --path /Game/... (--graph <json> | --graph-file <path.json>) [--clear] [--force] [--save]",
            "Builds a whole behavior tree from one nested JSON description.",
            CliCommandGroup.BehaviorTreeWorkflows,
            ProtocolConstants.CommandBtApplyGraph,
            canUseLocal: false, canUseLive: true, isAllowedWhileBusy: false,
            forceRule: ForceRule.OnDestructiveOp,
            notes: ["Graph JSON: {\"blackboard\":\"/Game/...\",\"rootDecorators\":[...],"
                    + "\"root\":{\"type\":\"Selector\",\"id\":\"Brain\",\"services\":[...],\"decorators\":[...],"
                    + "\"children\":[{\"type\":\"Wait\",\"values\":{\"WaitTime\":2}}]}}",
                    "Array order is execution order; it is encoded as node X positions for you.",
                    "--clear replaces the tree; without it, applying to a non-empty tree needs --force.",
                    "Run `bt inspect --json` first if you want something to restore from — there is no undo.",
                    "The editor runs this on the game thread with a 30s budget; split trees past ~40 nodes."]),

        new("bt compile",
            "bt compile --path /Game/... [--layout] [--save]",
            "Recompiles the runtime tree from the graph and optionally re-lays out the nodes.",
            CliCommandGroup.BehaviorTreeWorkflows,
            ProtocolConstants.CommandBtCompile,
            canUseLocal: false, canUseLive: true, isAllowedWhileBusy: false),

        new("bt set-blackboard",
            "bt set-blackboard --path /Game/... --blackboard /Game/... [--save]",
            "Points a behavior tree at a Blackboard and re-resolves every key selector in it.",
            CliCommandGroup.BehaviorTreeWorkflows,
            ProtocolConstants.CommandBtSetBlackboard,
            canUseLocal: false, canUseLive: true, isAllowedWhileBusy: false),

        // Plugin management (equivalent to Package in Unity)
        new("plugin list",
            "plugin list",
            "Lists all plugins known to the current project (enabled and available).",
            CliCommandGroup.PluginManagement,
            ProtocolConstants.CommandPluginList,
            canUseLocal: false, canUseLive: true, isAllowedWhileBusy: true),

        new("plugin enable",
            "plugin enable --name <PluginName>",
            "Enables a plugin in the current project (requires editor restart).",
            CliCommandGroup.PluginManagement,
            ProtocolConstants.CommandPluginEnable,
            canUseLocal: false, canUseLive: true, isAllowedWhileBusy: false),

        new("plugin disable",
            "plugin disable --name <PluginName> --force",
            "Disables a plugin in the current project (requires editor restart). Always requires --force.",
            CliCommandGroup.PluginManagement,
            ProtocolConstants.CommandPluginDisable,
            canUseLocal: false, canUseLive: true, isAllowedWhileBusy: false,
            forceRule: ForceRule.Always),

        // Instance management
        new("instances list",
            "instances list",
            "Lists known Unreal project instances and the active registry selection.",
            CliCommandGroup.InstanceManagement,
            protocolCommand: null,
            canUseLocal: true, canUseLive: false, isAllowedWhileBusy: false),

        new("instances use",
            "instances use <projectHash|projectPath>",
            "Pins the active target project by hash or project path.",
            CliCommandGroup.InstanceManagement,
            protocolCommand: null,
            canUseLocal: true, canUseLive: false, isAllowedWhileBusy: false),

        // Diagnostics
        new("doctor",
            "doctor",
            "Shows registry, project detection, and live reachability diagnostics.",
            CliCommandGroup.Diagnostics,
            protocolCommand: null,
            canUseLocal: true, canUseLive: false, isAllowedWhileBusy: false),

        new("raw",
            "raw --json '{\"command\":\"status\"}' [--force]",
            "Sends a raw live protocol envelope for low-level debugging.",
            CliCommandGroup.Diagnostics,
            protocolCommand: null,
            canUseLocal: false, canUseLive: true, isAllowedWhileBusy: false),
    ];

    public static CliCommandDescriptor[] GetCommands() => (CliCommandDescriptor[])Commands.Clone();

    public static CliCommandDescriptor? FindByCommand(string command)
    {
        foreach (var descriptor in Commands)
            if (string.Equals(descriptor.Command, command, StringComparison.Ordinal))
                return descriptor;
        return null;
    }

    public static CliCommandDescriptor? FindByProtocolCommand(string command)
    {
        foreach (var descriptor in Commands)
            if (!string.IsNullOrWhiteSpace(descriptor.ProtocolCommand) &&
                string.Equals(descriptor.ProtocolCommand, command, StringComparison.Ordinal))
                return descriptor;
        return null;
    }

    public static bool IsCommandAllowedWhileBusy(string command)
    {
        if (string.Equals(command, ProtocolConstants.CommandPing, StringComparison.Ordinal))
            return true;
        var descriptor = FindByProtocolCommand(command);
        return descriptor?.IsAllowedWhileBusy ?? false;
    }

    public static string BuildHelpText()
    {
        var sb = new StringBuilder();
        sb.AppendLine("usage: unreal-cli [--json] [--output <default|json|compact>] [--project <path>] <command> [options]");
        sb.AppendLine();
        sb.AppendLine("options:");
        sb.AppendLine("  --json                Equivalent to --output json.");
        sb.AppendLine("  --output <mode>       Response format: default, json, or compact.");
        sb.AppendLine("  --project <path>      Target a specific Unreal project root directory.");
        sb.AppendLine();
        sb.AppendLine("commands:");
        foreach (var cmd in Commands)
        {
            sb.Append("  ");
            sb.AppendLine(cmd.Synopsis);
        }
        return sb.ToString();
    }
}
