// Copyright 2026 pafuhana1213. All Rights Reserved.

using UnrealBuildTool;

public class DataAssetSheetEditor : ModuleRules
{
	public DataAssetSheetEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		// Public サブフォルダをインクルードパスに追加 / Add Public subdirectories to include paths
		PublicIncludePaths.AddRange(
			new string[]
			{
				System.IO.Path.Combine(ModuleDirectory, "Public", "Assets"),
				System.IO.Path.Combine(ModuleDirectory, "Public", "Models"),
				System.IO.Path.Combine(ModuleDirectory, "Public", "Toolkits"),
				System.IO.Path.Combine(ModuleDirectory, "Public", "Widgets"),
			}
		);

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"Engine",
				"Slate",
				"SlateCore",
				"InputCore",
				"UnrealEd",
				"EditorFramework",
				"ToolMenus",
				"PropertyEditor",
				"AssetRegistry",
				"ContentBrowser",
				"DesktopPlatform",
				"AssetDefinition",
				"CollectionManager",
				"AssetTools",
				"ApplicationCore",
				"Json",
				"GameplayTags",
			}
		);
	}
}
