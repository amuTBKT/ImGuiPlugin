// Copyright 2024-25 Amit Kumar Mehar. All Rights Reserved.

#pragma once

#include "Textures/SlateIcon.h"

#define IMGUI_UNREAL_API IMGUIRUNTIME_API
#include "ImGuiCustomizations.h"

// Not ideal but we include internal header here as some files do need access to it and we would like all includes to be directed through "ImGuiPluginTypes.h"
// This is to ensure macros are consistent b/w static ImGui.lib and runtime code
#include "imgui/imgui_internal.h"

#include "implot/implot.h"

// since the module is built as DLL, we need to register allocators for each module that makes ImGui calls, usually at module startup
#define SETUP_DEFAULT_IMGUI_ALLOCATOR()                                                         \
	ImGui::SetAllocatorFunctions(                                                               \
	/*Alloc*/    [](size_t Size, void* UserData = nullptr) { return FMemory::Malloc(Size); },   \
	/*Free*/     [](void* Pointer, void* UserData = nullptr) { FMemory::Free(Pointer); },       \
	/*UserData*/ nullptr);

#define IMGUI_FNAME(Name) [](){ static FName StaticFName(Name); return StaticFName; }()

// returns FSlateBrush for specified icon and style name
#define IMGUI_STYLE_ICON_BRUSH(StyleName, IconName) []() -> const FSlateBrush* { static const FSlateBrush* Brush = FSlateIcon(FName(StyleName), FName(IconName)).GetOptionalIcon(); return Brush; }()
// returns FSlateIcon for specified icon and style name
#define IMGUI_STYLE_ICON(StyleName, IconName)  []() -> const FSlateIcon&  { static const FSlateIcon Icon = FSlateIcon(FName(StyleName), FName(IconName)); return Icon; }()

class FDragDropOperation;
struct FImGuiTickContext
{
	ImGuiContext* ImguiContext = nullptr;
	ImPlotContext* ImplotContext = nullptr;

	// inputs
	TSharedPtr<FDragDropOperation> DragDropOperation = nullptr;
	bool bDragDropOperationReleasedThisFrame = false;

	// drawing remotely to NetImGui server
	bool bIsDrawingRemotely = false;

	TSharedPtr<FDragDropOperation> TryConsumeDragDropOperation()
	{
		TSharedPtr<FDragDropOperation> DragDropOp;
		if (bDragDropOperationReleasedThisFrame && DragDropOperation.IsValid())
		{
			Swap(DragDropOp, DragDropOperation);
		}
		return DragDropOp;
	}

	static FImGuiTickContext* GetTickContext(ImGuiContext* ImguiContext)
	{
		return ImguiContext ? (FImGuiTickContext*)ImguiContext->IO.UserData : nullptr;
	}
	static void StoreTickContext(FImGuiTickContext* TickContext, ImGuiContext* ImguiContext)
	{
		check(ImguiContext);
		ImguiContext->IO.UserData = TickContext;
	}
};

// since modules can be added as DLL, we need to set context before making ImGui calls.
struct FImGuiTickScope : FNoncopyable
{
	explicit FImGuiTickScope(FImGuiTickContext* Context)
		: PrevContext(BeginContext(Context))
	{
	}
	~FImGuiTickScope()
	{
		EndContext(PrevContext);
		PrevContext = nullptr;
	}

	FORCEINLINE static FImGuiTickContext* BeginContext(FImGuiTickContext* Context)
	{
		ImGuiContext* PrevImGuiContext = ImGui::GetCurrentContext();
		
		ImGui::SetCurrentContext(Context ? Context->ImguiContext : nullptr);
		ImPlot::SetCurrentContext(Context ? Context->ImplotContext : nullptr);

		return FImGuiTickContext::GetTickContext(PrevImGuiContext);
	}
	FORCEINLINE static void EndContext(FImGuiTickContext* PrevContext)
	{
		ImGui::SetCurrentContext(PrevContext ? PrevContext->ImguiContext : nullptr);
		ImPlot::SetCurrentContext(PrevContext ? PrevContext->ImplotContext : nullptr);
	}

	FImGuiTickContext* PrevContext = nullptr;
};

