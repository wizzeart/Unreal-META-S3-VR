using UnrealBuildTool;

public class Quest3S_Test : ModuleRules
{
    public Quest3S_Test(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new[] { "Core", "CoreUObject", "Engine", "InputCore", "UMG" });
        PrivateDependencyModuleNames.AddRange(new[] { "HTTP", "Json", "ImageWrapper", "ProceduralMeshComponent", "Slate", "SlateCore", "HeadMountedDisplay", "EnhancedInput" });
        if (Target.Platform == UnrealTargetPlatform.Android)
        {
            PrivateDependencyModuleNames.AddRange(new[] { "ApplicationCore", "Launch" });
            AdditionalPropertiesForReceipt.Add("AndroidPlugin", System.IO.Path.Combine(ModuleDirectory, "HouseSandbox_UPL.xml"));
        }
    }
}
