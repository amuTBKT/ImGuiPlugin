// Copyright 2024-26 Amit Kumar Mehar. All Rights Reserved.

///////////////////////////////////////////////////////////////////////////////////////////////////////

#include "ImGuiPluginTypes.h"

#ifdef WITH_IMGUI_STATIC_LIB
void ImGuiAssertHook(bool bCondition, const char* Expression, const char* File, uint32 Line)
{
	if (bCondition)
	{
		return;
	}

	static TSet<uint32> EncounteredAsserts;
	uint32 Key = HashCombine(PointerHash(File), GetTypeHash(Line));
	if (!EncounteredAsserts.Contains(Key)) // fire once, similar to `ensure(expr)`
	{
		EncounteredAsserts.Add(Key);

		FPlatformMisc::LowLevelOutputDebugString(*FString::Printf(TEXT("Ensure condition failed: %hs [File: %hs] [Line: %u]\n"), Expression, File, Line));
		if (FPlatformMisc::IsDebuggerPresent())
		{
			PLATFORM_BREAK();
		}
	}
}
#else
#include "ImGuiLib.cpp"
#endif

///////////////////////////////////////////////////////////////////////////////////////////////////////

#if !WITH_ENGINE

#include "Modules/ModuleManager.h"
#include "Modules/ModuleInterface.h"

#include "ImGuiSubsystem.h"

FAutoRegisterMainMenuWidget::FAutoRegisterMainMenuWidget(FImGuiWidgetRegisterParams RegisterParams) {}
FAutoRegisterStandaloneWidget::FAutoRegisterStandaloneWidget(FImGuiWidgetRegisterParams RegisterParams) {}

class FImGuiRuntimeModule : public IModuleInterface
{
private:
	virtual void StartupModule() override
	{
		if (!UImGuiSubsystem::ShouldEnableImGui())
		{
			return;
		}

		IMGUI_CHECKVERSION();
		IMGUI_SETUP_DEFAULT_ALLOCATOR();

		UImGuiSubsystem::InitializeSubsystem();
	}

	virtual void ShutdownModule() override
	{
	}
};
IMPLEMENT_MODULE(FImGuiRuntimeModule, ImGuiRuntime)

#endif //#if WITH_ENGINE
