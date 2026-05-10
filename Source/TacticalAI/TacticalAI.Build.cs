// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class TacticalAI : ModuleRules
{
	public TacticalAI(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"AIModule",
			"StateTreeModule",
			"GameplayStateTreeModule",
			"UMG",
			"Slate",
			"AIModule",          // AIController 위해
			"NavigationSystem",  // NavMesh 쿼리 위해 (이미 쓰고 있음)
			"GameplayTasks"      // AI Tasks 위해
		});

		PrivateDependencyModuleNames.AddRange(new string[] { });

		PublicIncludePaths.AddRange(new string[] {
			"TacticalAI",
			"TacticalAI/Variant_Platforming",
			"TacticalAI/Variant_Platforming/Animation",
			"TacticalAI/Variant_Combat",
			"TacticalAI/Variant_Combat/AI",
			"TacticalAI/Variant_Combat/Animation",
			"TacticalAI/Variant_Combat/Gameplay",
			"TacticalAI/Variant_Combat/Interfaces",
			"TacticalAI/Variant_Combat/UI",
			"TacticalAI/Variant_SideScrolling",
			"TacticalAI/Variant_SideScrolling/AI",
			"TacticalAI/Variant_SideScrolling/Gameplay",
			"TacticalAI/Variant_SideScrolling/Interfaces",
			"TacticalAI/Variant_SideScrolling/UI"
		});

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
