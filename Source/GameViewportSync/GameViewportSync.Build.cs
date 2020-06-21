// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class GameViewportSync : ModuleRules
{
	public GameViewportSync(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"EditorSubsystem"
			}
			);
			
		
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"Engine",
				"Slate",
				"SlateCore",
				"UnrealEd",
				"LevelEditor",
				"EditorStyle",
				"SceneOutliner"
			}
			);
	}
}
