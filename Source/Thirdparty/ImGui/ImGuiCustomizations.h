#pragma once

//-------------------- Assert Customization --------------------//
#if 0
// imported functions, defined in Unreal module 
extern void GImAssertFunc(bool cond);

#define IM_ASSERT(_EXPR)	GImAssertFunc((bool)(_EXPR))
#endif
//-------------------- Assert Customization --------------------//

//-------------------- DrawCallback Customization --------------------//
// NOTE: current setup assumes viewport is always (0, 0, ImGui::GetIO().DisplaySize),
// Callers can store ViewportSize and ClipRects as UserData if needed.

/**
 * Callback which can be used to render directly into ImGui window/render-target
 * 
 * @param immediate_command_list : FRHICommandListImmediate pointer used to render the ImGui widget.
 * @param user_data : User data passed to ImDrawList::AddCallback(...) function.
 * @param user_data_size : Size of user data, not used internally but can be used for validation.
 */
typedef void (*FImGuiDrawCallback)(void* immediate_command_list, void* user_data, size_t user_data_size);

// Override ImGui's callback to use the new type defined above.
#define ImDrawCallback FImGuiDrawCallback
//-------------------- DrawCallback Customization --------------------//
