// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class PirateGame : ModuleRules
{
	public PirateGame(ReadOnlyTargetRules Target) : base(Target)
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
			"Slate"
		});

		PrivateDependencyModuleNames.AddRange(new string[] { });

		PublicIncludePaths.AddRange(new string[] {
			"PirateGame",
			"PirateGame/Variant_Platforming",
			"PirateGame/Variant_Platforming/Animation",
			"PirateGame/Variant_Combat",
			"PirateGame/Variant_Combat/AI",
			"PirateGame/Variant_Combat/Animation",
			"PirateGame/Variant_Combat/Gameplay",
			"PirateGame/Variant_Combat/Interfaces",
			"PirateGame/Variant_Combat/UI",
			"PirateGame/Variant_SideScrolling",
			"PirateGame/Variant_SideScrolling/AI",
			"PirateGame/Variant_SideScrolling/Gameplay",
			"PirateGame/Variant_SideScrolling/Interfaces",
			"PirateGame/Variant_SideScrolling/UI"
		});

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
