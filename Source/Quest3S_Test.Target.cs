using UnrealBuildTool;
using System.Collections.Generic;
public class Quest3S_TestTarget : TargetRules
{
    public Quest3S_TestTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Game;
        DefaultBuildSettings = BuildSettingsVersion.V7;
        ExtraModuleNames.Add("Quest3S_Test");
    }
}
