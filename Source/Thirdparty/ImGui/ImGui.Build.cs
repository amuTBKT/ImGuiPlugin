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
		}
		else
		{
			PublicDefinitions.Add("WITH_IMGUI=0");
        }

		PublicDefinitions.Add("IMGUI_DISABLE_OBSOLETE_FUNCTIONS=1");
		PublicDefinitions.Add("IMGUI_DEFINE_MATH_OPERATORS=1");

		PublicIncludePaths.Add(Path.Combine(PluginDirectory, "Source/Thirdparty/ImGui/imgui"));
		PublicIncludePaths.Add(Path.Combine(PluginDirectory, "Source/Thirdparty/ImGui/implot"));
		PublicIncludePaths.Add(Path.Combine(PluginDirectory, "Source/Thirdparty/ImGui/remoteimgui"));

		if (Target.Configuration == UnrealTargetConfiguration.Debug)
		{
			PublicAdditionalLibraries.Add(Path.Combine(PluginDirectory, "Source/Thirdparty/ImGui/Binaries/Debug/ImGui.lib"));
		}
		else
        {
			PublicAdditionalLibraries.Add(Path.Combine(PluginDirectory, "Source/Thirdparty/ImGui/Binaries/Release/ImGui.lib"));
		}
	}
}
