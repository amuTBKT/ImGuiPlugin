// Copyright 2024-25 Amit Kumar Mehar. All Rights Reserved.

#pragma once

// static lib requires adding a function hook to enable asserts (can't rely on ensure/check macros)
#if !defined(IMGUI_UNREAL_API) || defined(WITH_IMGUI_STATIC_LIB)
#define IMGUI_ENABLE_ASSERT_HOOK 1
#endif

// `IMGUI_UNREAL_API` is defined when compiling with unreal engine
#ifdef IMGUI_UNREAL_API
	#ifndef WITH_IMGUI_STATIC_LIB
		#define IMGUI_API IMGUI_UNREAL_API
		#define IMPLOT_API IMGUI_UNREAL_API
		#define IM_ASSERT(expr) ensure(expr)
	#endif

	#if WITH_FREETYPE
		#define IMGUI_ENABLE_FREETYPE 1
	#endif
#else
	// not compiling with unreal, define common types
	#define IMGUI_UNREAL_API

	#include <cstdint>
	using uint32 = uint32_t;
	using uint64 = uint64_t;
#endif //#ifdef IMGUI_UNREAL_API

#ifdef IMGUI_ENABLE_ASSERT_HOOK
	IMGUI_UNREAL_API void ImGuiAssertHook(bool bCondition, const char* Expression, const char* File, uint32 Line);
	#define IM_ASSERT(expr) ImGuiAssertHook((bool)(expr), #expr, __FILE__, __LINE__)
#endif

//-------------------- Config Customization --------------------//
#define IMGUI_DISABLE_OBSOLETE_FUNCTIONS				1
#define IMGUI_DEFINE_MATH_OPERATORS						1
#define IMGUI_DISABLE_DEFAULT_ALLOCATORS				1
#define IMGUI_DISABLE_DEFAULT_SHELL_FUNCTIONS			1
#define IMGUI_DISABLE_WIN32_DEFAULT_CLIPBOARD_FUNCTIONS	1
#define IMGUI_DISABLE_WIN32_FUNCTIONS					1
#define IMGUI_DISABLE_DEFAULT_FILE_FUNCTIONS			1

#ifdef IMGUI_DISABLE_DEFAULT_FILE_FUNCTIONS
using ImFileHandle = class IFileHandle*;
IMGUI_UNREAL_API ImFileHandle ImFileOpen(const char* FileName, const char* Mode);
IMGUI_UNREAL_API bool ImFileClose(ImFileHandle File);
IMGUI_UNREAL_API uint64 ImFileGetSize(ImFileHandle File);
IMGUI_UNREAL_API uint64 ImFileRead(void* Data, uint64 Size, uint64 Count, ImFileHandle File);
IMGUI_UNREAL_API uint64 ImFileWrite(const void* Data, uint64 Size, uint64 Count, ImFileHandle File);
#endif
//-------------------- Config Customization --------------------//

//-------------------- DrawCallback Customization --------------------//
// TODO: it is unsafe to switch render targets/graphics stage (using compute shaders) in the callback.
// Since the widget is rendered using RenderPass, would have to wrap the callback inside End/Begin render pass stages.

/**
 * Callback which can be used to render directly into the ImGui Slate window
 * 
 * @param RHICmdList	: FRHICommandListImmediate used to render the ImGui widget.
 * @param DrawRect		: Draw rect inside the Slate window.
 * @param UserData		: User data passed to ImDrawList::AddCallback(...) function.
 * @param UserDataSize	: Size of user data, not used internally but can be used for validation.
 */
typedef void (*FImGuiDrawCallback)(class FRHICommandListImmediate& RHICmdList, const struct ImRect& DrawRect, void* UserData, size_t UserDataSize);

// Override ImGui's callback to use the new type defined above.
#define ImDrawCallback FImGuiDrawCallback
//-------------------- DrawCallback Customization --------------------//
