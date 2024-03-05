#pragma once

#include "imgui.h"

// since the module is built as DLL, we need to register allocators for each module that makes ImGui calls, usually at module startup
#define SETUP_DEFAULT_IMGUI_ALLOCATOR()                                                         \
    ImGui::SetAllocatorFunctions(                                                               \
    /*Alloc*/    [](size_t Size, void* UserData = nullptr) { return FMemory::Malloc(Size); },   \
    /*Free*/     [](void* Pointer, void* UserData = nullptr) { FMemory::Free(Pointer); },       \
    /*UserData*/ nullptr);

// since the module is built as DLL, we need to set context before making ImGui calls.
struct FImGuiTickScope final
{
    explicit FImGuiTickScope(ImGuiContext* Context)
    {
        ImGui::SetCurrentContext(Context);
    }
    ~FImGuiTickScope()
    {
        ImGui::SetCurrentContext(nullptr);
    }

    FImGuiTickScope(const FImGuiTickScope&) = delete;
    FImGuiTickScope(FImGuiTickScope&&) = delete;
    
    FImGuiTickScope& operator=(const FImGuiTickScope&) = delete;
    FImGuiTickScope& operator=(FImGuiTickScope&&) = delete;
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
    DisableAlphaBlending = 1 << 0,  // disable alpha writes from shader (outputs Color.a=1)
};
ENUM_CLASS_FLAGS(EImGuiRenderState);

FORCEINLINE FImGuiRenderState MakeImGuiRenderState(EImGuiRenderState RenderState)
{
    return (FImGuiRenderState)RenderState;
}
