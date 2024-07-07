// Copyright 2024 Amit Kumar Mehar. All Rights Reserved.

#pragma once

#include "ImGuiSubsystem.h"

// Static widgets are registered at ModuleInit, so we cannot use logic that relies on initialization order.

#define IMGUI_REGISTER_STATIC_WIDGET(InitFunction, TickFunction)									\
    struct FAtModuleInit																			\
    {																								\
        FAtModuleInit()																				\
        {																							\
			if (UImGuiSubsystem* Subsystem = UImGuiSubsystem::Get())								\
			{																						\
				InitFunction();																		\
				Subsystem->GetMainWindowTickDelegate().AddStatic(&TickFunction);					\
			}																						\
            else																					\
			{																						\
				UImGuiSubsystem::OnSubsystemInitialized().AddLambda(								\
				[](UImGuiSubsystem* Subsystem)														\
				{																					\
					InitFunction();																	\
					Subsystem->GetMainWindowTickDelegate().AddStatic(&TickFunction);				\
				});																					\
            }																						\
        }																							\
    };																								\
    static FAtModuleInit AtModuleInit = {};