# unreal-cli-bridge

Control the Unreal Editor from the command line. No manual server startup — the bridge starts when the Editor opens, and the CLI finds the right project automatically.

```
unreal-cli ──── Named Pipe ──── Unreal Editor (bridge plugin)
               (Windows: \\.\pipe\unreal-cli-<hash>)
```

Average response: ~200 ms.

## Quick Start

### 1. Install the Plugin

Copy `unreal-plugin/UnrealCliBridge/` into your project's `Plugins/` folder (or the engine `Plugins/Editor/` folder), then rebuild the project. The bridge starts automatically when the Editor opens.

### 2. Build the CLI

```powershell
# Windows x64 self-contained binary
.\scripts\publish-win-x64.ps1
# → dist\unreal-cli\unreal-cli.exe
```

Add `dist\unreal-cli\` to your PATH, or copy `unreal-cli.exe` somewhere on your PATH.

### 3. Verify

```bash
unreal-cli status --project C:\UnrealProjects\MyGame
```

---

## What You Can Do

### Editor Control

```bash
unreal-cli status                              # Editor state, UE version, current level
unreal-cli play / pause / stop                 # PIE control
unreal-cli compile                             # Trigger Hot Reload
unreal-cli compile --wait                      # Wait until compilation finishes
unreal-cli refresh                             # Reimport modified assets
unreal-cli read-log --type error               # Read Output Log
unreal-cli screenshot --path C:\shot.png       # Capture viewport
unreal-cli execute-menu --list "Edit"          # List menu items
unreal-cli execute-menu --path "Edit/Undo"     # Execute a menu item

# Execute arbitrary Python (requires Python Script Plugin enabled in project)
unreal-cli execute --code "import unreal; print(unreal.SystemLibrary.get_engine_version())" --force
unreal-cli execute --code "result = 1 + 1" --args '{"key":"value"}' --force
```

### Assets

```bash
unreal-cli asset find --type StaticMesh                      # Find by type
unreal-cli asset find --name Player --type Blueprint         # Find by name + type
unreal-cli asset info --path /Game/Characters/Player         # Asset metadata
unreal-cli asset create --type material --path /Game/Mat/Red # Create material
unreal-cli asset mkdir --path /Game/NewFolder                # Create folder
unreal-cli asset move --from /Game/Old --to /Game/New --force
unreal-cli asset rename --path /Game/OldName --name NewName
unreal-cli asset delete --path /Game/Unused --force
```

### Levels (equivalent to Unity Scenes)

```bash
# Open and inspect
unreal-cli level open --path /Game/Maps/Main
unreal-cli level inspect --with-values

# Add actors
unreal-cli level add-actor --class StaticMeshActor --name MyCube \
  --location 100,0,0 --rotation 0,45,0 --scale 2,2,2

# Transform
unreal-cli level set-transform --actor MyCube --location 200,0,0 --scale 3,3,3

# Component operations
unreal-cli level list-components --actor MyCube
unreal-cli level add-component --actor MyCube --type PointLightComponent
unreal-cli level remove-component --actor MyCube --type PointLightComponent --force

# Material assignment
unreal-cli level assign-material --actor MyCube --material /Game/Materials/Red --slot 0

# Delete
unreal-cli level delete-actor --actor MyCube --force
```

### Blueprints (equivalent to Unity Prefabs)

```bash
unreal-cli blueprint inspect --path /Game/Blueprints/BP_Player --with-values
unreal-cli blueprint set-property --path /Game/Blueprints/BP_Player \
  --property MaxHealth --value 200
```

### Plugins (equivalent to Unity Packages)

```bash
unreal-cli plugin list
unreal-cli plugin enable --name PythonScriptPlugin
unreal-cli plugin disable --name UnwantedPlugin --force
```

### Instance Management

```bash
unreal-cli instances list
unreal-cli instances use C:\UnrealProjects\MyGame
```

---

## Output Formats

```bash
unreal-cli status --json       # Full JSON envelope
unreal-cli status --output compact   # Data payload only
unreal-cli status              # Human-readable default
```

---

## Architecture

```
unreal-cli-bridge/
├── cli/
│   ├── UnrealCli.Protocol/    # Shared protocol models (C#/.NET 9)
│   └── UnrealCli.Cli/         # CLI executable (unreal-cli.exe)
├── unreal-plugin/
│   └── UnrealCliBridge/       # UE5 Editor plugin (C++)
│       └── Source/UnrealCliBridge/
│           ├── Private/
│           │   ├── BridgeHost.cpp         # Startup / registry
│           │   ├── IpcServer.cpp          # Named Pipe server thread
│           │   ├── CommandDispatcher.cpp  # Game-thread dispatch
│           │   ├── InstanceRegistry.cpp   # %APPDATA% registry file
│           │   └── Handlers/
│           │       ├── EditorCommandHandler.cpp
│           │       ├── AssetCommandHandler.cpp
│           │       ├── LevelCommandHandler.cpp
│           │       ├── BlueprintCommandHandler.cpp
│           │       ├── ExecuteCommandHandler.cpp
│           │       └── PluginCommandHandler.cpp
├── scripts/
│   └── publish-win-x64.ps1
└── UnrealCliBridge.sln
```

**Key design decisions:**

- **One hop, no intermediary.** CLI → Named Pipe → Unreal Editor directly.
- **Game thread marshaling.** All UObject/editor API calls are dispatched via `AsyncTask(ENamedThreads::GameThread)` with a promise/future. The IPC thread blocks until the result is ready.
- **Instance registry** at `%APPDATA%\UnrealCliBridge\instances.json` tracks running editors with PID + pipe name. Stale entries (dead process) are pruned automatically.
- **Protocol version check** ensures CLI and plugin are in sync.
- **Force gates** on all destructive operations (delete, remove-component, plugin disable, execute).

---

## Requirements

- **CLI**: .NET 9 SDK (build), Windows 10+ (runtime, self-contained)
- **Plugin**: Unreal Engine 5.x, MSVC 2022, C++17

## Build Commands

```bash
# Build CLI (Debug)
dotnet build UnrealCliBridge.sln -c Debug

# Publish CLI (Release, self-contained)
.\scripts\publish-win-x64.ps1

# Build plugin: open your .uproject in UE5, or use UnrealBuildTool:
# UnrealBuildTool.exe MyProject Win64 Development "C:\Path\To\MyProject.uproject"
```

## Unreal Path Format

Unlike Unity's `Assets/...` paths, Unreal uses content paths:

| Unity | Unreal |
|-------|--------|
| `Assets/Characters/Player.prefab` | `/Game/Characters/Player` |
| `Assets/Materials/Red.mat` | `/Game/Materials/Red` |
| Engine assets | `/Engine/...` |
| Plugin assets | `/PluginName/...` |

## Notes

- PIE (Play In Editor) commands require the editor not to be busy.
- `plugin enable/disable` modifies `.uproject` and requires an editor restart to take effect.
- `execute` requires the **Python Script Plugin** to be enabled in `Edit > Plugins > Scripting`.
- Blueprint `set-property` triggers a Blueprint recompile automatically.
