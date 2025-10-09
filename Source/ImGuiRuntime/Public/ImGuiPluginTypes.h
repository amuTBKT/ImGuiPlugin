// Copyright 2024 Amit Kumar Mehar. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

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

// commonly used function to get icon for the default app style
#define IMGUI_ICON(IconName) IMGUI_STYLE_ICON("ImGuiStyle", IconName)

static constexpr ImU32 FColorToImU32(const FColor& Color)
{
	return IM_COL32(Color.R, Color.G, Color.B, Color.A);
}

struct FImGuiTickContext
{
	ImGuiContext* ImGuiContext = nullptr;

	// inputs
	TSharedPtr<class FDragDropOperation> DragDropOperation = nullptr;
	bool bApplyDragDropOperation = false;

	// outputs
	bool bWasDragDropOperationHandled = false;

	bool ConsumeDragDropOperation()
	{
		if (bApplyDragDropOperation && DragDropOperation.IsValid())
		{
			DragDropOperation.Reset();
			bWasDragDropOperationHandled = true;

			return true;
		}
		return false;
	}
};

// since the module is built as DLL, we need to set context before making ImGui calls.
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

// scope to resolve widget label/name collisions
struct FImGuiNamedWidgetScope final : FNoncopyable
{
	explicit FImGuiNamedWidgetScope(const char* ScopeName)
	{
		ImGui::PushID(ScopeName);
	}
	explicit FImGuiNamedWidgetScope(int32 ScopeId)
	{
		ImGui::PushID(ScopeId);
	}
	explicit FImGuiNamedWidgetScope(const TCHAR* ScopeName)
		: FImGuiNamedWidgetScope(FCrc::StrCrc32(ScopeName))
	{
	}
	explicit FImGuiNamedWidgetScope(uint32 ScopeId)
		: FImGuiNamedWidgetScope(static_cast<int32>(ScopeId))
	{
	}

	~FImGuiNamedWidgetScope()
	{
		ImGui::PopID();
	}
};

// params used to create Image widget, works for slate icons too (they are atlased)
struct FImGuiImageBindingParams
{
	ImVec2 Size = ImVec2(1.f, 1.f);
	ImVec2 UV0 = ImVec2(0.f, 0.f);
	ImVec2 UV1 = ImVec2(1.f, 1.f);
	ImTextureID Id = 0;
};

#define ImDrawCallback_SetRenderState (ImDrawCallback)(-2)
using FImGuiRenderState = void*;

enum class EImGuiRenderState : uint32
{
	Default				 = 0,
	DisableAlphaBlending = 1 << 0,  // disable alpha writes from shader (outputs Color.a=1)
};
ENUM_CLASS_FLAGS(EImGuiRenderState);

FORCEINLINE FImGuiRenderState MakeImGuiRenderState(EImGuiRenderState RenderState)
{
	return (FImGuiRenderState)RenderState;
}
