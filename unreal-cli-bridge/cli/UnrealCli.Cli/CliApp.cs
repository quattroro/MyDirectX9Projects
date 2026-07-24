#nullable enable

using System.Text.Json;
using UnrealCli.Cli.Models;
using UnrealCli.Cli.Services;
using UnrealCli.Protocol;

namespace UnrealCli.Cli;

public static class CliApp
{
    public static async Task<int> RunAsync(string[] args, CancellationToken cancellationToken)
    {
        var outputMode = CliArgumentParser.DetectOutputMode(args);

        ParsedCommand parsed;
        try
        {
            parsed = CliArgumentParser.Parse(args);
        }
        catch (CliUsageException ex)
        {
            Console.Error.WriteLine(ResponseFormatter.FormatError(ProtocolConstants.ErrorCliUsage, ex.Message, outputMode));
            return 1;
        }

        if (parsed.Kind == CommandKind.Help)
        {
            Console.Write(CliCommandCatalog.BuildHelpText());
            return 0;
        }

        var store = new InstanceRegistryStore();

        // Local-only commands
        switch (parsed.Kind)
        {
            case CommandKind.InstancesList:
                return HandleInstancesList(store, parsed.OutputMode);
            case CommandKind.InstancesUse:
                return HandleInstancesUse(store, parsed);
            case CommandKind.Doctor:
                return HandleDoctor(store, parsed.OutputMode);
            case CommandKind.Status when ShouldRunLocalStatus(store, parsed):
                return HandleLocalStatus(store, parsed.OutputMode);
        }

        // Live IPC commands
        return await HandleLiveCommandAsync(store, parsed, cancellationToken);
    }

    private static bool ShouldRunLocalStatus(InstanceRegistryStore store, ParsedCommand parsed)
    {
        try
        {
            store.ResolveTarget(parsed.ProjectOverride);
            return false;
        }
        catch
        {
            return true;
        }
    }

    private static async Task<int> HandleLiveCommandAsync(
        InstanceRegistryStore store, ParsedCommand parsed, CancellationToken cancellationToken)
    {
        InstanceRecord target;
        try
        {
            target = store.ResolveTarget(parsed.ProjectOverride)!;
        }
        catch (CliUsageException ex)
        {
            Console.Error.WriteLine(ResponseFormatter.FormatError(ProtocolConstants.ErrorCliUsage, ex.Message, parsed.OutputMode));
            return 1;
        }

        CommandEnvelope envelope;
        try
        {
            envelope = BuildEnvelope(parsed);
        }
        catch (Exception ex) when (ex is CliUsageException or JsonException or IOException)
        {
            Console.Error.WriteLine(ResponseFormatter.FormatError(ProtocolConstants.ErrorCliUsage, ex.Message, parsed.OutputMode));
            return 1;
        }

        var client = new LocalIpcClient();

        ResponseEnvelope response;
        try
        {
            response = await client.SendAsync(target, envelope, parsed.TimeoutMs, cancellationToken);
        }
        catch (OperationCanceledException)
        {
            Console.Error.WriteLine(ResponseFormatter.FormatError(ProtocolConstants.ErrorTimeout,
                $"Unreal 에디터 응답 없음 (timeout {parsed.TimeoutMs}ms). 에디터가 실행 중인지 확인하세요.", parsed.OutputMode));
            return 1;
        }
        catch (Exception ex)
        {
            Console.Error.WriteLine(ResponseFormatter.FormatError(ProtocolConstants.ErrorInternalError,
                $"IPC 통신 실패: {ex.Message}", parsed.OutputMode));
            return 1;
        }

        // compile / refresh --wait: poll until bridge is reachable again
        if (response.status == "ok" && parsed.Wait &&
            (parsed.Kind == CommandKind.Compile || parsed.Kind == CommandKind.Refresh))
        {
            await WaitForBridgeReadyAsync(store, parsed, target, cancellationToken);
        }

        Console.WriteLine(ResponseFormatter.Format(response, parsed.OutputMode));
        return response.status == "ok" ? 0 : 1;
    }

