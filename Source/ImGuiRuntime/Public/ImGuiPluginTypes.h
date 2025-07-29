// Copyright 2024 Amit Kumar Mehar. All Rights Reserved.

#pragma once

#include "imgui.h"

// since the module is built as DLL, we need to register allocators for each module that makes ImGui calls, usually at module startup
#define SETUP_DEFAULT_IMGUI_ALLOCATOR()                                                         \
    ImGui::SetAllocatorFunctions(                                                               \
    /*Alloc*/    [](size_t Size, void* UserData = nullptr) { return FMemory::Malloc(Size); },   \
    /*Free*/     [](void* Pointer, void* UserData = nullptr) { FMemory::Free(Pointer); },       \
    /*UserData*/ nullptr);

#define IMGUI_FNAME(Name) [](){ static FName StaticFName(Name); return StaticFName; }()

// returns FSlateBrush for specified icon and style name
#define IMGUI_ICON(StyleName, IconName) [](){ static const FSlateBrush* Brush = FSlateIcon(FName(StyleName), FName(IconName)).GetIcon(); return Brush; }()

// commonly used function to get icon for the default app style
#define IMGUI_EDITOR_ICON(IconName) IMGUI_ICON("EditorStyle", IconName)

// since the module is built as DLL, we need to set context before making ImGui calls.
struct FImGuiTickScope final : FNoncopyable
{
    explicit FImGuiTickScope(ImGuiContext* Context)
    {
        ImGui::SetCurrentContext(Context);
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

enum class EImGuiRenderState : uint32_t
{
    Default              = 0,
    DisableAlphaBlending = 1 << 0,  // disable alpha writes from shader (outputs Color.a=1)
};
ENUM_CLASS_FLAGS(EImGuiRenderState);

FORCEINLINE FImGuiRenderState MakeImGuiRenderState(EImGuiRenderState RenderState)
{
    return (FImGuiRenderState)RenderState;
}
