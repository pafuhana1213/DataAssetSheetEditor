// Copyright 2026 pafuhana1213. All Rights Reserved.

using UnrealBuildTool;
using System.Collections.Generic;

public class DataAssetSheetEditorSampleEditorTarget : TargetRules
{
	public DataAssetSheetEditorSampleEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.AddRange(new string[] { "DataAssetSheetEditorSample" });
	}
}