    private static CommandEnvelope BuildEnvelope(ParsedCommand parsed)
    {
        var args = new Dictionary<string, object?>();

        switch (parsed.Kind)
        {
            case CommandKind.ReadLog:
                if (parsed.LogLimit.HasValue) args["limit"] = parsed.LogLimit.Value;
                if (parsed.LogType != null) args["type"] = parsed.LogType;
                break;
            case CommandKind.Screenshot:
                if (parsed.ScreenshotViewport != null) args["viewport"] = parsed.ScreenshotViewport;
                if (parsed.ScreenshotPath != null) args["path"] = parsed.ScreenshotPath;
                if (parsed.ScreenshotWidth.HasValue) args["width"] = parsed.ScreenshotWidth.Value;
                if (parsed.ScreenshotHeight.HasValue) args["height"] = parsed.ScreenshotHeight.Value;
                break;
            case CommandKind.ExecuteCode:
                if (parsed.ExecuteCodeSnippet != null) args["code"] = parsed.ExecuteCodeSnippet;
                else if (parsed.ExecuteCodeFile != null) args["code"] = File.ReadAllText(parsed.ExecuteCodeFile);
                if (parsed.ExecuteCodeArgsJson != null) args["argsJson"] = parsed.ExecuteCodeArgsJson;
                break;
            case CommandKind.ExecuteMenu:
                if (parsed.MenuList) { args["list"] = true; args["prefix"] = parsed.MenuListPrefix ?? ""; }
                else args["path"] = parsed.MenuPath ?? "";
                break;
            case CommandKind.AssetFind:
                if (parsed.AssetName != null) args["name"] = parsed.AssetName;
                if (parsed.AssetType != null) args["type"] = parsed.AssetType;
                if (parsed.AssetFolder != null) args["folder"] = parsed.AssetFolder;
                if (parsed.AssetLimit.HasValue) args["limit"] = parsed.AssetLimit.Value;
                break;
            case CommandKind.AssetInfo:
                args["path"] = parsed.AssetPath ?? "";
                break;
            case CommandKind.AssetMove:
                args["from"] = parsed.AssetFrom ?? "";
                args["to"] = parsed.AssetTo ?? "";
                break;
            case CommandKind.AssetRename:
                args["path"] = parsed.AssetPath ?? "";
                args["name"] = parsed.AssetNewName ?? "";
                break;
            case CommandKind.AssetDelete:
                args["path"] = parsed.AssetPath ?? "";
                break;
            case CommandKind.AssetCreate:
                args["type"] = parsed.AssetCreateType ?? "";
                args["path"] = parsed.AssetPath ?? "";
                if (parsed.AssetDataJson != null) args["dataJson"] = parsed.AssetDataJson;
                break;
            case CommandKind.AssetMkdir:
                args["path"] = parsed.AssetPath ?? "";
                break;
            case CommandKind.LevelOpen:
                if (parsed.LevelPath != null) args["path"] = parsed.LevelPath;
                break;
            case CommandKind.LevelInspect:
                if (parsed.LevelPath != null) args["path"] = parsed.LevelPath;
                if (parsed.LevelWithValues) args["withValues"] = true;
                if (parsed.MaxDepth.HasValue) args["maxDepth"] = parsed.MaxDepth.Value;
                break;
            case CommandKind.LevelAddActor:
                args["class"] = parsed.ActorClass ?? "";
                if (parsed.ActorLabel != null) args["name"] = parsed.ActorLabel;
                if (parsed.ActorLocation != null) args["location"] = parsed.ActorLocation;
                if (parsed.ActorRotation != null) args["rotation"] = parsed.ActorRotation;
                if (parsed.ActorScale != null) args["scale"] = parsed.ActorScale;
                break;
            case CommandKind.LevelSetTransform:
                args["actor"] = parsed.ActorLabel ?? "";
                if (parsed.ActorLocation != null) args["location"] = parsed.ActorLocation;
                if (parsed.ActorRotation != null) args["rotation"] = parsed.ActorRotation;
                if (parsed.ActorScale != null) args["scale"] = parsed.ActorScale;
                break;
            case CommandKind.LevelDeleteActor:
                args["actor"] = parsed.ActorLabel ?? "";
                break;
            case CommandKind.LevelListComponents:
                args["actor"] = parsed.ActorLabel ?? "";
                break;
            case CommandKind.LevelAddComponent:
                args["actor"] = parsed.ActorLabel ?? "";
                args["type"] = parsed.ComponentType ?? "";
                if (parsed.ComponentValuesJson != null) args["values"] = JsonSerializer.Deserialize<object>(parsed.ComponentValuesJson);
                break;
            case CommandKind.LevelRemoveComponent:
                args["actor"] = parsed.ActorLabel ?? "";
                args["type"] = parsed.ComponentType ?? "";
                if (parsed.ComponentIndex.HasValue) args["index"] = parsed.ComponentIndex.Value;
                break;
            case CommandKind.LevelAssignMaterial:
                args["actor"] = parsed.ActorLabel ?? "";
                args["material"] = parsed.MaterialPath ?? "";
                if (parsed.MaterialSlot.HasValue) args["slot"] = parsed.MaterialSlot.Value;
                break;
            case CommandKind.BlueprintInspect:
                args["path"] = parsed.BlueprintPath ?? "";
                if (parsed.BlueprintWithValues) args["withValues"] = true;
                if (parsed.MaxDepth.HasValue) args["maxDepth"] = parsed.MaxDepth.Value;
                break;
            case CommandKind.BlueprintSetProperty:
                args["path"] = parsed.BlueprintPath ?? "";
                args["property"] = parsed.BlueprintProperty ?? "";
                args["value"] = parsed.BlueprintValue ?? "";
                break;
            case CommandKind.AnimCreateAbp:
                args["skeleton"] = parsed.AnimSkeletonPath ?? "";
                args["path"]     = parsed.AnimPath ?? "";
                break;
            case CommandKind.AnimAssignAbp:
                args["bp"]     = parsed.AnimTargetBpPath ?? "";
                args["animBp"] = parsed.AnimBlueprintPath ?? "";
                if (parsed.AnimComponentName != null) args["component"] = parsed.AnimComponentName;
                break;
            case CommandKind.AnimListStates:
                args["path"] = parsed.AnimPath ?? "";
                break;
            case CommandKind.AnimAddVariable:
                args["path"] = parsed.AnimPath ?? "";
                args["name"] = parsed.AnimVariableName ?? "";
                args["type"] = parsed.AnimVariableType ?? "";
                break;
            case CommandKind.AnimPlayMontage:
                args["actor"]   = parsed.AnimActorLabel ?? "";
                args["montage"] = parsed.AnimMontagePath ?? "";
                if (parsed.AnimPlayRate.HasValue) args["rate"] = parsed.AnimPlayRate.Value;
                break;
            case CommandKind.AnimSetupStateMachine:
                args["path"]     = parsed.AnimPath ?? "";
                args["idleAnim"] = parsed.AnimIdleAnimPath ?? "";
                args["walkAnim"] = parsed.AnimWalkAnimPath ?? "";
                if (parsed.AnimWalkThreshold.HasValue) args["walkThreshold"] = parsed.AnimWalkThreshold.Value;
                break;
            case CommandKind.MaterialCreate:
                args["path"] = parsed.MatPath ?? "";
                AddMaterialPreset(args, parsed);
                break;
            case CommandKind.MaterialInspect:
                args["path"] = parsed.MatPath ?? "";
                if (parsed.MatWithValues) args["withValues"] = true;
                break;
            case CommandKind.MaterialListNodeTypes:
                if (parsed.MatFilter != null) args["filter"] = parsed.MatFilter;
                if (parsed.MatLimit.HasValue) args["limit"] = parsed.MatLimit.Value;
                break;
            case CommandKind.MaterialAddNode:
                args["path"] = parsed.MatPath ?? "";
                args["type"] = parsed.MatNodeType ?? "";
                if (parsed.MatNodeId != null) args["name"] = parsed.MatNodeId;
                AddMaterialPos(args, parsed.MatPos);
                AddMaterialValues(args, parsed.MatValuesJson);
                break;
            case CommandKind.MaterialSetNode:
                args["path"] = parsed.MatPath ?? "";
                args["node"] = parsed.MatNodeId ?? "";
                AddMaterialPos(args, parsed.MatPos);
                AddMaterialValues(args, parsed.MatValuesJson);
                break;
            case CommandKind.MaterialConnect:
                args["path"] = parsed.MatPath ?? "";
                args["from"] = parsed.MatFrom ?? "";
                if (parsed.MatFromOutput != null) args["fromOutput"] = parsed.MatFromOutput;
                if (parsed.MatTo != null) args["to"] = parsed.MatTo;
                if (parsed.MatToInput != null) args["toInput"] = parsed.MatToInput;
                if (parsed.MatProperty != null) args["property"] = parsed.MatProperty;
                break;
            case CommandKind.MaterialDisconnect:
                args["path"] = parsed.MatPath ?? "";
                if (parsed.MatTo != null) args["to"] = parsed.MatTo;
                if (parsed.MatToInput != null) args["toInput"] = parsed.MatToInput;
                if (parsed.MatProperty != null) args["property"] = parsed.MatProperty;
                break;
            case CommandKind.MaterialDeleteNode:
                args["path"] = parsed.MatPath ?? "";
                args["node"] = parsed.MatNodeId ?? "";
                break;
            case CommandKind.MaterialSetProperty:
                args["path"] = parsed.MatPath ?? "";
                if (parsed.MatProperty != null) args["property"] = parsed.MatProperty;
                if (parsed.MatValue != null) args["value"] = parsed.MatValue;
                AddMaterialValues(args, parsed.MatValuesJson);
                AddMaterialPreset(args, parsed);
                break;
            case CommandKind.MaterialApplyGraph:
                args["path"] = parsed.MatPath ?? "";
                args["graph"] = ReadGraphJson(parsed);
                if (parsed.MatClear) args["clear"] = true;
                if (parsed.MatLayout) args["layout"] = true;
                break;
            case CommandKind.MaterialCompile:
                args["path"] = parsed.MatPath ?? "";
                if (parsed.MatLayout) args["layout"] = true;
                break;
            case CommandKind.MaterialCreateInstance:
                args["path"] = parsed.MatPath ?? "";
                args["parent"] = parsed.MatParent ?? "";
                break;
            case CommandKind.MaterialSetInstanceParam:
                args["path"] = parsed.MatPath ?? "";
                args["name"] = parsed.MatParamName ?? "";
                args["type"] = parsed.MatParamType ?? "";
                args["value"] = ParseScalarOrJson(parsed.MatValue ?? "");
                break;
            case CommandKind.PluginEnable:
                args["name"] = parsed.PluginName ?? "";
                break;
            case CommandKind.PluginDisable:
                args["name"] = parsed.PluginName ?? "";
                break;
            case CommandKind.Raw:
                var rawEnv = ProtocolJson.Deserialize<CommandEnvelope>(parsed.RawJson!)!;
                if (parsed.Force) rawEnv.force = true;
                return rawEnv;
        }

        // Material commands share these two switches.
        if (parsed.MatSave) args["save"] = true;
        if (parsed.MatNoCompile) args["noCompile"] = true;

        return new CommandEnvelope
        {
            command = GetProtocolCommand(parsed.Kind),
            arguments = args.Count > 0 ? args : null,
            force = parsed.Force,
        };
    }

