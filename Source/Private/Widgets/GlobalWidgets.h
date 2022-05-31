#pragma once

#include "ImGuiPluginModule.h"

// GlobalWidgets are registered at ModuleInit, so we cannot use logic that relies on initialization order.

#define IMGUI_REGISTER_GLOBAL_WIDGET(InitFunction, TickFunction)                            \
    struct FAtModuleInit                                                                    \
    {                                                                                       \
        FAtModuleInit()                                                                     \
        {                                                                                   \
            FImGuiPluginModule::OnPluginInitialized.AddLambda(                              \
                [](FImGuiPluginModule& ImGuiPlugin)                                         \
                {                                                                           \
                    InitFunction(ImGuiPlugin);                                              \
                    ImGuiPlugin.GetMainWindowTickDelegate().AddStatic(&TickFunction);       \
                }                                                                           \
            );                                                                              \
        }                                                                                   \
    };                                                                                      \
    static FAtModuleInit AtModuleInit = {};