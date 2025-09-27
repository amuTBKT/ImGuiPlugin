// Copyright 2024 Amit Kumar Mehar. All Rights Reserved.

using System.IO;
using UnrealBuildTool;

public class ImGuiRuntime : ModuleRules
{
	public ImGuiRuntime(ReadOnlyTargetRules Target) : base(Target)
	{
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"RHI",
				"Core",
				"Slate",
				"Engine",
                "Projects",
                "SlateCore",
				"InputCore",
				"RenderCore",
				"CoreUObject",
				"ImGuiShaders"
            }
		);

		PublicDependencyModuleNames.AddRange( new string[] { "ImGui" });

		// need to include private header files for accessing slate resource
		var EngineDir = Path.GetFullPath(Target.RelativeEnginePath);
		PrivateIncludePaths.Add(Path.Combine(EngineDir, "Source/Runtime/SlateRHIRenderer/Private"));

		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"EditorStyle",
				}
			);
		}
	}
}