    private static void AddMaterialPreset(Dictionary<string, object?> args, ParsedCommand parsed)
    {
        if (parsed.MatDomain != null) args["domain"] = parsed.MatDomain;
        if (parsed.MatBlend != null) args["blend"] = parsed.MatBlend;
        if (parsed.MatShading != null) args["shading"] = parsed.MatShading;
        if (parsed.MatTwoSided) args["twoSided"] = true;
    }

    // "--pos -600,120" -> [-600, 120]
    private static void AddMaterialPos(Dictionary<string, object?> args, string? pos)
    {
        if (pos == null) return;
        var parts = pos.Split(',', StringSplitOptions.TrimEntries);
        if (parts.Length != 2 || !int.TryParse(parts[0], out int x) || !int.TryParse(parts[1], out int y))
            throw new CliUsageException($"`--pos`는 `x,y` 형식이어야 합니다. 입력값: {pos}");
        args["pos"] = new[] { x, y };
    }

    private static void AddMaterialValues(Dictionary<string, object?> args, string? valuesJson)
    {
        if (valuesJson == null) return;
        try
        {
            args["values"] = JsonSerializer.Deserialize<JsonElement>(valuesJson);
        }
        catch (JsonException ex)
        {
            throw new CliUsageException($"`--values` JSON을 읽을 수 없습니다: {ex.Message}");
        }
    }

