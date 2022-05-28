#pragma once

#include "ImGuiPluginModule.h"

// GlobalWidgets are registered at ModuleInit, so we cannot use logic that relies on initialization order.

#define IMGUI_REGISTER_GLOBAL_WIDGET(InitFunction, TickFunction)                            \
    struct FAtModuleInit                                                                    \
    {                                                                                       \
        FAtModuleInit()                                                                     \
        {                                                                                   \
            InitFunction();                                                                 \
            FImGuiPluginModule::OnPluginInitialized.AddLambda(                              \
                [](FImGuiPluginModule& ImGuiPlugin)                                         \
                {                                                                           \
                    ImGuiPlugin.GetMainWindowTickDelegate().AddStatic(&TickFunction);     \
                }                                                                           \
            );                                                                              \
        }                                                                                   \
    };                                                                                      \
    static FAtModuleInit AtModuleInit = {};