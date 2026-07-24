# Project Overview

This repository contains Unreal Engine 5 projects and the `unreal-cli-bridge` tool for controlling the Unreal Editor from the command line.

## unreal-cli — Unreal Editor CLI Tool

**Executable path (relative to the current Unreal project folder):**
```
..\unreal-cli-bridge\dist\unreal-cli\unreal-cli.exe
```

The project folder (e.g. `My3DAction\`, `OwnershipRules\`) and `unreal-cli-bridge\` always sit as siblings directly under `MyDirectX9Projects\`, so this relative path works regardless of machine or absolute drive/path. Resolve it from the Unreal project folder you're currently working in — do not hardcode an absolute path, since it differs per machine.

When the user asks you to do anything inside the Unreal Editor (add actors, open levels, inspect assets, run PIE, etc.), **always use this tool first** without asking the user. The editor must be open and the UnrealCliBridge plugin must be loaded for live commands to work.

### Usage

```
unreal-cli.exe <command> [options]
```

Check editor connection first if unsure:
```
unreal-cli.exe status
```

### Available Commands

#### Editor Control
| Command | Description |
|---------|-------------|
| `status` | Editor connection status, current level, PIE state |
| `play` | Start PIE |
| `pause` | Pause PIE |
| `stop` | Stop PIE |
| `compile [--wait]` | Trigger Hot Reload compile |
| `refresh [--wait]` | Reimport modified assets |
| `read-log [--limit N] [--type log\|warning\|error]` | Read Output Log |
| `screenshot [--viewport game\|editor] [--path out.png] [--width N] [--height N]` | Capture viewport screenshot |
| `execute-menu (--path "Menu/Item" \| --list "Prefix")` | Execute or list menu items |
| `execute (--code <python> \| --file <path.py>) [--args <json>] --force` | Run Python in editor |

#### Asset Workflows
| Command | Description |
|---------|-------------|
| `asset find [--name <term>] [--type <type>] [--folder /Game/...] [--limit N]` | Search assets |
| `asset info --path /Game/...` | Asset metadata |
| `asset move --from /Game/... --to /Game/... [--force]` | Move asset |
| `asset rename --path /Game/... --name <newName> [--force]` | Rename asset |
| `asset delete --path /Game/... --force` | Delete asset |
| `asset create --type <kind> --path /Game/... [--force]` | Create asset (types: material, texture, staticmesh, blueprint, dataasset, curve, soundcue) |
| `asset mkdir --path /Game/...` | Create content folder |

#### Level Workflows
| Command | Description |
|---------|-------------|
| `level open --path /Game/... [--force]` | Open a level |
| `level inspect [--path /Game/...] [--with-values] [--max-depth N]` | Inspect actor hierarchy |
| `level add-actor --class <ClassName> [--name <label>] [--location x,y,z] [--rotation p,y,r] [--scale x,y,z]` | Spawn actor (common classes: StaticMeshActor, PointLight, DirectionalLight, CameraActor, TriggerBox) |
| `level set-transform --actor <label> [--location x,y,z] [--rotation p,y,r] [--scale x,y,z]` | Set actor transform |
| `level delete-actor --actor <label> --force` | Delete actor |
| `level list-components --actor <label>` | List actor components |
| `level add-component --actor <label> --type <ComponentType> [--values <json>]` | Add component to actor |
| `level remove-component --actor <label> --type <ComponentType> [--index N] --force` | Remove component |
| `level assign-material --actor <label> --material /Game/... [--slot N]` | Assign material to mesh |

#### Blueprint Workflows
| Command | Description |
|---------|-------------|
| `blueprint inspect --path /Game/... [--with-values] [--max-depth N]` | Inspect Blueprint hierarchy and properties |
| `blueprint set-property --path /Game/... --property <name> --value <json>` | Set Blueprint CDO property |

#### Animation Workflows
| Command | Description |
|---------|-------------|
| `anim create-abp --skeleton /Game/... --path /Game/... [--force]` | Create an Animation Blueprint for a Skeleton |
| `anim assign-abp --bp /Game/... --anim-bp /Game/... [--component <name>]` | Assign an ABP to a Blueprint's SkeletalMeshComponent |
| `anim list-states --path /Game/...` | List state machines, states and variables |
| `anim add-variable --path /Game/... --name <var> --type float\|bool\|int` | Add an ABP variable |
| `anim play-montage --actor <label> --montage /Game/... [--rate <float>]` | Play a montage on a live PIE actor |
| `anim setup-statemachine --path /Game/... --idle-anim /Game/... --walk-anim /Game/... [--walk-threshold N]` | Build an Idle/Walk state machine |

#### Material / Shader Workflows
| Command | Description |
|---------|-------------|
| `material create --path /Game/... [--domain <d>] [--blend <b>] [--shading <s>] [--two-sided] [--force] [--save]` | Create a Material with domain/blend/shading preset |
| `material inspect --path /Game/... [--with-values]` | Dump the node graph: nodes, pin connections, material outputs |
| `material list-node-types [--filter <term>] [--limit N]` | Search available expression node types |
| `material add-node --path /Game/... --type <NodeType> [--name <id>] [--pos x,y] [--values <json>]` | Add one node |
| `material set-node --path /Game/... --node <id> --values <json> [--pos x,y]` | Set properties on an existing node |
| `material connect --path /Game/... --from <id> [--from-output <pin>] (--to <id> [--to-input <pin>] \| --property <MatOutput>)` | Connect node→node or node→material output |
| `material disconnect --path /Game/... (--to <id> [--to-input <pin>] \| --property <MatOutput>)` | Clear an input pin |
| `material delete-node --path /Game/... --node <id> --force` | Delete a node |
| `material set-property --path /Game/... (--property <n> --value <v> \| --values <json> \| --blend/--shading/...)` | Set material-level settings |
| `material apply-graph --path /Game/... (--graph <json> \| --graph-file <path>) [--clear] [--layout] [--force]` | Build a whole graph from one JSON description |
| `material compile --path /Game/... [--layout] [--save]` | Recompile (and optionally auto-arrange) |
| `material create-instance --path /Game/... --parent /Game/... [--force]` | Create a Material Instance Constant |
| `material set-instance-param --path /Game/... --name <p> --type scalar\|vector\|texture\|switch --value <v>` | Override an instance parameter |

**Implementing a shader described in natural language:** build the graph with a single
`material apply-graph --graph-file <scratch>.json --clear`, then `material inspect` to verify.
Node ids come from `--name` / the JSON `"id"` field and are stored as the node's graph comment,
so they stay stable across later `connect` / `set-node` calls. Every mutating command recompiles
the material by default (`--no-compile` to batch, `--save` to write the package to disk).
Graph JSON shape:

```json
{
  "settings": { "blend": "translucent", "shading": "unlit" },
  "nodes": [
    { "id": "tex",  "type": "TextureSample",   "values": { "Texture": "/Game/T_Noise" } },
    { "id": "tint", "type": "VectorParameter", "values": { "ParameterName": "Tint", "DefaultValue": [1, 0.4, 0.1, 1] } },
    { "id": "mul",  "type": "Multiply" }
  ],
  "connections": [
    { "from": "tex",  "fromOutput": "RGB", "to": "mul", "toInput": "A" },
    { "from": "tint", "to": "mul", "toInput": "B" }
  ],
  "outputs": [ { "from": "mul", "property": "EmissiveColor" } ]
}
```

#### Plugin Management
| Command | Description |
|---------|-------------|
| `plugin list` | List all plugins |
| `plugin enable --name <PluginName>` | Enable plugin (requires restart) |
| `plugin disable --name <PluginName> --force` | Disable plugin (requires restart) |

#### Diagnostics
| Command | Description |
|---------|-------------|
| `instances list` | List known editor instances |
| `instances use <hash\|path>` | Switch active target project |
| `doctor` | Connection diagnostics |
| `raw --json '{...}' [--force]` | Send raw protocol envelope |

### Notes
- **Run unreal-cli through PowerShell, not the Bash tool.** Git Bash rewrites `/Game/...` arguments
  into Windows paths (`C:/Program Files/Git/Game/...`), which makes the editor pop a modal error
  dialog and the command fail.
- `--force` is required for destructive operations (delete, overwrite)
- `execute` requires the Python Script Plugin enabled in the project
- Rebuilding the CLI: run `unreal-cli-bridge\scripts\publish-win-x64.bat`
- Plugin source: see the dual-location sync rule below
- Plugin C++ changes are **not** picked up by `compile` (hot reload only rebuilds the game module).
  The editor must be closed, the project's editor target rebuilt, and the editor reopened.

## Source File Encoding (Korean/non-ASCII comments)

C++ source/header files in these projects contain Korean comments. Files without a UTF-8 BOM are ambiguous — different tools (Visual Studio, git, MSVC, you) can guess the wrong encoding on save and silently corrupt the Korean text into `U+FFFD` replacement characters, which is **unrecoverable** (not just a display glitch — the original bytes are permanently lost once this happens).

- Before editing any `.cpp`/`.h` file, if it contains non-ASCII (Korean) text, treat it as UTF-8 and read/write it faithfully — never re-save content you can't render correctly.
- After editing a file that contains Korean comments, verify no `U+FFFD` characters were introduced by the edit.
- When creating a **new** `.cpp`/`.h` file that will contain Korean comments, save it with a **UTF-8 BOM** (`EF BB BF` at the start of the file) so every tool that touches it afterward (Visual Studio, git, MSVC) unambiguously detects UTF-8 instead of guessing. A root-level `.editorconfig` (`charset = utf-8-bom` for `*.cpp,*.h,*.hpp,*.cc,*.cxx`) makes Visual Studio do this automatically on save — but you write files directly, not through VS, so add the BOM yourself regardless.
- If you ever find `U+FFFD` characters already baked into a file, check git history first (`git log --follow -- <path>`) for a clean pre-corruption commit before attempting to reconstruct the comment from context — reconstruction from context is a last resort since it changes the original wording.

## Projects

- **OwnershipRules** — Main UE5 project (`OwnershipRules\OwnershipRules.uproject`)
- **unreal-cli-bridge** — CLI tool + UE5 plugin source (`unreal-cli-bridge\`)

## UnrealCliBridge Plugin — Multi-Location Sync Rule

The plugin exists in three locations:

| Role | Path |
|------|------|
| **Canonical source** (원본) | `unreal-cli-bridge\unreal-plugin\UnrealCliBridge\` |
| **Project copy** (프로젝트 내 사본) | `OwnershipRules\Plugins\UnrealCliBridge\` |
| **Project copy** (프로젝트 내 사본) | `My3DAction\Plugins\UnrealCliBridge\` |

**IMPORTANT: Whenever you modify any plugin source file in any location, you MUST apply the identical change to every other location.** They drift silently otherwise: a command implemented in only one copy is advertised by the CLI everywhere but fails with `Unknown command` in the projects that never got the handler.

Verify with:

```
diff -rq unreal-cli-bridge/unreal-plugin/UnrealCliBridge/Source <project>/Plugins/UnrealCliBridge/Source
```

### Files to keep in sync

- `UnrealCliBridge.uplugin`
- `Source\UnrealCliBridge\UnrealCliBridge.Build.cs`
- `Source\UnrealCliBridge\Private\*.cpp` / `*.h`
- `Source\UnrealCliBridge\Private\Handlers\*.cpp` / `*.h`
- `Source\UnrealCliBridge\Public\*.h`

### Files to NOT sync (build artifacts — project-local only)

- `Binaries\`
- `Intermediate\`

### Workflow

1. Edit the file in whichever location is relevant to the task.
2. Immediately apply the same edit to the corresponding file in the other locations.
3. If a file exists in one location but not the others, create it in all of them.
4. After any plugin source change, tell the user the editor has to be **closed and rebuilt** —
   `unreal-cli.exe compile` runs Unreal's hot reload, which only rebuilds the *game* module and
   never the plugin. Rebuild with:
   `"C:\Program Files\Epic Games\UE_5.3\Engine\Build\BatchFiles\Build.bat" <Project>Editor Win64 Development -Project="<...>.uproject"`
   (this fails while any editor is open, because Live Coding holds a lock on the binaries).
