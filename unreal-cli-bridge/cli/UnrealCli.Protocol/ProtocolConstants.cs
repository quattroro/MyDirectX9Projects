#nullable enable

namespace UnrealCli.Protocol;

public static class ProtocolConstants
{
    public const string ProtocolVersion = "1.0.0";
    public const string PipePrefix = "unreal-cli-";
    public const int DefaultLiveTimeoutMs = 30_000;
    public const int DefaultCompileTimeoutMs = 120_000;

    // Core commands
    public const string CommandPing = "ping";
    public const string CommandStatus = "status";
    public const string CommandPlay = "play";
    public const string CommandPause = "pause";
    public const string CommandStop = "stop";
    public const string CommandCompile = "compile";
    public const string CommandRefresh = "refresh";
    public const string CommandReadLog = "read-log";
    public const string CommandScreenshot = "screenshot";
    public const string CommandExecuteCode = "execute";
    public const string CommandExecuteMenu = "execute-menu";

    // Asset commands
    public const string CommandAssetFind = "asset.find";
    public const string CommandAssetInfo = "asset.info";
    public const string CommandAssetMove = "asset.move";
    public const string CommandAssetRename = "asset.rename";
    public const string CommandAssetDelete = "asset.delete";
    public const string CommandAssetCreate = "asset.create";
    public const string CommandAssetMkdir = "asset.mkdir";

    // Level commands (equivalent to Scene in Unity)
    public const string CommandLevelOpen = "level.open";
    public const string CommandLevelInspect = "level.inspect";
    public const string CommandLevelAddActor = "level.add-actor";
    public const string CommandLevelSetTransform = "level.set-transform";
    public const string CommandLevelDeleteActor = "level.delete-actor";
    public const string CommandLevelListComponents = "level.list-components";
    public const string CommandLevelAddComponent = "level.add-component";
    public const string CommandLevelRemoveComponent = "level.remove-component";
    public const string CommandLevelAssignMaterial = "level.assign-material";

    // Blueprint commands (equivalent to Prefab in Unity)
    public const string CommandBlueprintInspect = "blueprint.inspect";
    public const string CommandBlueprintSetProperty = "blueprint.set-property";

    // Plugin commands (equivalent to Package in Unity)
    public const string CommandPluginList = "plugin.list";
    public const string CommandPluginEnable = "plugin.enable";
    public const string CommandPluginDisable = "plugin.disable";

    // Error codes
    public const string ErrorProtocolMismatch = "PROTOCOL_MISMATCH";
    public const string ErrorCliUsage = "CLI_USAGE";
    public const string ErrorEditorBusy = "EDITOR_BUSY";
    public const string ErrorNotFound = "NOT_FOUND";
    public const string ErrorForceRequired = "FORCE_REQUIRED";
    public const string ErrorOperationFailed = "OPERATION_FAILED";
    public const string ErrorPythonUnavailable = "PYTHON_UNAVAILABLE";
    public const string ErrorPlaymodeActive = "PLAYMODE_ACTIVE";
    public const string ErrorPlaymodeNotActive = "PLAYMODE_NOT_ACTIVE";
    public const string ErrorTimeout = "TIMEOUT";
    public const string ErrorInternalError = "INTERNAL_ERROR";

    // Transport
    public const string TransportLive = "live";
    public const string TransportLocal = "local";
}
