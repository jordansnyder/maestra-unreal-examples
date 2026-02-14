using UnrealBuildTool;
using System.Collections.Generic;

public class MaestraUnrealCleanTarget : TargetRules
{
	public MaestraUnrealCleanTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.V6;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_7;
		ExtraModuleNames.AddRange(new string[] { "MaestraUnrealClean" });
	}
}
