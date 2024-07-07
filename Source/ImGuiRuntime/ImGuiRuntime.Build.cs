// Copyright 2024 Amit Kumar Mehar. All Rights Reserved.

using System.IO;
using UnrealBuildTool;

public class ImGuiRuntime : ModuleRules
{
	public ImGuiRuntime(ReadOnlyTargetRules Target) : base(Target)
	{
		//PCHUsage = PCHUsageMode.NoPCHs;

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"InputCore",
				"Slate",
				"SlateCore",
				"RHI",
				"RenderCore",
			}
		);

		PublicDependencyModuleNames.AddRange( new string[] { "ImGui", "ImGuiShaders" });

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
