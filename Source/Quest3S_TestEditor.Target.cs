using UnrealBuildTool;
using System.Collections.Generic;
public class Quest3S_TestEditorTarget : TargetRules
{
    public Quest3S_TestEditorTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Editor;
        DefaultBuildSettings = BuildSettingsVersion.V7;
        ExtraModuleNames.Add("Quest3S_Test");
    }
}
