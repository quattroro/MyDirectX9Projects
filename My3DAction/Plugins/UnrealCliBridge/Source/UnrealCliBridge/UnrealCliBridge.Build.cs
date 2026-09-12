using UnrealBuildTool;
using System.IO;

public class UnrealCliBridge : ModuleRules
{
	public UnrealCliBridge(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		bUseUnity = false;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"CoreUObject",
			"Engine",
			"Slate",
			"SlateCore",
			"UnrealEd",
			"AssetRegistry",
			"AssetTools",
			"LevelEditor",
			"Json",
			"JsonUtilities",
			"Projects",
			"ToolMenus",
			"EditorFramework",
			"EditorSubsystem",
			"BlueprintGraph",
			"Kismet",
			"KismetCompiler",
			"HotReload",
			"AnimGraphRuntime",
			"AnimGraph",
			"MaterialEditor",

			// Behavior Tree editing.
			// BehaviorTreeEditor and AIGraph declare every dependency privately, so nothing comes
			// through transitively — anything they expose has to be listed here explicitly. Their
			// Classes/ folders become public include paths automatically (UnrealBuildTool does that
			// in UEBuildModuleCPP.AddDefaultIncludePaths), which is why the headers are reachable
			// even though neither module has a Public/ folder for them.
			"AIModule",
			"AIGraph",
			"BehaviorTreeEditor",
			"GraphEditor",
			"GameplayTasks",
		});

		// Python Script Plugin is optional — only link if available in this engine build
		if (Target.bBuildEditor)
		{
			var PythonPlugin = Path.Combine(EngineDirectory, "Plugins", "Experimental", "PythonScriptPlugin");
			var PythonPluginAlt = Path.Combine(EngineDirectory, "Plugins", "Editor", "PythonScriptPlugin");
			if (Directory.Exists(PythonPlugin) || Directory.Exists(PythonPluginAlt))
			{
				PrivateDependencyModuleNames.Add("PythonScriptPlugin");
				PublicDefinitions.Add("WITH_PYTHON_SCRIPT_PLUGIN=1");
			}
			else
			{
				PublicDefinitions.Add("WITH_PYTHON_SCRIPT_PLUGIN=0");
			}
		}
	}
}
