using System.IO;

namespace UnrealBuildTool.Rules
{
	public class ImGuiPlugin : ModuleRules
	{
		public ImGuiPlugin(ReadOnlyTargetRules Target) : base(Target)
		{
			PublicDefinitions.Add("WITH_IMGUI=1");

			PublicIncludePaths.AddRange(
				new string[]
				{
					Path.Combine(ModuleDirectory, "Public/imgui"),
				}
			);

			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"CoreUObject",
					"Engine",
					"InputCore",
					"Slate",
					"SlateCore",
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
}
