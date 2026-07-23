// Copyright 2024-26 Amit Kumar Mehar. All Rights Reserved.

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
				"Core",
				"Slate",
				"SlateCore",
				"InputCore",
				"RenderCore",
				"CoreUObject",
				"ApplicationCore",
			}
		);

		if (Target.bCompileAgainstEngine)
		{
			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"Engine",
					"RHI",
					"ImGuiShaders"
				}
			);
		}

		if ((Target.Type != TargetType.Server) && Target.bCompileFreeType)
		{
			AddEngineThirdPartyPrivateStaticDependencies(Target, "FreeType2", "UElibPNG", "zlib");
		}
		PublicDependencyModuleNames.Add("ImGui");

		// need to include private header files for accessing slate resource
		var EngineDir = Path.GetFullPath(Target.RelativeEnginePath);
		PrivateIncludePaths.Add(Path.Combine(EngineDir, "Source/Runtime/SlateRHIRenderer/Private"));

		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"UnrealEd",
					"ToolMenus",
					"EditorStyle",
					"LevelEditor",
					"WorkspaceMenuStructure",
				}
			);
		}
	}
}
