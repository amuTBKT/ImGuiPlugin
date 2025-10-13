// Copyright 2024-25 Amit Kumar Mehar. All Rights Reserved.

using System.IO;

namespace UnrealBuildTool.Rules
{
	public class ImGuiShaders : ModuleRules
	{
		public ImGuiShaders(ReadOnlyTargetRules Target) : base(Target)
		{
            PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

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
