// Copyright 2026 pafuhana1213. All Rights Reserved.

using UnrealBuildTool;
using System.Collections.Generic;

public class DataAssetSheetEditorEditorTarget : TargetRules
{
	public DataAssetSheetEditorEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.AddRange(new string[] { "DataAssetSheetEditorSample" });
	}
}
