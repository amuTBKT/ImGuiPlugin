// Copyright 2024 Amit Kumar Mehar. All Rights Reserved.

using System.IO;
using UnrealBuildTool;

public class ImGui : ModuleRules
{
	public ImGui(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		// disabled on Shipping and Server configs
		bool bIsConfigurationSupported = (Target.Configuration != UnrealTargetConfiguration.Shipping);
		bool bIsTargetTypeSupported = ((Target.Type != TargetType.Server) && (Target.Type != TargetType.Program));
		if (bIsConfigurationSupported && bIsTargetTypeSupported)
		{
			PublicDefinitions.Add("WITH_IMGUI=1");

			PublicIncludePaths.Add(Path.Combine(PluginDirectory, "Source/Thirdparty/ImGui"));

			if (Target.Configuration == UnrealTargetConfiguration.Debug)
			{
				if (Target.Platform == UnrealTargetPlatform.Win64)
				{
					PublicAdditionalLibraries.Add(Path.Combine(PluginDirectory, "Source/Thirdparty/ImGui/Binaries/Debug/Win64/ImGui.lib"));
				}
			}
			else
			{
				if (Target.Platform == UnrealTargetPlatform.Win64)
				{
					PublicAdditionalLibraries.Add(Path.Combine(PluginDirectory, "Source/Thirdparty/ImGui/Binaries/Release/Win64/ImGui.lib"));
				}
			}
		}
	}
}