    private static JsonElement ReadGraphJson(ParsedCommand parsed)
    {
        string json;
        if (parsed.MatGraphFile != null)
        {
            if (!File.Exists(parsed.MatGraphFile))
                throw new CliUsageException($"그래프 파일을 찾을 수 없습니다: {parsed.MatGraphFile}");
            json = File.ReadAllText(parsed.MatGraphFile);
        }
        else
        {
            json = parsed.MatGraphJson ?? "";
        }

        try
        {
            return JsonSerializer.Deserialize<JsonElement>(json);
        }
        catch (JsonException ex)
        {
            throw new CliUsageException($"그래프 JSON을 읽을 수 없습니다: {ex.Message}");
        }
    }

    // Numbers/bools/arrays are sent as JSON; anything else (asset paths, names) as a string.
    private static object ParseScalarOrJson(string value)
    {
        try
        {
            return JsonSerializer.Deserialize<JsonElement>(value);
        }
        catch (JsonException)
        {
            return value;
        }
    }

    private static string GetProtocolCommand(CommandKind kind) => kind switch
    {
        CommandKind.Status => ProtocolConstants.CommandStatus,
        CommandKind.Play => ProtocolConstants.CommandPlay,
        CommandKind.Pause => ProtocolConstants.CommandPause,
        CommandKind.Stop => ProtocolConstants.CommandStop,
        CommandKind.Compile => ProtocolConstants.CommandCompile,
        CommandKind.Refresh => ProtocolConstants.CommandRefresh,
        CommandKind.ReadLog => ProtocolConstants.CommandReadLog,
        CommandKind.Screenshot => ProtocolConstants.CommandScreenshot,
        CommandKind.ExecuteCode => ProtocolConstants.CommandExecuteCode,
        CommandKind.ExecuteMenu => ProtocolConstants.CommandExecuteMenu,
        CommandKind.AssetFind => ProtocolConstants.CommandAssetFind,
        CommandKind.AssetInfo => ProtocolConstants.CommandAssetInfo,
        CommandKind.AssetMove => ProtocolConstants.CommandAssetMove,
        CommandKind.AssetRename => ProtocolConstants.CommandAssetRename,
        CommandKind.AssetDelete => ProtocolConstants.CommandAssetDelete,
        CommandKind.AssetCreate => ProtocolConstants.CommandAssetCreate,
        CommandKind.AssetMkdir => ProtocolConstants.CommandAssetMkdir,
        CommandKind.LevelOpen => ProtocolConstants.CommandLevelOpen,
        CommandKind.LevelInspect => ProtocolConstants.CommandLevelInspect,
        CommandKind.LevelAddActor => ProtocolConstants.CommandLevelAddActor,
        CommandKind.LevelSetTransform => ProtocolConstants.CommandLevelSetTransform,
        CommandKind.LevelDeleteActor => ProtocolConstants.CommandLevelDeleteActor,
        CommandKind.LevelListComponents => ProtocolConstants.CommandLevelListComponents,
        CommandKind.LevelAddComponent => ProtocolConstants.CommandLevelAddComponent,
        CommandKind.LevelRemoveComponent => ProtocolConstants.CommandLevelRemoveComponent,
        CommandKind.LevelAssignMaterial => ProtocolConstants.CommandLevelAssignMaterial,
        CommandKind.BlueprintInspect => ProtocolConstants.CommandBlueprintInspect,
        CommandKind.BlueprintSetProperty => ProtocolConstants.CommandBlueprintSetProperty,
        CommandKind.AnimCreateAbp        => ProtocolConstants.CommandAnimCreateAbp,
        CommandKind.AnimAssignAbp        => ProtocolConstants.CommandAnimAssignAbp,
        CommandKind.AnimListStates       => ProtocolConstants.CommandAnimListStates,
        CommandKind.AnimAddVariable      => ProtocolConstants.CommandAnimAddVariable,
        CommandKind.AnimPlayMontage      => ProtocolConstants.CommandAnimPlayMontage,
        CommandKind.AnimSetupStateMachine => ProtocolConstants.CommandAnimSetupStateMachine,
        CommandKind.MaterialCreate           => ProtocolConstants.CommandMaterialCreate,
        CommandKind.MaterialInspect          => ProtocolConstants.CommandMaterialInspect,
        CommandKind.MaterialListNodeTypes    => ProtocolConstants.CommandMaterialListNodeTypes,
        CommandKind.MaterialAddNode          => ProtocolConstants.CommandMaterialAddNode,
        CommandKind.MaterialSetNode          => ProtocolConstants.CommandMaterialSetNode,
        CommandKind.MaterialConnect          => ProtocolConstants.CommandMaterialConnect,
        CommandKind.MaterialDisconnect       => ProtocolConstants.CommandMaterialDisconnect,
        CommandKind.MaterialDeleteNode       => ProtocolConstants.CommandMaterialDeleteNode,
        CommandKind.MaterialSetProperty      => ProtocolConstants.CommandMaterialSetProperty,
        CommandKind.MaterialApplyGraph       => ProtocolConstants.CommandMaterialApplyGraph,
        CommandKind.MaterialCompile          => ProtocolConstants.CommandMaterialCompile,
        CommandKind.MaterialCreateInstance   => ProtocolConstants.CommandMaterialCreateInstance,
        CommandKind.MaterialSetInstanceParam => ProtocolConstants.CommandMaterialSetInstanceParam,
        CommandKind.PluginList => ProtocolConstants.CommandPluginList,
        CommandKind.PluginEnable => ProtocolConstants.CommandPluginEnable,
        CommandKind.PluginDisable => ProtocolConstants.CommandPluginDisable,
        _ => throw new InvalidOperationException($"No protocol command for {kind}"),
    };