// scope to resolve label/name conflicts
struct FImGuiNamedScope final : FNoncopyable
{
	explicit FImGuiNamedScope(int32 ScopeId)
	{
		ImGui::PushID(ScopeId);
	}
	explicit FImGuiNamedScope(uint32 ScopeId)
	{
		ImGui::PushID(static_cast<int32>(ScopeId));
	}
	explicit FImGuiNamedScope(const char* ScopeName)
	{
		ImGui::PushID(ScopeName);
	}
	explicit FImGuiNamedScope(const TCHAR* ScopeName)
	{
		ImGui::PushID(static_cast<int32>(FCrc::StrCrc32(ScopeName)));
	}
	explicit FImGuiNamedScope(const FName& ScopeName)
	{
		ImGui::PushID(static_cast<int32>(GetTypeHash(ScopeName)));
	}
	~FImGuiNamedScope()
	{
		ImGui::PopID();
	}
};

// params used to create image widgets, works for slate icons too (they are atlased)
struct FImGuiImageBindingParams
{
	ImVec2 Size = ImVec2(1.f, 1.f);
	ImVec2 UV0 = ImVec2(0.f, 0.f);
	ImVec2 UV1 = ImVec2(1.f, 1.f);
	ImTextureID Id = 0u;
};

///////////////////////////////////////////////////////////////////////////////////////////////////////

#define ImDrawCallback_ResetRenderState (ImDrawCallback)(-1)
#define ImDrawCallback_SetShaderState	(ImDrawCallback)(-2)
using FImGuiShaderState = void*;

enum class EImGuiShaderState : uint32
{
	Default				 = 0u,
	OutputInSRGB		 = 1u << 0,	 // source texture is in sRGB format (internal use only)
	DisableAlphaBlending = 1u << 1,  // disable alpha writes from shader (outputs Color.a=1)
};
ENUM_CLASS_FLAGS(EImGuiShaderState);

static FORCEINLINE FImGuiShaderState MakeImGuiShaderState(EImGuiShaderState ShaderState)
{
	return (FImGuiShaderState)ShaderState;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////

// params used to register an ImGui widget as standalone or main menu widget
struct FImGuiWidgetRegisterParams
{
	// ImGui widget init function (called during module load)
	void(*InitFunction)(void);

	// ImGui widget tick function
	void(*TickFunction)(FImGuiTickContext* Context);
	
	// optional icon to use for the widget menu item
	FSlateIcon WidgetIcon;
	
	// full path to widget (example: "Tools.MyWidget")
	const char* WidgetPath = nullptr;
	
	// widget tooltip
	const char* WidgetDescription = "";
	
	// to enable ImGui viewport support for the widget
	bool bEnableViewports = true;
	
	// for drawing widget directly in the menubar (only valid when adding widget to the main menu)
	bool bTickInMenuBar = false;

	const char* GetWidetName() const
	{
		if (!ensureAlways(WidgetPath))
		{
			return nullptr;
		}

		int32 WidgetNameOffset = FAnsiStringView(WidgetPath).FindLastChar('.', WidgetNameOffset) ? WidgetNameOffset + 1 : 0;
		return WidgetPath + WidgetNameOffset;
	}

	bool IsValid() const
	{
		return InitFunction && TickFunction && WidgetPath && WidgetDescription;
	}
};

// adds widget to the main ImGui window
struct FAutoRegisterMainMenuWidget
{
	IMGUIRUNTIME_API FAutoRegisterMainMenuWidget(FImGuiWidgetRegisterParams RegisterParams);
};

// adds widget to a new editor docktab
struct FAutoRegisterStandaloneWidget
{
	IMGUIRUNTIME_API FAutoRegisterStandaloneWidget(FImGuiWidgetRegisterParams RegisterParams);
};

#define IMGUI_REGISTER_MAIN_MENU_WIDGET(RegisterParams)					\
static FAutoRegisterMainMenuWidget UE_JOIN(AtModuleInit, __LINE__) = { RegisterParams };

#if WITH_EDITOR
#define IMGUI_REGISTER_STANDALONE_WIDGET(RegisterParams)				\
static FAutoRegisterStandaloneWidget UE_JOIN(AtModuleInit, __LINE__) = { RegisterParams };
#else
// at runtime push standalone widgets to the menu
#define IMGUI_REGISTER_STANDALONE_WIDGET(RegisterParams)				\
static FAutoRegisterMainMenuWidget UE_JOIN(AtModuleInit, __LINE__) = { RegisterParams };
#endif
