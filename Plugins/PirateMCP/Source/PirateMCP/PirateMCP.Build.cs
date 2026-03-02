using UnrealBuildTool;

public class PirateMCP : ModuleRules
{
	public PirateMCP(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicIncludePaths.AddRange(new string[] {
			System.IO.Path.Combine(ModuleDirectory, "Public"),
			System.IO.Path.Combine(ModuleDirectory, "Public/Commands"),
			System.IO.Path.Combine(ModuleDirectory, "Public/Commands/Editor"),
			System.IO.Path.Combine(ModuleDirectory, "Public/Commands/Blueprint"),
			System.IO.Path.Combine(ModuleDirectory, "Public/Commands/BlueprintGraph"),
			System.IO.Path.Combine(ModuleDirectory, "Public/Commands/BlueprintGraph/Nodes"),
			System.IO.Path.Combine(ModuleDirectory, "Public/Commands/BlueprintGraph/Function"),
			System.IO.Path.Combine(ModuleDirectory, "Public/Commands/Ocean"),
			System.IO.Path.Combine(ModuleDirectory, "Public/Commands/DataTable"),
			System.IO.Path.Combine(ModuleDirectory, "Public/Commands/World"),
			System.IO.Path.Combine(ModuleDirectory, "Public/Commands/Python"),
			System.IO.Path.Combine(ModuleDirectory, "Public/Commands/EditorUtility"),
			System.IO.Path.Combine(ModuleDirectory, "Public/Commands/Asset"),
		});

		PrivateIncludePaths.AddRange(new string[] {
			System.IO.Path.Combine(ModuleDirectory, "Private"),
			System.IO.Path.Combine(ModuleDirectory, "Private/Commands"),
			System.IO.Path.Combine(ModuleDirectory, "Private/Commands/Editor"),
			System.IO.Path.Combine(ModuleDirectory, "Private/Commands/Blueprint"),
			System.IO.Path.Combine(ModuleDirectory, "Private/Commands/BlueprintGraph"),
			System.IO.Path.Combine(ModuleDirectory, "Private/Commands/BlueprintGraph/Nodes"),
			System.IO.Path.Combine(ModuleDirectory, "Private/Commands/BlueprintGraph/Function"),
			System.IO.Path.Combine(ModuleDirectory, "Private/Commands/Ocean"),
			System.IO.Path.Combine(ModuleDirectory, "Private/Commands/DataTable"),
			System.IO.Path.Combine(ModuleDirectory, "Private/Commands/World"),
			System.IO.Path.Combine(ModuleDirectory, "Private/Commands/Python"),
			System.IO.Path.Combine(ModuleDirectory, "Private/Commands/EditorUtility"),
			System.IO.Path.Combine(ModuleDirectory, "Private/Commands/Asset"),
		});

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"Networking",
			"Sockets",
			"HTTP",
			"Json",
			"JsonUtilities",
			"DeveloperSettings",
			"PhysicsCore",
			"UnrealEd",
			"BlueprintGraph",
			"KismetCompiler",
			"Niagara",
			"NiagaraEditor",
		});

		PrivateDependencyModuleNames.AddRange(new string[] {
			"EditorScriptingUtilities",
			"EditorSubsystem",
			"Slate",
			"SlateCore",
			"Kismet",
			"Projects",
			"AssetRegistry",
			"AssetTools",
		});

		// PythonScriptPlugin: NO compile/link dependency needed.
		// Python execution uses the `py` console command at runtime (registered by PythonScriptPlugin).
		// This avoids crashes from module load ordering issues.

		if (Target.bBuildEditor == true)
		{
			PrivateDependencyModuleNames.AddRange(new string[] {
				"PropertyEditor",
				"ToolMenus",
				"BlueprintEditorLibrary",
			});
		}
	}
}
