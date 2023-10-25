#pragma once

#include "CoreMinimal.h"
#include "ImGuiRuntimeModule.h"

// Static widgets are registered at ModuleInit, so we cannot use logic that relies on initialization order.

#define IMGUI_REGISTER_STATIC_WIDGET(InitFunction, TickFunction)									\
    struct FAtModuleInit																			\
    {																								\
        FAtModuleInit()																				\
        {																							\
			if (FImGuiRuntimeModule::IsPluginInitialized)											\
			{																						\
				FImGuiRuntimeModule& ImGuiRuntimeModule =											\
					FModuleManager::GetModuleChecked<FImGuiRuntimeModule>("ImGuiRuntime");			\
				InitFunction();																		\
				ImGuiRuntimeModule.GetMainWindowTickDelegate().AddStatic(&TickFunction);			\
			}																						\
            else																					\
			{																						\
				FImGuiRuntimeModule::OnPluginInitialized.AddLambda(									\
					[](FImGuiRuntimeModule& ImGuiRuntimeModule)										\
					{																				\
						InitFunction();																\
						ImGuiRuntimeModule.GetMainWindowTickDelegate().AddStatic(&TickFunction);	\
					});																				\
            }																						\
        }																							\
    };																								\
    static FAtModuleInit AtModuleInit = {};