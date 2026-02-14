using UnrealBuildTool;

public class MaestraUnrealClean : ModuleRules
{
	public MaestraUnrealClean(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core", "CoreUObject", "Engine", "InputCore", "MaestraPlugin",
			// Visualization systems
			"ProceduralMeshComponent",
			"Niagara", "NiagaraCore",
			"RenderCore", "RHI",
			"Sockets", "Networking",
			"Json", "JsonUtilities",
			"AudioMixer", "AudioCapture"
		});

		PrivateDependencyModuleNames.AddRange(new string[] {  });
	}
}
