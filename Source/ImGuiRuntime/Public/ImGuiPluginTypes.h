// Copyright 2024-25 Amit Kumar Mehar. All Rights Reserved.

#pragma once

#include "Textures/SlateIcon.h"

#include "ImGuiCustomizations.h"

// Not ideal but we include internal header here as some files do need access to it and we would like all includes to be directed through "ImGuiPluginTypes.h"
// This is to ensure macros are consistent b/w static ImGui.lib and runtime code
#include "imgui/imgui_internal.h"

// since the module is built as DLL, we need to register allocators for each module that makes ImGui calls, usually at module startup
#define SETUP_DEFAULT_IMGUI_ALLOCATOR()                                                         \
	ImGui::SetAllocatorFunctions(                                                               \
	/*Alloc*/    [](size_t Size, void* UserData = nullptr) { return FMemory::Malloc(Size); },   \
	/*Free*/     [](void* Pointer, void* UserData = nullptr) { FMemory::Free(Pointer); },       \
	/*UserData*/ nullptr);

#define IMGUI_FNAME(Name) [](){ static FName StaticFName(Name); return StaticFName; }()

// returns FSlateBrush for specified icon and style name
#define IMGUI_STYLE_ICON(StyleName, IconName) [](){ static const FSlateBrush* Brush = FSlateIcon(FName(StyleName), FName(IconName)).GetIcon(); return Brush; }()

class FDragDropOperation;
struct FImGuiTickContext
{
	ImGuiContext* ImGuiContext = nullptr;

	// inputs
	TSharedPtr<FDragDropOperation> DragDropOperation = nullptr;
	bool bDragDropOperationReleasedThisFrame = false;

	// outputs
	bool bWasDragDropOperationConsumed = false;

	TSharedPtr<FDragDropOperation> TryConsumeDragDropOperation()
	{
		TSharedPtr<FDragDropOperation> DragDropOp;
		if (bDragDropOperationReleasedThisFrame && DragDropOperation.IsValid())
		{
			bWasDragDropOperationConsumed = true;
			Swap(DragDropOp, DragDropOperation);
		}
		return DragDropOp;
	}
};

// since modules can be added as DLL, we need to set context before making ImGui calls.
struct FImGuiTickScope : FNoncopyable
{
	explicit FImGuiTickScope(FImGuiTickContext* Context)
	{
		ImGui::SetCurrentContext(Context->ImGuiContext);
	}
	~FImGuiTickScope()
	{
		ImGui::SetCurrentContext(nullptr);
	}
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

#define ImDrawCallback_SetShaderState (ImDrawCallback)(-2)
using FImGuiShaderState = void*;

enum class EImGuiShaderState : uint32
{
	Default				 = 0u,
	DisableAlphaBlending = 1u << 0,  // disable alpha writes from shader (outputs Color.a=1)
};
ENUM_CLASS_FLAGS(EImGuiShaderState);

static FORCEINLINE FImGuiShaderState MakeImGuiShaderState(EImGuiShaderState ShaderState)
{
	return (FImGuiShaderState)ShaderState;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////

// params used to register an ImGui widget as standalone or main window widget
struct FStaticWidgetRegisterParams
{
	void(*InitFunction)(void);
	void(*TickFunction)(FImGuiTickContext* Context);
	FSlateIcon WidgetIcon;
	const char* WidgetName = nullptr;
	const char* WidgetDescription = nullptr;
	bool bEnableViewports = true;

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
static FAutoRegisterMainWindowWidget UE_JOIN(AtModuleInit, __LINE__) = { RegisterParams };

#if WITH_EDITOR
#define IMGUI_REGISTER_STANDALONE_WIDGET(RegisterParams)				\
static FAutoRegisterStandaloneWidget UE_JOIN(AtModuleInit, __LINE__) = { RegisterParams };
#else
// at runtime push standalone widgets to the main window
#define IMGUI_REGISTER_STANDALONE_WIDGET(RegisterParams)				\
static FAutoRegisterMainWindowWidget UE_JOIN(AtModuleInit, __LINE__) = { RegisterParams };
#endif
