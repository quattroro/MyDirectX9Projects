#nullable enable

using System.Text;

namespace UnrealCli.Protocol;

public enum CliCommandGroup
{
    EditorControl,
    AssetWorkflows,
    LevelWorkflows,
    BlueprintWorkflows,
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