    private static int HandleInstancesList(InstanceRegistryStore store, OutputMode outputMode)
    {
        var registry = store.Load();
        var output = new
        {
            activeProjectRoot = registry.activeProjectRoot,
            instances = registry.instances.Select(i => new
            {
                i.projectRoot,
                i.projectHash,
                i.pipeName,
                i.pid,
                i.startedAt,
                i.engineVersion,
                i.projectName,
                alive = IsProcessAlive(i.pid),
            }).ToArray(),
        };
        Console.WriteLine(outputMode == OutputMode.Default
            ? ProtocolJson.Serialize(output, pretty: true)
            : ProtocolJson.Serialize(output));
        return 0;
    }

    private static int HandleInstancesUse(InstanceRegistryStore store, ParsedCommand parsed)
    {
        if (parsed.InstanceTarget == null)
        {
            Console.Error.WriteLine("instances use에는 프로젝트 경로 또는 해시가 필요합니다.");
            return 1;
        }
        store.SetActiveProject(parsed.InstanceTarget);
        Console.WriteLine($"활성 프로젝트가 설정되었습니다: {parsed.InstanceTarget}");
        return 0;
    }

    private static int HandleDoctor(InstanceRegistryStore store, OutputMode outputMode)
    {
        var registry = store.Load();
        var live = registry.instances.Where(i => IsProcessAlive(i.pid)).ToArray();

        var report = new
        {
            cwd = Directory.GetCurrentDirectory(),
            activeProjectRoot = registry.activeProjectRoot,
            liveInstances = live.Length,
            instances = registry.instances.Select(i => new
            {
                i.projectRoot,
                i.projectHash,
                i.pipeName,
                i.pid,
                alive = IsProcessAlive(i.pid),
            }).ToArray(),
        };

        Console.WriteLine(ProtocolJson.Serialize(report, pretty: true));
        return 0;
    }

