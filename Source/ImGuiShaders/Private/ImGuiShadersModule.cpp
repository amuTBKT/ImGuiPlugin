// Copyright 2024-25 Amit Kumar Mehar. All Rights Reserved.

#include "Misc/Paths.h"
#include "ShaderCore.h"
#include "ImGuiShaders.h"
#include "Modules/ModuleManager.h"
#include "Modules/ModuleInterface.h"
#include "Interfaces/IPluginManager.h"

IMPLEMENT_GLOBAL_SHADER(FImGuiVS, "/Plugin/ImGui/ImGuiShader.usf", "MainVS", SF_Vertex);
IMPLEMENT_GLOBAL_SHADER(FImGuiPS, "/Plugin/ImGui/ImGuiShader.usf", "MainPS", SF_Pixel);

class FImGuiShadersModule : public IModuleInterface
{
	virtual void StartupModule() override
	{
		const FString PluginShaderDir = FPaths::Combine(IPluginManager::Get().FindPlugin(TEXT("ImGuiPlugin"))->GetBaseDir(), TEXT("Shaders"));
		AddShaderSourceDirectoryMapping(TEXT("/Plugin/ImGui"), PluginShaderDir);
	}
	
	virtual void ShutdownModule() override
	{
	}
};

IMPLEMENT_MODULE(FImGuiShadersModule, ImGuiShaders)
