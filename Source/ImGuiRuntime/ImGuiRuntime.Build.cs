using System.IO;
using UnrealBuildTool;

public class ImGuiRuntime : ModuleRules
{
	public ImGuiRuntime(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicDefinitions.Add("WITH_IMGUI=1");

		PublicIncludePaths.AddRange(
			new string[]
			{
				Path.Combine(ModuleDirectory, "../../Thirdparty/ImGui/imgui"),
			}
		);

		if (Target.Configuration == UnrealTargetConfiguration.Debug)
		{
			PublicAdditionalLibraries.AddRange(
				new string[]
				{
					Path.Combine(ModuleDirectory, "../../Thirdparty/ImGui/libs/Debug/ImGui.lib"),
				}
			);
		}
		else
        {
			PublicAdditionalLibraries.AddRange(
				new string[]
				{
					Path.Combine(ModuleDirectory, "../../Thirdparty/ImGui/libs/Release/ImGui.lib"),
				}
			);
		}

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"InputCore",
				"Slate",
				"SlateCore",
				"RenderCore",
				"RHI",
				"ImGuiShaders",
			}
		);

		var EngineDir = Path.GetFullPath(Target.RelativeEnginePath);
		PrivateIncludePaths.AddRange(
			new string[]
			{
				Path.Combine(EngineDir, "Source/Runtime/SlateRHIRenderer/Private")
			}
		);

		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"UnrealEd",
					"EditorStyle",
					"Blutility",
					"SourceControl",
					"UncontrolledChangelists",
				}
			);
		}
	}
}