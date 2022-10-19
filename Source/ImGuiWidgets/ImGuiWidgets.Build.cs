using System.IO;
using UnrealBuildTool;

public class ImGuiWidgets : ModuleRules
{
	public ImGuiWidgets(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				// needed modules
				"Core",
				"CoreUObject",
				"Engine",
				"Slate",
				"SlateCore",
				"RHI",
				"RenderCore",
				"ImGui",
				"ImGuiRuntime",

				// widget specific modules
				"Niagara",
			}
		);

		if (Target.bBuildEditor)
		{
			// widget specific modules
			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"UnrealEd",
					"AssetRegistry"
				}
			);
		}

		// need to include private header files for accessing NiagaraGpuComputeDispatch.h
		var EngineDir = Path.GetFullPath(Target.RelativeEnginePath);
		PrivateIncludePaths.Add(Path.Combine(EngineDir, "Plugins/FX/Niagara/Source/Niagara/Private"));
	}
}