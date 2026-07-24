#nullable enable

using UnrealCli.Cli.Models;
using UnrealCli.Protocol;

namespace UnrealCli.Cli.Services;

public static class CliArgumentParser
{
    public static ParsedCommand Parse(string[] args)
    {
        if (args.Length == 0)
            return new ParsedCommand(CommandKind.Help);

        var tokens = new Queue<string>(args);
        var outputMode = OutputMode.Default;
        string? projectOverride = null;

        while (tokens.Count > 0)
        {
            if (tokens.Peek() == "--json") { outputMode = OutputMode.Json; tokens.Dequeue(); continue; }
            if (tokens.Peek() == "--output") { tokens.Dequeue(); outputMode = RequireOutputMode(RequireValue(tokens, "--output")); continue; }
            if (tokens.Peek() == "--project") { tokens.Dequeue(); projectOverride = RequireValue(tokens, "--project"); continue; }
            break;
        }

        if (tokens.Count == 0)
            return new ParsedCommand(CommandKind.Help) { OutputMode = outputMode };

        var command = tokens.Dequeue().ToLowerInvariant();

        ParsedCommand parsed = command switch
        {
            "status" => new ParsedCommand(CommandKind.Status),
            "play" => new ParsedCommand(CommandKind.Play),
            "pause" => new ParsedCommand(CommandKind.Pause),
            "stop" => new ParsedCommand(CommandKind.Stop),
            "compile" => new ParsedCommand(CommandKind.Compile),
            "refresh" => new ParsedCommand(CommandKind.Refresh),
            "read-log" => new ParsedCommand(CommandKind.ReadLog),
            "screenshot" => new ParsedCommand(CommandKind.Screenshot),
            "execute" => new ParsedCommand(CommandKind.ExecuteCode),
            "execute-menu" => new ParsedCommand(CommandKind.ExecuteMenu),
            "asset" => ParseAsset(tokens),
            "level" => ParseLevel(tokens),
            "blueprint" => ParseBlueprint(tokens),
            "anim" => ParseAnim(tokens),
            "material" => ParseMaterial(tokens),
            "plugin" => ParsePlugin(tokens),
            "instances" => ParseInstances(tokens),
            "doctor" => new ParsedCommand(CommandKind.Doctor),
            "raw" => new ParsedCommand(CommandKind.Raw),
            "help" or "--help" or "-h" => new ParsedCommand(CommandKind.Help),
            _ => throw new CliUsageException($"알 수 없는 명령: {command}"),
        };

        parsed.OutputMode = outputMode;
        parsed.ProjectOverride = projectOverride;

        ApplyCatalogDefaults(parsed);
        ParseOptions(parsed, tokens);
        return parsed;
    }

    private static void ApplyCatalogDefaults(ParsedCommand parsed)
    {
        if (parsed.Kind == CommandKind.Help) return;
        string? protocolCmd = GetProtocolCommand(parsed.Kind);
        if (protocolCmd == null) return;
        var descriptor = CliCommandCatalog.FindByProtocolCommand(protocolCmd);
        if (descriptor?.DefaultLiveTimeoutMs != null)
            parsed.TimeoutMs = descriptor.DefaultLiveTimeoutMs.Value;
    }

