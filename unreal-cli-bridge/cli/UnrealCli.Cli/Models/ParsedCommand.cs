#nullable enable

using UnrealCli.Cli.Models;

namespace UnrealCli.Cli.Models;

public enum CommandKind
{
    Help,
    Status,
    Play,
    Pause,
    Stop,
    Compile,
    Refresh,
    ReadLog,
    Screenshot,
    ExecuteCode,
    ExecuteMenu,
    AssetFind,
    AssetInfo,
    AssetMove,
    AssetRename,
    AssetDelete,
    AssetCreate,
    AssetMkdir,
    LevelOpen,
    LevelInspect,
    LevelAddActor,
    LevelSetTransform,
    LevelDeleteActor,
    LevelListComponents,
    LevelAddComponent,
    LevelRemoveComponent,
    LevelAssignMaterial,
    BlueprintInspect,
    BlueprintSetProperty,
    AnimCreateAbp,
    AnimAssignAbp,
    AnimListStates,
    AnimAddVariable,
    AnimPlayMontage,
    AnimSetupStateMachine,
    PluginList,
    PluginEnable,
    PluginDisable,
    InstancesList,
    InstancesUse,
    Doctor,
    Raw,
}

public sealed class ParsedCommand
{
    public ParsedCommand(CommandKind kind) { Kind = kind; }

    public CommandKind Kind { get; }
    public OutputMode OutputMode { get; set; } = OutputMode.Default;
    public string? ProjectOverride { get; set; }
    public int TimeoutMs { get; set; } = UnrealCli.Protocol.ProtocolConstants.DefaultLiveTimeoutMs;
    public bool Force { get; set; }
    public bool Wait { get; set; }

    // status / instances
    public string? InstanceTarget { get; set; }

    // read-log
    public int? LogLimit { get; set; }
    public string? LogType { get; set; }

    // screenshot
    public string? ScreenshotViewport { get; set; }
    public string? ScreenshotPath { get; set; }
    public int? ScreenshotWidth { get; set; }
    public int? ScreenshotHeight { get; set; }

    // execute
    public string? ExecuteCodeSnippet { get; set; }
    public string? ExecuteCodeFile { get; set; }
    public string? ExecuteCodeArgsJson { get; set; }

    // execute-menu
    public string? MenuPath { get; set; }
    public bool MenuList { get; set; }
    public string? MenuListPrefix { get; set; }

    // asset
    public string? AssetName { get; set; }
    public string? AssetType { get; set; }
    public string? AssetFolder { get; set; }
    public int? AssetLimit { get; set; }
    public string? AssetPath { get; set; }
    public string? AssetFrom { get; set; }
    public string? AssetTo { get; set; }
    public string? AssetNewName { get; set; }
    public string? AssetCreateType { get; set; }
    public string? AssetDataJson { get; set; }

    // level
    public string? LevelPath { get; set; }
    public bool LevelWithValues { get; set; }
    public int? MaxDepth { get; set; }
    public string? ActorLabel { get; set; }
    public string? ActorClass { get; set; }
    public string? ActorLocation { get; set; }
    public string? ActorRotation { get; set; }
    public string? ActorScale { get; set; }
    public string? ComponentType { get; set; }
    public int? ComponentIndex { get; set; }
    public string? ComponentValuesJson { get; set; }
    public string? MaterialPath { get; set; }
    public int? MaterialSlot { get; set; }

    // blueprint
    public string? BlueprintPath { get; set; }
    public bool BlueprintWithValues { get; set; }
    public string? BlueprintProperty { get; set; }
    public string? BlueprintValue { get; set; }

    // anim
    public string? AnimSkeletonPath { get; set; }
    public string? AnimPath { get; set; }
    public string? AnimBlueprintPath { get; set; }
    public string? AnimTargetBpPath { get; set; }
    public string? AnimComponentName { get; set; }
    public string? AnimVariableName { get; set; }
    public string? AnimVariableType { get; set; }
    public string? AnimActorLabel { get; set; }
    public string? AnimMontagePath { get; set; }
    public double? AnimPlayRate { get; set; }
    // anim setup-statemachine
    public string? AnimIdleAnimPath { get; set; }
    public string? AnimWalkAnimPath { get; set; }
    public double? AnimWalkThreshold { get; set; }

    // plugin
    public string? PluginName { get; set; }

    // raw
    public string? RawJson { get; set; }
}
