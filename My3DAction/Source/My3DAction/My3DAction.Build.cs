// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class My3DAction : ModuleRules
{
	public My3DAction(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput" });
	}
}
