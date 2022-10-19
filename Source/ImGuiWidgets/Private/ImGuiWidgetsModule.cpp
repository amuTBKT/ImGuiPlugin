#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"

#include "ImGuiPluginTypes.h"

class FImGuiWidgetsModule : public IModuleInterface
{
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};

IMPLEMENT_MODULE(FImGuiWidgetsModule, ImGuiWidgets)

void FImGuiWidgetsModule::StartupModule()
{
	SETUP_DEFAULT_IMGUI_ALLOCATOR();
}

void FImGuiWidgetsModule::ShutdownModule()
{
}