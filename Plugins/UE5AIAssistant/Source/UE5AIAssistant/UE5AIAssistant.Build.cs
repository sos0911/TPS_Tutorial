using UnrealBuildTool;

public class UE5AIAssistant : ModuleRules
{
	public UE5AIAssistant(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"EngineSettings",
			"UnrealEd",
			"HTTP",
			"HTTPServer",
			"Json",
			"JsonUtilities",
			"InputCore",
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Slate",
			"SlateCore",
			"EditorFramework",
			"LevelEditor",
			"BlueprintGraph",
			"KismetCompiler",
			"Kismet",
			"AssetTools",
			"AssetRegistry",
			"MaterialEditor",
			"AnimGraph",
			"AnimationBlueprintEditor",
			"Persona",
			"EnhancedInput",
			"UMG",
			"UMGEditor",
		});
	}
}
