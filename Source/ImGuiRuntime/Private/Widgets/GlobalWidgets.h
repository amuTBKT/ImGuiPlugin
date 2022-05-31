#pragma once

#include "ImGuiRuntimeModule.h"

// GlobalWidgets are registered at ModuleInit, so we cannot use logic that relies on initialization order.

#define IMGUI_REGISTER_GLOBAL_WIDGET(InitFunction, TickFunction)                            \
    struct FAtModuleInit                                                                    \
    {                                                                                       \
        FAtModuleInit()                                                                     \
        {                                                                                   \
            FImGuiRuntimeModule::OnPluginInitialized.AddLambda(                             \
                [](FImGuiRuntimeModule& ImGuiRuntimeModule)                                 \
                {                                                                           \
                    InitFunction(ImGuiRuntimeModule);                                       \
                    ImGuiRuntimeModule.GetMainWindowTickDelegate().AddStatic(&TickFunction);\
                }                                                                           \
            );                                                                              \
        }                                                                                   \
    };                                                                                      \
    static FAtModuleInit AtModuleInit = {};