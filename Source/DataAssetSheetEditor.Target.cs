// Copyright 2026 pafuhana1213. All Rights Reserved.

using UnrealBuildTool;
using System.Collections.Generic;

public class DataAssetSheetEditorTarget : TargetRules
{
	public DataAssetSheetEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.AddRange(new string[] { "DataAssetSheetEditorSample" });
	}
}
