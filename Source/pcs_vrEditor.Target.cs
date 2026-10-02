using UnrealBuildTool;
using System.Collections.Generic;

public class pcs_vrEditorTarget : TargetRules
{
	public pcs_vrEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.V4;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_3;
		ExtraModuleNames.Add("pcs_vr");
	}
}