    private static int HandleLocalStatus(InstanceRegistryStore store, OutputMode outputMode)
    {
        var registry = store.Load();
        var result = new
        {
            status = "offline",
            message = "실행 중인 Unreal 에디터 인스턴스가 없습니다.",
            activeProjectRoot = registry.activeProjectRoot,
        };
        Console.WriteLine(ResponseFormatter.Format(ResponseEnvelope.Success(
            Guid.NewGuid().ToString("N"), result, 0), outputMode));
        return 0;
    }

    private static async Task WaitForBridgeReadyAsync(
        InstanceRegistryStore store, ParsedCommand parsed, InstanceRecord target, CancellationToken ct)
    {
        var client = new LocalIpcClient();
        var pingEnv = new CommandEnvelope { command = ProtocolConstants.CommandPing };
        var deadline = DateTime.UtcNow.AddMilliseconds(ProtocolConstants.DefaultCompileTimeoutMs);

        while (DateTime.UtcNow < deadline && !ct.IsCancellationRequested)
        {
            await Task.Delay(2000, ct);
            try
            {
                var pong = await client.SendAsync(target, pingEnv, 5000, ct);
                if (pong.status == "ok") return;
            }
            catch { }
        }
    }

    private static bool IsProcessAlive(int pid)
    {
        try
        {
            using var p = System.Diagnostics.Process.GetProcessById(pid);
            return !p.HasExited;
        }
        catch { return false; }
    }
}
