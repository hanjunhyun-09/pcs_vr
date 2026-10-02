using UnrealBuildTool;
using System.Collections.Generic;

public class pcs_vrTarget : TargetRules
{
	public pcs_vrTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.V4;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_3;
		ExtraModuleNames.Add("pcs_vr");
	}
}