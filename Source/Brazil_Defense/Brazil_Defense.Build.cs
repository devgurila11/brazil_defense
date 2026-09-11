// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class Brazil_Defense : ModuleRules
{
	public Brazil_Defense(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
	
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput", "DeveloperSettings" });

		// RenderCore and RHI are pulled in by SceneView.h, used by the grid debug canvas pass.
		// AssetRegistry lets the placement console command find a data asset by bare name.
		PrivateDependencyModuleNames.AddRange(new string[] { "RenderCore", "RHI", "AssetRegistry" });

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });
		
		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
