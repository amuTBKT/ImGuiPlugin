#if 0
//---------------- ASSERT CUSTOMIZATION ----------------//
// imported functions, defined in Unreal module 
extern void GImAssertFunc(bool cond);
extern void GImUserErrorFunc(bool cond, const char* message);

#define IM_ASSERT(_EXPR)                    GImAssertFunc((bool)(_EXPR))
#define IM_ASSERT_USER_ERROR(_EXPR,_MSG)    GImUserErrorFunc((bool)(_EXPR), (const char*)(_MSG))
//---------------- ASSERT CUSTOMIZATION ----------------//
#endif

// imgui
#include "imgui/imgui.cpp"
#include "imgui/imgui_draw.cpp"
#include "imgui/imgui_tables.cpp"
#include "imgui/imgui_widgets.cpp"

#if 0 // TODO: implot is not used atm
// implot
#include "implot/implot.cpp"
#include "implot/implot_items.cpp"
#endif
