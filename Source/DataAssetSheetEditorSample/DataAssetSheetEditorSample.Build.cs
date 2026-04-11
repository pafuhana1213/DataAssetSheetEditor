// Copyright 2026 pafuhana1213. All Rights Reserved.

using UnrealBuildTool;

public class DataAssetSheetEditorSample : ModuleRules
{
	public DataAssetSheetEditorSample(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
			}
		);
	}
}
