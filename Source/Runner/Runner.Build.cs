// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class Runner : ModuleRules
{
	public Runner(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput", "ProceduralMeshComponent", "UMG", "Niagara" });
	}
}