    private static string? GetProtocolCommand(CommandKind kind) => kind switch
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
        _ => null,
    };

    private static bool IsMaterialCommand(CommandKind kind) => kind is
        CommandKind.MaterialCreate or CommandKind.MaterialInspect or CommandKind.MaterialListNodeTypes or
        CommandKind.MaterialAddNode or CommandKind.MaterialSetNode or CommandKind.MaterialConnect or
        CommandKind.MaterialDisconnect or CommandKind.MaterialDeleteNode or CommandKind.MaterialSetProperty or
        CommandKind.MaterialApplyGraph or CommandKind.MaterialCompile or CommandKind.MaterialCreateInstance or
        CommandKind.MaterialSetInstanceParam;

    private static ParsedCommand ParseAsset(Queue<string> tokens)
    {
        string sub = RequireSubcommand(tokens, "asset");
        return sub switch
        {
            "find" => new ParsedCommand(CommandKind.AssetFind),
            "info" => new ParsedCommand(CommandKind.AssetInfo),
            "move" => new ParsedCommand(CommandKind.AssetMove),
            "rename" => new ParsedCommand(CommandKind.AssetRename),
            "delete" => new ParsedCommand(CommandKind.AssetDelete),
            "create" => new ParsedCommand(CommandKind.AssetCreate),
            "mkdir" => new ParsedCommand(CommandKind.AssetMkdir),
            _ => throw new CliUsageException($"알 수 없는 asset 하위 명령: {sub}"),
        };
    }

    private static ParsedCommand ParseLevel(Queue<string> tokens)
    {
        string sub = RequireSubcommand(tokens, "level");
        return sub switch
        {
            "open" => new ParsedCommand(CommandKind.LevelOpen),
            "inspect" => new ParsedCommand(CommandKind.LevelInspect),
            "add-actor" => new ParsedCommand(CommandKind.LevelAddActor),
            "set-transform" => new ParsedCommand(CommandKind.LevelSetTransform),
            "delete-actor" => new ParsedCommand(CommandKind.LevelDeleteActor),
            "list-components" => new ParsedCommand(CommandKind.LevelListComponents),
            "add-component" => new ParsedCommand(CommandKind.LevelAddComponent),
            "remove-component" => new ParsedCommand(CommandKind.LevelRemoveComponent),
            "assign-material" => new ParsedCommand(CommandKind.LevelAssignMaterial),
            _ => throw new CliUsageException($"알 수 없는 level 하위 명령: {sub}"),
        };
    }

    private static ParsedCommand ParseBlueprint(Queue<string> tokens)
    {
        string sub = RequireSubcommand(tokens, "blueprint");
        return sub switch
        {
            "inspect" => new ParsedCommand(CommandKind.BlueprintInspect),
            "set-property" => new ParsedCommand(CommandKind.BlueprintSetProperty),
            _ => throw new CliUsageException($"알 수 없는 blueprint 하위 명령: {sub}"),
        };
    }

    private static ParsedCommand ParseAnim(Queue<string> tokens)
    {
        string sub = RequireSubcommand(tokens, "anim");
        return sub switch
        {
            "create-abp"        => new ParsedCommand(CommandKind.AnimCreateAbp),
            "assign-abp"        => new ParsedCommand(CommandKind.AnimAssignAbp),
            "list-states"       => new ParsedCommand(CommandKind.AnimListStates),
            "add-variable"      => new ParsedCommand(CommandKind.AnimAddVariable),
            "play-montage"      => new ParsedCommand(CommandKind.AnimPlayMontage),
            "setup-statemachine" => new ParsedCommand(CommandKind.AnimSetupStateMachine),
            _ => throw new CliUsageException($"알 수 없는 anim 하위 명령: {sub}"),
        };
    }

    private static ParsedCommand ParseMaterial(Queue<string> tokens)
    {
        string sub = RequireSubcommand(tokens, "material");
        return sub switch
        {
            "create"             => new ParsedCommand(CommandKind.MaterialCreate),
            "inspect"            => new ParsedCommand(CommandKind.MaterialInspect),
            "list-node-types"    => new ParsedCommand(CommandKind.MaterialListNodeTypes),
            "add-node"           => new ParsedCommand(CommandKind.MaterialAddNode),
            "set-node"           => new ParsedCommand(CommandKind.MaterialSetNode),
            "connect"            => new ParsedCommand(CommandKind.MaterialConnect),
            "disconnect"         => new ParsedCommand(CommandKind.MaterialDisconnect),
            "delete-node"        => new ParsedCommand(CommandKind.MaterialDeleteNode),
            "set-property"       => new ParsedCommand(CommandKind.MaterialSetProperty),
            "apply-graph"        => new ParsedCommand(CommandKind.MaterialApplyGraph),
            "compile"            => new ParsedCommand(CommandKind.MaterialCompile),
            "create-instance"    => new ParsedCommand(CommandKind.MaterialCreateInstance),
            "set-instance-param" => new ParsedCommand(CommandKind.MaterialSetInstanceParam),
            _ => throw new CliUsageException($"알 수 없는 material 하위 명령: {sub}"),
        };
    }

    private static ParsedCommand ParsePlugin(Queue<string> tokens)
    {
        string sub = RequireSubcommand(tokens, "plugin");
        return sub switch
        {
            "list" => new ParsedCommand(CommandKind.PluginList),
            "enable" => new ParsedCommand(CommandKind.PluginEnable),
            "disable" => new ParsedCommand(CommandKind.PluginDisable),
            _ => throw new CliUsageException($"알 수 없는 plugin 하위 명령: {sub}"),
        };
    }

    private static ParsedCommand ParseInstances(Queue<string> tokens)
    {
        string sub = RequireSubcommand(tokens, "instances");
        return sub switch
        {
            "list" => new ParsedCommand(CommandKind.InstancesList),
            "use" => new ParsedCommand(CommandKind.InstancesUse)
            {
                InstanceTarget = tokens.Count > 0 ? tokens.Dequeue() : null,
            },
            _ => throw new CliUsageException($"알 수 없는 instances 하위 명령: {sub}"),
        };
    }

    private static void ParseOptions(ParsedCommand parsed, Queue<string> tokens)
    {
        while (tokens.Count > 0)
        {
            var token = tokens.Dequeue();

            if (token == "--project") { parsed.ProjectOverride = RequireValue(tokens, "--project"); continue; }
            if (token == "--output") { parsed.OutputMode = RequireOutputMode(RequireValue(tokens, "--output")); continue; }
            if (token == "--json" && parsed.Kind != CommandKind.Raw) { parsed.OutputMode = OutputMode.Json; continue; }
            if (token == "--force") { parsed.Force = true; continue; }
            if (token == "--wait") { parsed.Wait = true; continue; }

            // Shared across every material subcommand.
            if (IsMaterialCommand(parsed.Kind))
            {
                if (token == "--save") { parsed.MatSave = true; continue; }
                if (token == "--no-compile") { parsed.MatNoCompile = true; continue; }
                if (token == "--path") { parsed.MatPath = RequireValue(tokens, "--path"); continue; }
            }

            switch (parsed.Kind)
            {
                // read-log
                case CommandKind.ReadLog when token == "--limit":
                    parsed.LogLimit = RequireInt(tokens, "--limit"); break;
                case CommandKind.ReadLog when token == "--type":
                    parsed.LogType = RequireValue(tokens, "--type"); break;

                // screenshot
                case CommandKind.Screenshot when token == "--viewport":
                    parsed.ScreenshotViewport = RequireValue(tokens, "--viewport"); break;
                case CommandKind.Screenshot when token == "--path":
                    parsed.ScreenshotPath = RequireValue(tokens, "--path"); break;
                case CommandKind.Screenshot when token == "--width":
                    parsed.ScreenshotWidth = RequireInt(tokens, "--width"); break;
                case CommandKind.Screenshot when token == "--height":
                    parsed.ScreenshotHeight = RequireInt(tokens, "--height"); break;

                // execute
                case CommandKind.ExecuteCode when token == "--code":
                    parsed.ExecuteCodeSnippet = RequireValue(tokens, "--code"); break;
                case CommandKind.ExecuteCode when token == "--file":
                    parsed.ExecuteCodeFile = RequireValue(tokens, "--file"); break;
                case CommandKind.ExecuteCode when token == "--args":
                    parsed.ExecuteCodeArgsJson = RequireValue(tokens, "--args"); break;

                // execute-menu
                case CommandKind.ExecuteMenu when token == "--path":
                    parsed.MenuPath = RequireValue(tokens, "--path"); break;
                case CommandKind.ExecuteMenu when token == "--list":
                    parsed.MenuList = true;
                    parsed.MenuListPrefix = RequireValue(tokens, "--list"); break;

                // asset find
                case CommandKind.AssetFind when token == "--name":
                    parsed.AssetName = RequireValue(tokens, "--name"); break;
                case CommandKind.AssetFind when token == "--type":
                    parsed.AssetType = RequireValue(tokens, "--type"); break;
                case CommandKind.AssetFind when token == "--folder":
                    parsed.AssetFolder = RequireValue(tokens, "--folder"); break;
                case CommandKind.AssetFind when token == "--limit":
                    parsed.AssetLimit = RequireInt(tokens, "--limit"); break;

                // asset info / delete / mkdir
                case CommandKind.AssetInfo when token == "--path":
                case CommandKind.AssetDelete when token == "--path":
                case CommandKind.AssetMkdir when token == "--path":
                    parsed.AssetPath = RequireValue(tokens, "--path"); break;

                // asset move
                case CommandKind.AssetMove when token == "--from":
                    parsed.AssetFrom = RequireValue(tokens, "--from"); break;
                case CommandKind.AssetMove when token == "--to":
                    parsed.AssetTo = RequireValue(tokens, "--to"); break;

                // asset rename
                case CommandKind.AssetRename when token == "--path":
                    parsed.AssetPath = RequireValue(tokens, "--path"); break;
                case CommandKind.AssetRename when token == "--name":
                    parsed.AssetNewName = RequireValue(tokens, "--name"); break;

                // asset create
                case CommandKind.AssetCreate when token == "--type":
                    parsed.AssetCreateType = RequireValue(tokens, "--type"); break;
                case CommandKind.AssetCreate when token == "--path":
                    parsed.AssetPath = RequireValue(tokens, "--path"); break;
                case CommandKind.AssetCreate when token == "--data-json":
                    parsed.AssetDataJson = RequireValue(tokens, "--data-json"); break;

                // level open
                case CommandKind.LevelOpen when token == "--path":
                    parsed.LevelPath = RequireValue(tokens, "--path"); break;

                // level inspect
                case CommandKind.LevelInspect when token == "--path":
                    parsed.LevelPath = RequireValue(tokens, "--path"); break;
                case CommandKind.LevelInspect when token == "--with-values":
                    parsed.LevelWithValues = true; break;
                case CommandKind.LevelInspect when token == "--max-depth":
                    parsed.MaxDepth = RequireInt(tokens, "--max-depth"); break;

                // level add-actor
                case CommandKind.LevelAddActor when token == "--class":
                    parsed.ActorClass = RequireValue(tokens, "--class"); break;
                case CommandKind.LevelAddActor when token == "--name":
                    parsed.ActorLabel = RequireValue(tokens, "--name"); break;
                case CommandKind.LevelAddActor when token == "--location":
                    parsed.ActorLocation = RequireValue(tokens, "--location"); break;
                case CommandKind.LevelAddActor when token == "--rotation":
                    parsed.ActorRotation = RequireValue(tokens, "--rotation"); break;
                case CommandKind.LevelAddActor when token == "--scale":
                    parsed.ActorScale = RequireValue(tokens, "--scale"); break;

                // level set-transform
                case CommandKind.LevelSetTransform when token == "--actor":
                    parsed.ActorLabel = RequireValue(tokens, "--actor"); break;
                case CommandKind.LevelSetTransform when token == "--location":
                    parsed.ActorLocation = RequireValue(tokens, "--location"); break;
                case CommandKind.LevelSetTransform when token == "--rotation":
                    parsed.ActorRotation = RequireValue(tokens, "--rotation"); break;
                case CommandKind.LevelSetTransform when token == "--scale":
                    parsed.ActorScale = RequireValue(tokens, "--scale"); break;

                // level delete-actor / list-components / add-component / remove-component
                case CommandKind.LevelDeleteActor when token == "--actor":
                case CommandKind.LevelListComponents when token == "--actor":
                case CommandKind.LevelAddComponent when token == "--actor":
                case CommandKind.LevelRemoveComponent when token == "--actor":
                case CommandKind.LevelAssignMaterial when token == "--actor":
                    parsed.ActorLabel = RequireValue(tokens, "--actor"); break;

                case CommandKind.LevelAddComponent when token == "--type":
                case CommandKind.LevelRemoveComponent when token == "--type":
                    parsed.ComponentType = RequireValue(tokens, "--type"); break;
                case CommandKind.LevelAddComponent when token == "--values":
                    parsed.ComponentValuesJson = RequireValue(tokens, "--values"); break;
                case CommandKind.LevelRemoveComponent when token == "--index":
                    parsed.ComponentIndex = RequireInt(tokens, "--index"); break;

                // level assign-material
                case CommandKind.LevelAssignMaterial when token == "--material":
                    parsed.MaterialPath = RequireValue(tokens, "--material"); break;
                case CommandKind.LevelAssignMaterial when token == "--slot":
                    parsed.MaterialSlot = RequireInt(tokens, "--slot"); break;

                // anim create-abp
                case CommandKind.AnimCreateAbp when token == "--skeleton":
                    parsed.AnimSkeletonPath = RequireValue(tokens, "--skeleton"); break;
                case CommandKind.AnimCreateAbp when token == "--path":
                    parsed.AnimPath = RequireValue(tokens, "--path"); break;

                // anim assign-abp
                case CommandKind.AnimAssignAbp when token == "--bp":
                    parsed.AnimTargetBpPath = RequireValue(tokens, "--bp"); break;
                case CommandKind.AnimAssignAbp when token == "--anim-bp":
                    parsed.AnimBlueprintPath = RequireValue(tokens, "--anim-bp"); break;
                case CommandKind.AnimAssignAbp when token == "--component":
                    parsed.AnimComponentName = RequireValue(tokens, "--component"); break;

                // anim list-states
                case CommandKind.AnimListStates when token == "--path":
                    parsed.AnimPath = RequireValue(tokens, "--path"); break;

                // anim add-variable
                case CommandKind.AnimAddVariable when token == "--path":
                    parsed.AnimPath = RequireValue(tokens, "--path"); break;
                case CommandKind.AnimAddVariable when token == "--name":
                    parsed.AnimVariableName = RequireValue(tokens, "--name"); break;
                case CommandKind.AnimAddVariable when token == "--type":
                    parsed.AnimVariableType = RequireValue(tokens, "--type"); break;

                // anim play-montage
                case CommandKind.AnimPlayMontage when token == "--actor":
                    parsed.AnimActorLabel = RequireValue(tokens, "--actor"); break;
                case CommandKind.AnimPlayMontage when token == "--montage":
                    parsed.AnimMontagePath = RequireValue(tokens, "--montage"); break;
                case CommandKind.AnimPlayMontage when token == "--rate":
                    parsed.AnimPlayRate = double.Parse(RequireValue(tokens, "--rate")); break;

                // anim setup-statemachine
                case CommandKind.AnimSetupStateMachine when token == "--path":
                    parsed.AnimPath = RequireValue(tokens, "--path"); break;
                case CommandKind.AnimSetupStateMachine when token == "--idle-anim":
                    parsed.AnimIdleAnimPath = RequireValue(tokens, "--idle-anim"); break;
                case CommandKind.AnimSetupStateMachine when token == "--walk-anim":
                    parsed.AnimWalkAnimPath = RequireValue(tokens, "--walk-anim"); break;
                case CommandKind.AnimSetupStateMachine when token == "--walk-threshold":
                    parsed.AnimWalkThreshold = double.Parse(RequireValue(tokens, "--walk-threshold")); break;

                // blueprint
                case CommandKind.BlueprintInspect when token == "--path":
                case CommandKind.BlueprintSetProperty when token == "--path":
                    parsed.BlueprintPath = RequireValue(tokens, "--path"); break;
                case CommandKind.BlueprintInspect when token == "--with-values":
                    parsed.BlueprintWithValues = true; break;
                case CommandKind.BlueprintInspect when token == "--max-depth":
                    parsed.MaxDepth = RequireInt(tokens, "--max-depth"); break;
                case CommandKind.BlueprintSetProperty when token == "--property":
                    parsed.BlueprintProperty = RequireValue(tokens, "--property"); break;
                case CommandKind.BlueprintSetProperty when token == "--value":
                    parsed.BlueprintValue = RequireValue(tokens, "--value"); break;

                // material create / set-property presets
                case CommandKind.MaterialCreate when token == "--domain":
                case CommandKind.MaterialSetProperty when token == "--domain":
                    parsed.MatDomain = RequireValue(tokens, "--domain"); break;
                case CommandKind.MaterialCreate when token == "--blend":
                case CommandKind.MaterialSetProperty when token == "--blend":
                    parsed.MatBlend = RequireValue(tokens, "--blend"); break;
                case CommandKind.MaterialCreate when token == "--shading":
                case CommandKind.MaterialSetProperty when token == "--shading":
                    parsed.MatShading = RequireValue(tokens, "--shading"); break;
                case CommandKind.MaterialCreate when token == "--two-sided":
                case CommandKind.MaterialSetProperty when token == "--two-sided":
                    parsed.MatTwoSided = true; break;

                // material inspect
                case CommandKind.MaterialInspect when token == "--with-values":
                    parsed.MatWithValues = true; break;

                // material list-node-types
                case CommandKind.MaterialListNodeTypes when token == "--filter":
                    parsed.MatFilter = RequireValue(tokens, "--filter"); break;
                case CommandKind.MaterialListNodeTypes when token == "--limit":
                    parsed.MatLimit = RequireInt(tokens, "--limit"); break;

                // material add-node / set-node / delete-node
                case CommandKind.MaterialAddNode when token == "--type":
                    parsed.MatNodeType = RequireValue(tokens, "--type"); break;
                case CommandKind.MaterialAddNode when token == "--name":
                case CommandKind.MaterialAddNode when token == "--node":
                case CommandKind.MaterialSetNode when token == "--node":
                case CommandKind.MaterialDeleteNode when token == "--node":
                    parsed.MatNodeId = RequireValue(tokens, token); break;
                case CommandKind.MaterialAddNode when token == "--pos":
                case CommandKind.MaterialSetNode when token == "--pos":
                    parsed.MatPos = RequireValue(tokens, "--pos"); break;
                case CommandKind.MaterialAddNode when token == "--values":
                case CommandKind.MaterialSetNode when token == "--values":
                case CommandKind.MaterialSetProperty when token == "--values":
                    parsed.MatValuesJson = RequireValue(tokens, "--values"); break;

                // material connect / disconnect
                case CommandKind.MaterialConnect when token == "--from":
                    parsed.MatFrom = RequireValue(tokens, "--from"); break;
                case CommandKind.MaterialConnect when token == "--from-output":
                    parsed.MatFromOutput = RequireValue(tokens, "--from-output"); break;
                case CommandKind.MaterialConnect when token == "--to":
                case CommandKind.MaterialDisconnect when token == "--to":
                    parsed.MatTo = RequireValue(tokens, "--to"); break;
                case CommandKind.MaterialConnect when token == "--to-input":
                case CommandKind.MaterialDisconnect when token == "--to-input":
                    parsed.MatToInput = RequireValue(tokens, "--to-input"); break;
                case CommandKind.MaterialConnect when token == "--property":
                case CommandKind.MaterialDisconnect when token == "--property":
                case CommandKind.MaterialSetProperty when token == "--property":
                    parsed.MatProperty = RequireValue(tokens, "--property"); break;
                case CommandKind.MaterialSetProperty when token == "--value":
                case CommandKind.MaterialSetInstanceParam when token == "--value":
                    parsed.MatValue = RequireValue(tokens, "--value"); break;

                // material apply-graph
                case CommandKind.MaterialApplyGraph when token == "--graph":
                    parsed.MatGraphJson = RequireValue(tokens, "--graph"); break;
                case CommandKind.MaterialApplyGraph when token == "--graph-file":
                    parsed.MatGraphFile = RequireValue(tokens, "--graph-file"); break;
                case CommandKind.MaterialApplyGraph when token == "--clear":
                    parsed.MatClear = true; break;
                case CommandKind.MaterialApplyGraph when token == "--layout":
                case CommandKind.MaterialCompile when token == "--layout":
                    parsed.MatLayout = true; break;

                // material create-instance / set-instance-param
                case CommandKind.MaterialCreateInstance when token == "--parent":
                    parsed.MatParent = RequireValue(tokens, "--parent"); break;
                case CommandKind.MaterialSetInstanceParam when token == "--name":
                    parsed.MatParamName = RequireValue(tokens, "--name"); break;
                case CommandKind.MaterialSetInstanceParam when token == "--type":
                    parsed.MatParamType = RequireValue(tokens, "--type"); break;

                // plugin
                case CommandKind.PluginEnable when token == "--name":
                case CommandKind.PluginDisable when token == "--name":
                    parsed.PluginName = RequireValue(tokens, "--name"); break;

                // raw
                case CommandKind.Raw when token == "--json":
                    parsed.RawJson = RequireValue(tokens, "--json"); break;

                default:
                    throw new CliUsageException($"지원하지 않는 옵션: {token}");
            }
        }

        Validate(parsed);
    }

    private static void Validate(ParsedCommand parsed)
    {
        switch (parsed.Kind)
        {
            case CommandKind.AssetFind when parsed.AssetName == null && parsed.AssetType == null:
                throw new CliUsageException("asset find에는 --name 또는 --type 중 하나가 필요합니다.");
            case CommandKind.AssetInfo when parsed.AssetPath == null:
                throw new CliUsageException("asset info에는 --path가 필요합니다.");
            case CommandKind.AssetMove when parsed.AssetFrom == null || parsed.AssetTo == null:
                throw new CliUsageException("asset move에는 --from과 --to가 모두 필요합니다.");
            case CommandKind.AssetRename when parsed.AssetPath == null || parsed.AssetNewName == null:
                throw new CliUsageException("asset rename에는 --path와 --name이 모두 필요합니다.");
            case CommandKind.AssetDelete when parsed.AssetPath == null:
                throw new CliUsageException("asset delete에는 --path가 필요합니다.");
            case CommandKind.AssetDelete when !parsed.Force:
                throw new CliUsageException("asset delete는 항상 --force가 필요합니다.");
            case CommandKind.AssetCreate when parsed.AssetCreateType == null || parsed.AssetPath == null:
                throw new CliUsageException("asset create에는 --type과 --path가 필요합니다.");
            case CommandKind.AssetMkdir when parsed.AssetPath == null:
                throw new CliUsageException("asset mkdir에는 --path가 필요합니다.");
            case CommandKind.LevelAddActor when parsed.ActorClass == null:
                throw new CliUsageException("level add-actor에는 --class가 필요합니다.");
            case CommandKind.LevelSetTransform when parsed.ActorLabel == null:
                throw new CliUsageException("level set-transform에는 --actor가 필요합니다.");
            case CommandKind.LevelSetTransform when parsed.ActorLocation == null && parsed.ActorRotation == null && parsed.ActorScale == null:
                throw new CliUsageException("level set-transform에는 --location, --rotation, --scale 중 하나가 필요합니다.");
            case CommandKind.LevelDeleteActor when parsed.ActorLabel == null:
                throw new CliUsageException("level delete-actor에는 --actor가 필요합니다.");
            case CommandKind.LevelDeleteActor when !parsed.Force:
                throw new CliUsageException("level delete-actor는 항상 --force가 필요합니다.");
            case CommandKind.LevelListComponents when parsed.ActorLabel == null:
                throw new CliUsageException("level list-components에는 --actor가 필요합니다.");
            case CommandKind.LevelAddComponent when parsed.ActorLabel == null || parsed.ComponentType == null:
                throw new CliUsageException("level add-component에는 --actor와 --type이 필요합니다.");
            case CommandKind.LevelRemoveComponent when parsed.ActorLabel == null || parsed.ComponentType == null:
                throw new CliUsageException("level remove-component에는 --actor와 --type이 필요합니다.");
            case CommandKind.LevelRemoveComponent when !parsed.Force:
                throw new CliUsageException("level remove-component는 항상 --force가 필요합니다.");
            case CommandKind.LevelAssignMaterial when parsed.ActorLabel == null || parsed.MaterialPath == null:
                throw new CliUsageException("level assign-material에는 --actor와 --material이 필요합니다.");
            case CommandKind.AnimCreateAbp when parsed.AnimSkeletonPath == null || parsed.AnimPath == null:
                throw new CliUsageException("anim create-abp에는 --skeleton과 --path가 필요합니다.");
            case CommandKind.AnimAssignAbp when parsed.AnimTargetBpPath == null || parsed.AnimBlueprintPath == null:
                throw new CliUsageException("anim assign-abp에는 --bp와 --anim-bp가 필요합니다.");
            case CommandKind.AnimListStates when parsed.AnimPath == null:
                throw new CliUsageException("anim list-states에는 --path가 필요합니다.");
            case CommandKind.AnimAddVariable when parsed.AnimPath == null || parsed.AnimVariableName == null || parsed.AnimVariableType == null:
                throw new CliUsageException("anim add-variable에는 --path, --name, --type이 필요합니다.");
            case CommandKind.AnimPlayMontage when parsed.AnimActorLabel == null || parsed.AnimMontagePath == null:
                throw new CliUsageException("anim play-montage에는 --actor와 --montage가 필요합니다.");
            case CommandKind.AnimSetupStateMachine when parsed.AnimPath == null || parsed.AnimIdleAnimPath == null || parsed.AnimWalkAnimPath == null:
                throw new CliUsageException("anim setup-statemachine에는 --path, --idle-anim, --walk-anim이 필요합니다.");
            case CommandKind.BlueprintInspect when parsed.BlueprintPath == null:
                throw new CliUsageException("blueprint inspect에는 --path가 필요합니다.");
            case CommandKind.BlueprintSetProperty when parsed.BlueprintPath == null || parsed.BlueprintProperty == null || parsed.BlueprintValue == null:
                throw new CliUsageException("blueprint set-property에는 --path, --property, --value가 모두 필요합니다.");
            case CommandKind.MaterialCreate when parsed.MatPath == null:
            case CommandKind.MaterialInspect when parsed.MatPath == null:
            case CommandKind.MaterialAddNode when parsed.MatPath == null:
            case CommandKind.MaterialSetNode when parsed.MatPath == null:
            case CommandKind.MaterialConnect when parsed.MatPath == null:
            case CommandKind.MaterialDisconnect when parsed.MatPath == null:
            case CommandKind.MaterialDeleteNode when parsed.MatPath == null:
            case CommandKind.MaterialSetProperty when parsed.MatPath == null:
            case CommandKind.MaterialApplyGraph when parsed.MatPath == null:
            case CommandKind.MaterialCompile when parsed.MatPath == null:
            case CommandKind.MaterialCreateInstance when parsed.MatPath == null:
            case CommandKind.MaterialSetInstanceParam when parsed.MatPath == null:
                throw new CliUsageException("material 명령에는 --path가 필요합니다.");
            case CommandKind.MaterialAddNode when parsed.MatNodeType == null:
                throw new CliUsageException("material add-node에는 --type이 필요합니다.");
            case CommandKind.MaterialSetNode when parsed.MatNodeId == null || parsed.MatValuesJson == null:
                throw new CliUsageException("material set-node에는 --node와 --values가 필요합니다.");
            case CommandKind.MaterialDeleteNode when parsed.MatNodeId == null:
                throw new CliUsageException("material delete-node에는 --node가 필요합니다.");
            case CommandKind.MaterialDeleteNode when !parsed.Force:
                throw new CliUsageException("material delete-node는 항상 --force가 필요합니다.");
            case CommandKind.MaterialConnect when parsed.MatFrom == null:
                throw new CliUsageException("material connect에는 --from이 필요합니다.");
            case CommandKind.MaterialConnect when parsed.MatTo == null && parsed.MatProperty == null:
                throw new CliUsageException("material connect에는 --to 또는 --property가 필요합니다.");
            case CommandKind.MaterialDisconnect when parsed.MatTo == null && parsed.MatProperty == null:
                throw new CliUsageException("material disconnect에는 --to 또는 --property가 필요합니다.");
            case CommandKind.MaterialSetProperty when parsed.MatProperty != null && parsed.MatValue == null:
                throw new CliUsageException("material set-property에서 --property를 쓰면 --value도 필요합니다.");
            case CommandKind.MaterialSetProperty when parsed.MatProperty == null && parsed.MatValuesJson == null
                && parsed.MatDomain == null && parsed.MatBlend == null && parsed.MatShading == null && !parsed.MatTwoSided:
                throw new CliUsageException("material set-property에는 --property/--value, --values, 또는 --domain/--blend/--shading/--two-sided 중 하나가 필요합니다.");
            case CommandKind.MaterialApplyGraph when parsed.MatGraphJson == null && parsed.MatGraphFile == null:
                throw new CliUsageException("material apply-graph에는 --graph 또는 --graph-file이 필요합니다.");
            case CommandKind.MaterialCreateInstance when parsed.MatParent == null:
                throw new CliUsageException("material create-instance에는 --parent가 필요합니다.");
            case CommandKind.MaterialSetInstanceParam when parsed.MatParamName == null || parsed.MatParamType == null || parsed.MatValue == null:
                throw new CliUsageException("material set-instance-param에는 --name, --type, --value가 필요합니다.");
            case CommandKind.PluginEnable when parsed.PluginName == null:
                throw new CliUsageException("plugin enable에는 --name이 필요합니다.");
            case CommandKind.PluginDisable when parsed.PluginName == null:
                throw new CliUsageException("plugin disable에는 --name이 필요합니다.");
            case CommandKind.PluginDisable when !parsed.Force:
                throw new CliUsageException("plugin disable은 항상 --force가 필요합니다.");
            case CommandKind.ExecuteCode when parsed.ExecuteCodeSnippet == null && parsed.ExecuteCodeFile == null:
                throw new CliUsageException("execute에는 --code 또는 --file이 필요합니다.");
            case CommandKind.ExecuteCode when !parsed.Force:
                throw new CliUsageException("execute는 항상 --force가 필요합니다.");
            case CommandKind.ExecuteMenu when parsed.MenuPath == null && !parsed.MenuList:
                throw new CliUsageException("execute-menu에는 --path 또는 --list가 필요합니다.");
            case CommandKind.Raw when string.IsNullOrWhiteSpace(parsed.RawJson):
                throw new CliUsageException("raw에는 --json payload가 필요합니다.");
        }
    }

    private static string RequireSubcommand(Queue<string> tokens, string parent)
    {
        if (tokens.Count == 0)
            throw new CliUsageException($"`{parent}` 다음에는 하위 명령이 필요합니다.");
        return tokens.Dequeue().ToLowerInvariant();
    }

    private static string RequireValue(Queue<string> tokens, string option)
    {
        if (tokens.Count == 0 || tokens.Peek().StartsWith("--", StringComparison.Ordinal))
            throw new CliUsageException($"`{option}`에 값이 필요합니다.");
        return tokens.Dequeue();
    }

    private static int RequireInt(Queue<string> tokens, string option)
    {
        string value = RequireValue(tokens, option);
        if (!int.TryParse(value, out int result) || result < 0)
            throw new CliUsageException($"`{option}`에 유효한 양의 정수가 필요합니다. 입력값: {value}");
        return result;
    }

    private static OutputMode RequireOutputMode(string value)
    {
        return value.ToLowerInvariant() switch
        {
            "default" => OutputMode.Default,
            "json" => OutputMode.Json,
            "compact" => OutputMode.Compact,
            _ => throw new CliUsageException($"`--output`은 default, json, compact 중 하나여야 합니다. 입력값: {value}"),
        };
    }

    public static OutputMode DetectOutputMode(string[] args)
    {
        var tokens = new Queue<string>(args);
        var mode = OutputMode.Default;
        while (tokens.Count > 0)
        {
            if (tokens.Peek() == "--json") { mode = OutputMode.Json; tokens.Dequeue(); continue; }
            if (tokens.Peek() == "--output")
            {
                tokens.Dequeue();
                if (tokens.Count > 0)
                {
                    var val = tokens.Dequeue().ToLowerInvariant();
                    mode = val switch { "json" => OutputMode.Json, "compact" => OutputMode.Compact, _ => OutputMode.Default };
                }
                continue;
            }
            if (tokens.Peek() == "--project") { tokens.Dequeue(); if (tokens.Count > 0) tokens.Dequeue(); continue; }
            break;
        }
        return mode;
    }
}

public sealed class CliUsageException : Exception
{
    public CliUsageException(string message) : base(message) { }
}
