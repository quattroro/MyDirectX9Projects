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

        CommandEnvelope envelope = BuildEnvelope(parsed);
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

        return new CommandEnvelope
        {
            command = GetProtocolCommand(parsed.Kind),
            arguments = args.Count > 0 ? args : null,
            force = parsed.Force,
        };
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
