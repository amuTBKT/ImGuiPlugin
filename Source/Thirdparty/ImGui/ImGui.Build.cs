// Copyright 2024-25 Amit Kumar Mehar. All Rights Reserved.

using System.IO;
using UnrealBuildTool;

public class ImGui : ModuleRules
{
	public ImGui(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		PublicDefinitions.Add("WITH_IMGUI=1");

		// NOTE: some imgui files prefer "imgui.h" include instead of "imgui/imgui.h"
		PublicIncludePaths.Add(Path.Combine(PluginDirectory, "Source/Thirdparty/ImGui"));
		PublicIncludePaths.Add(Path.Combine(PluginDirectory, "Source/Thirdparty/ImGui/imgui"));

		string LibPath = null;
		// static and dynamic lib have similar performance in non editor builds so prefer dynamic linking
		if (Target.Type == TargetType.Editor)
		{
			if (Target.Configuration == UnrealTargetConfiguration.Debug)
			{
				if (Target.Platform == UnrealTargetPlatform.Win64)
				{
					LibPath = Path.Combine(PluginDirectory, "Source/Thirdparty/ImGui/Binaries/Debug/Win64/ImGui.lib");
				}
			}
			else
			{
				if (Target.Platform == UnrealTargetPlatform.Win64)
				{
					LibPath = Path.Combine(PluginDirectory, "Source/Thirdparty/ImGui/Binaries/Release/Win64/ImGui.lib");
				}
			}
		}
		if (File.Exists(LibPath))
		{
			PublicDefinitions.Add("WITH_IMGUI_STATIC_LIB=1");
			PublicAdditionalLibraries.Add(LibPath);
		}
	}
}
