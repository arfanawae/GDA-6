using UnrealBuildTool;

public class Downtown : ModuleRules
{
	public Downtown(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicIncludePaths.Add(ModuleDirectory);

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core", "CoreUObject", "Engine", "InputCore",
			"EnhancedInput",
			"ChaosVehicles", "PhysicsCore",
			"AIModule", "NavigationSystem", "GameplayTasks",
			"UMG", "Slate", "SlateCore",
			"MotionWarping"
		});
	}
}
