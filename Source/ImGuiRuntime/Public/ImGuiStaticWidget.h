// Copyright 2024 Amit Kumar Mehar. All Rights Reserved.

#pragma once

// Static widgets are registered at ModuleInit, so we cannot use logic that relies on initialization order.

struct FSlateIcon;
struct ImGuiContext;

// adds widget to the main ImGui window
struct FAutoRegisterMainWindowWidget
{
	IMGUIRUNTIME_API FAutoRegisterMainWindowWidget(void(*InitFunction)(void), void(*TickFunction)(ImGuiContext* Context));
};

// creates a new tab for displaying widget
struct FAutoRegisterStandaloneWidget
{
	struct FParams
	{
		void(*InitFunction)(void);
		void(*TickFunction)(ImGuiContext* Context);
		const FSlateIcon& TabIcon;
		const char* TabName = nullptr;
		const char* TabTooltip = nullptr;

		bool IsValid() const
		{
			return InitFunction && TickFunction && TabName && TabTooltip;
		}
	};
	IMGUIRUNTIME_API FAutoRegisterStandaloneWidget(FParams RegisterParams);
};

#define IMGUI_REGISTER_MAIN_WINDOW_WIDGET(InitFunction, TickFunction)   \
static FAutoRegisterMainWindowWidget PREPROCESSOR_JOIN(AtModuleInit, __LINE__) = { InitFunction, TickFunction };

#if WITH_EDITOR
#define IMGUI_REGISTER_STANDALONE_WIDGET(RegsiterParams)				\
static FAutoRegisterStandaloneWidget PREPROCESSOR_JOIN(AtModuleInit, __LINE__) = { RegsiterParams };
#else
// at runtime push static widgets to the main window
#define IMGUI_REGISTER_STANDALONE_WIDGET(RegsiterParams)				\
static FAutoRegisterMainWindowWidget PREPROCESSOR_JOIN(AtModuleInit, __LINE__) = { RegsiterParams.InitFunction, RegsiterParams.TickFunction };
#endif
