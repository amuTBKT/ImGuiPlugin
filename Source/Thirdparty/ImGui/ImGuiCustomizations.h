// Copyright 2024-25 Amit Kumar Mehar. All Rights Reserved.

#pragma once

//-------------------- Config Customization --------------------//
#define IMGUI_DISABLE_OBSOLETE_FUNCTIONS	1
#define IMGUI_DEFINE_MATH_OPERATORS			1
#define IMGUI_DISABLE_DEFAULT_ALLOCATORS	1
#define IMGUI_DISABLE_DEMO_WINDOWS			1
//-------------------- Config Customization --------------------//

//-------------------- Assert Customization --------------------//
#if 0
// imported functions, defined in Unreal module 
extern void GImAssertFunc(bool cond);

#define IM_ASSERT(_EXPR)	GImAssertFunc((bool)(_EXPR))
#endif
//-------------------- Assert Customization --------------------//

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
