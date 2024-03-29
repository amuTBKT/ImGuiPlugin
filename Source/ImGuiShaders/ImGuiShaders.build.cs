using System.IO;

namespace UnrealBuildTool.Rules
{
	public class ImGuiShaders : ModuleRules
	{
		public ImGuiShaders(ReadOnlyTargetRules Target) : base(Target)
		{
			PublicDependencyModuleNames.AddRange(
	            new string[]
	            {
	                "Core",
	                "CoreUObject",
	                "Projects",
	                "Engine",
				}
	        );

			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"RHI",
					"RenderCore",
					"Renderer",
				}
			);
        }
	}
}
