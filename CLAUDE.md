# Project Overview

This repository contains Unreal Engine 5 projects and the `unreal-cli-bridge` tool for controlling the Unreal Editor from the command line.

## unreal-cli — Unreal Editor CLI Tool

**Executable path:**
```
C:\Codes\Git\MyDirectX9Projects\unreal-cli-bridge\dist\unreal-cli\unreal-cli.exe
```

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
- `--force` is required for destructive operations (delete, overwrite)
- `execute` requires the Python Script Plugin enabled in the project
- Rebuilding the CLI: run `unreal-cli-bridge\scripts\publish-win-x64.bat`
- Plugin source: `OwnershipRules\Plugins\UnrealCliBridge\`

## Projects

- **OwnershipRules** — Main UE5 project (`OwnershipRules\OwnershipRules.uproject`)
- **unreal-cli-bridge** — CLI tool + UE5 plugin source (`unreal-cli-bridge\`)

## UnrealCliBridge Plugin — Dual-Location Sync Rule

The plugin exists in two locations:

| Role | Path |
|------|------|
| **Canonical source** (원본) | `unreal-cli-bridge\unreal-plugin\UnrealCliBridge\` |
| **Project copy** (프로젝트 내 사본) | `OwnershipRules\Plugins\UnrealCliBridge\` |

**IMPORTANT: Whenever you modify any plugin source file in either location, you MUST apply the identical change to the corresponding file in the other location.**

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
2. Immediately apply the same edit to the corresponding file in the other location.
3. If a file exists in one location but not the other, create it in both.
4. After any plugin source change, remind the user to recompile (`unreal-cli.exe compile --wait`) so the editor picks up the new code.
