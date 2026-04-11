// Copyright 2026 pafuhana1213. All Rights Reserved.

using UnrealBuildTool;

public class DataAssetSheetEditor : ModuleRules
{
	public DataAssetSheetEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

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
			}
		);
	}
}
