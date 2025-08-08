// Copyright 2024 Amit Kumar Mehar. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Textures/SlateIcon.h"

// Static widgets are registered at ModuleInit, so we cannot use logic that relies on initialization order.

struct ImGuiContext;

struct FStaticWidgetRegisterParams
{
	void(*InitFunction)(void);
	void(*TickFunction)(ImGuiContext* Context);
	FSlateIcon WidgetIcon;
	const char* WidgetName = nullptr;
	const char* WidgetDescription = nullptr;

	bool IsValid() const
	{
		return InitFunction && TickFunction && WidgetName && WidgetDescription;
	}
};

// adds widget to the main ImGui window
struct FAutoRegisterMainWindowWidget
{
	IMGUIRUNTIME_API FAutoRegisterMainWindowWidget(FStaticWidgetRegisterParams RegisterParams);
};

// creates a new tab for displaying widget
struct FAutoRegisterStandaloneWidget
{
	IMGUIRUNTIME_API FAutoRegisterStandaloneWidget(FStaticWidgetRegisterParams RegisterParams);
};

#define IMGUI_REGISTER_MAIN_WINDOW_WIDGET(RegisterParams)				\
static FAutoRegisterMainWindowWidget PREPROCESSOR_JOIN(AtModuleInit, __LINE__) = { RegisterParams };

#if WITH_EDITOR
#define IMGUI_REGISTER_STANDALONE_WIDGET(RegisterParams)				\
static FAutoRegisterStandaloneWidget PREPROCESSOR_JOIN(AtModuleInit, __LINE__) = { RegisterParams };
#else
// at runtime push static widgets to the main window
#define IMGUI_REGISTER_STANDALONE_WIDGET(RegisterParams)				\
static FAutoRegisterMainWindowWidget PREPROCESSOR_JOIN(AtModuleInit, __LINE__) = { RegisterParams };
#endif
