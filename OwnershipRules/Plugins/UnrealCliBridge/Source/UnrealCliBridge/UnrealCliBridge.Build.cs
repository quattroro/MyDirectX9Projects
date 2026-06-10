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
