// Copyright 2024-26 Amit Kumar Mehar. All Rights Reserved.

#pragma once

#include "Textures/SlateIcon.h"

#include "imgui/imgui_internal.h"
#include "implot/implot.h"

// since the module is built as DLL, we need to register allocators for each module that makes ImGui calls, usually at module startup
#define IMGUI_SETUP_DEFAULT_ALLOCATOR()                                                         \
	ImGui::SetAllocatorFunctions(                                                               \
	/*Alloc*/    [](size_t Size, void* UserData = nullptr) { return FMemory::Malloc(Size); },   \
	/*Free*/     [](void* Pointer, void* UserData = nullptr) { FMemory::Free(Pointer); },       \
	/*UserData*/ nullptr);

#define IMGUI_FNAME(Name) [](){ static FName StaticName(Name); return StaticName; }()

// returns FSlateBrush for specified icon and style name
#define IMGUI_STYLE_ICON_BRUSH(StyleName, IconName) []() -> const FSlateBrush* { static const FSlateBrush* Brush = FSlateIcon(FName(StyleName), FName(IconName)).GetOptionalIcon(); return Brush; }()
// returns FSlateIcon for specified icon and style name
#define IMGUI_STYLE_ICON(StyleName, IconName)  []() -> const FSlateIcon&  { static const FSlateIcon Icon = FSlateIcon(FName(StyleName), FName(IconName)); return Icon; }()

// whether we can draw locally, disabled when running headless (NetImGui will be used for remote drawing if enabled)
#define IMGUI_ALLOW_LOCAL_DRAWING (USE_NULL_RHI == 0)

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

	// updating the main menu bar
	// TODO: is there a better way to detect if we are inside `BeginMainMenuBar`/`EndMainMenuBar` block?
	bool  MainMenuBar_bIsTicking = false;
	bool  MainMenuBar_bTickingRightAlignedItems = false;
	float MainMenuBar_Height = 0.f;
	float MainMenuBar_RightDirOffsetX = 0.f;
	float MainMenuBar_RightDirOffsetY = 0.f;
	float MainMenuBar_RightDirCursorPosX = 0.f;

	// allow callback to modify enabled state (alternative to UImGuiSubsystem::GetMainMenuWidgetActiveState(...))
	bool* MainMenuBar_CurrentItemEnabledState = nullptr;

	// this is different from `AllocateSpaceForRightAlignedMenuWidget` as ImGui::BeginMenu has some custom logic to handle item spacing
	bool AllocateSpaceForRightAlignedMenuItem(const char* Label)
	{
		if (!ensure(MainMenuBar_bIsTicking && MainMenuBar_bTickingRightAlignedItems))
		{
			return false;
		}

		const float ItemSpacing = ImGui::GetStyle().ItemSpacing.x;

		float LabelSize = ImGui::CalcTextSize(Label, ImGui::FindRenderedTextEnd(Label), false).x;
		LabelSize += ItemSpacing * 2.f - 1.f;
		if (MainMenuBar_RightDirCursorPosX - LabelSize > MainMenuBar_RightDirOffsetX)
		{
			MainMenuBar_RightDirCursorPosX = MainMenuBar_RightDirCursorPosX - LabelSize;

			ImGui::SetCursorPosX(MainMenuBar_RightDirCursorPosX + ItemSpacing - 1.f);
			ImGui::SetCursorPosY(MainMenuBar_RightDirOffsetY);
			return true;
		}

		return false;
	}

	bool AllocateSpaceForRightAlignedMenuWidget(float RequestedWidth)
	{
		if (!ensure(MainMenuBar_bIsTicking && MainMenuBar_bTickingRightAlignedItems))
		{
			return false;
		}

		if (MainMenuBar_RightDirCursorPosX - RequestedWidth > MainMenuBar_RightDirOffsetX)
		{
			const float ItemSpacing = ImGui::GetStyle().ItemSpacing.x;
			MainMenuBar_RightDirCursorPosX = MainMenuBar_RightDirCursorPosX - RequestedWidth - ItemSpacing;

			ImGui::SetCursorPosX(MainMenuBar_RightDirCursorPosX + ItemSpacing);
			ImGui::SetCursorPosY(MainMenuBar_RightDirOffsetY);
			return true;
		}

		return false;
	}

	TSharedPtr<FDragDropOperation> TryConsumeDragDropOperation()
	{
		TSharedPtr<FDragDropOperation> DragDropOp;
		if (bDragDropOperationReleasedThisFrame && DragDropOperation.IsValid())
		{
			Swap(DragDropOp, DragDropOperation);
		}
		return DragDropOp;
	}

	static FImGuiTickContext* GetTickContextFromImGuiContext(ImGuiContext* ImguiContext)
	{
		return ImguiContext ? (FImGuiTickContext*)ImguiContext->IO.UserData : nullptr;
	}
	static void SetTickContextUserData(ImGuiContext* ImguiContext, FImGuiTickContext* InTickContext)
	{
		check(ImguiContext);
		ImguiContext->IO.UserData = InTickContext;
	}
};

// since modules can be added as DLL, we need to set context before making ImGui calls.
struct FImGuiTickScope : FNoncopyable
{
	explicit FImGuiTickScope(FImGuiTickContext* Context)
		: PrevContext(BeginContext(Context))
		, bRestoreContext(PrevContext != Context)
	{
	}
	~FImGuiTickScope()
	{
		if (bRestoreContext)
		{
			EndContext(PrevContext);
		}
		PrevContext = nullptr;
	}

	FORCEINLINE static FImGuiTickContext* BeginContext(FImGuiTickContext* Context)
	{
		FImGuiTickContext* PrevContext = FImGuiTickContext::GetTickContextFromImGuiContext(ImGui::GetCurrentContext());
		if (PrevContext != Context)
		{
			ImGui::SetCurrentContext(Context ? Context->ImguiContext : nullptr);
			ImPlot::SetCurrentContext(Context ? Context->ImplotContext : nullptr);
		}
		else if (PrevContext)
		{
			check(PrevContext->ImguiContext == ImGui::GetCurrentContext());
			check(PrevContext->ImplotContext == ImPlot::GetCurrentContext());
		}
		return PrevContext;
	}
	FORCEINLINE static void EndContext(FImGuiTickContext* PrevContext)
	{
		ImGui::SetCurrentContext(PrevContext ? PrevContext->ImguiContext : nullptr);
		ImPlot::SetCurrentContext(PrevContext ? PrevContext->ImplotContext : nullptr);
	}

	FImGuiTickContext* PrevContext = nullptr;
	bool bRestoreContext = false;
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
	ImVec2 Size = ImVec2(0.f, 0.f);
	ImVec2 UV0  = ImVec2(0.f, 0.f);
	ImVec2 UV1  = ImVec2(1.f, 1.f);
	ImTextureRef Id;

	ImTextureID GetTexID()	const { return Id.GetTexID(); }
	bool		IsValid()	const { return GetTexID() != ImTextureID_Invalid; }
};

///////////////////////////////////////////////////////////////////////////////////////////////////////

namespace FImGui
{
	// utility function to allow adding icon to menu item
	FORCEINLINE bool MenuItem(FImGuiTickContext* TickContext, const char* Label, bool bIsActive, const FImGuiImageBindingParams& Icon, ImVec2 IconOffset = ImVec2(0.f, 0.f))
	{
		const float ItemSpacing = ImGui::GetStyle().ItemSpacing.x;
		const float HalfItemSpacing = ItemSpacing * 0.5f;
		const float FramePadding = ImGui::GetStyle().FramePadding.x;
		const bool bIsHorizontalMenu = ImGui::GetCurrentWindow()->DC.LayoutType == ImGuiLayoutType_Horizontal;

		const bool bIsIconOnlyItem = FCStringAnsi::Strstr(Label, "##") == Label;
		ensureMsgf(bIsHorizontalMenu || !bIsIconOnlyItem, TEXT("Icon only menu item (%hs) should only be used with horizontal menus"), Label);

		// label name with padding for icon
		TAnsiStringBuilder<128> LabelBuffer;
		if (Icon.Size.x > 0)
		{
			// TODO: only tested with Roboto font
			const float WhitespaceCharSize = ImGui::CalcTextSize("\u200A").x;
			float WhitespaceWidth = bIsHorizontalMenu ? Icon.Size.x : Icon.Size.x + FramePadding;
			if (bIsIconOnlyItem)
			{
				// TODO: this is probably a hack :p
				WhitespaceWidth = FMath::Max(0.f, WhitespaceWidth - ItemSpacing);
			}
			const int32 WhitespaceCount = FMath::TruncToInt(WhitespaceWidth / WhitespaceCharSize);
			for (int32 i = 0; i < WhitespaceCount; ++i)
			{
				LabelBuffer.Append("\u200A");
			}
		}
		LabelBuffer.Append(Label);

		if (TickContext->MainMenuBar_bTickingRightAlignedItems && !TickContext->AllocateSpaceForRightAlignedMenuItem(*LabelBuffer))
		{
			return false;
		}

		if (bIsHorizontalMenu)
		{
			ImGui::SetCursorPosX(ImGui::GetCursorPosX() - HalfItemSpacing);
			ImGui::BeginGroup();
			ImGui::SetCursorPosX(ImGui::GetCursorPosX() + HalfItemSpacing);
		}
		else
		{
			ImGui::BeginGroup();
		}

		float CursorPosX = ImGui::GetCursorPosX();
		bool bPressed = ImGui::MenuItem(*LabelBuffer, nullptr, bIsActive);
		const float MenuItemExtentMaxX = ImGui::GetItemRectMax().x;

		if (Icon.Size.x > 0)
		{
			ImGui::SameLine();
			ImGui::SetCursorPosX(CursorPosX);
			ImGui::SetCursorPos(ImGui::GetCursorPos() + IconOffset);
			if (Icon.GetTexID() != ImTextureID_Invalid)
			{
				ImGui::Image(Icon.Id, Icon.Size, Icon.UV0, Icon.UV1);
			}
			else
			{
				ImGui::Dummy(ImVec2(Icon.Size.x, Icon.Size.y));
			}

			// make sure LastItemRectMax matches menu item's rect
			if (bIsHorizontalMenu)
			{
				ImGui::Dummy(ImVec2(MenuItemExtentMaxX - ImGui::GetItemRectMax().x - ItemSpacing, 0.f));
			}
		}
		ImGui::EndGroup();
		if (bIsHorizontalMenu)
		{
			ImGui::SetCursorPosX(ImGui::GetCursorPosX() - HalfItemSpacing);
		}

		return bPressed;
	}

	// utility function to allow adding icon to sub menu
	template <typename MenuCallback>
	FORCEINLINE bool SubMenu(FImGuiTickContext* TickContext, const char* Label, MenuCallback MenuFunc, const FImGuiImageBindingParams& ExpandedIcon, const FImGuiImageBindingParams& CollapsedIcon, ImVec2 IconOffset = ImVec2(0.f, 0.f))
	{
		const float ItemSpacing = ImGui::GetStyle().ItemSpacing.x;
		const float HalfItemSpacing = ItemSpacing * 0.5f;
		const float FramePadding = ImGui::GetStyle().FramePadding.x;
		const bool bIsHorizontalMenu = ImGui::GetCurrentWindow()->DC.LayoutType == ImGuiLayoutType_Horizontal;

		const bool bIsIconOnlyItem = FCStringAnsi::Strstr(Label, "##") == Label;
		ensureMsgf(bIsHorizontalMenu || !bIsIconOnlyItem, TEXT("Icon only menu item (%hs) should only be used with horizontal menus"), Label);

		// label name with padding for icon
		// NOTE: "[Icon] Label" has the same ID as "Label"
		// this is to ensure calling ImGui::BeginMenu("Label") finds the same menu as FImGui::SubMenu("Label", ...)
		TAnsiStringBuilder<256> LabelBuffer;
		if (ExpandedIcon.Size.x > 0)
		{
			// TODO: only tested with Roboto font
			const float WhitespaceCharSize = ImGui::CalcTextSize("\u200A").x;
			float WhitespaceWidth = bIsHorizontalMenu ? ExpandedIcon.Size.x : ExpandedIcon.Size.x + FramePadding;
			if (bIsIconOnlyItem)
			{
				// TODO: this is probably a hack :p
				WhitespaceWidth = FMath::Max(0.f, WhitespaceWidth - ItemSpacing);
			}
			const int32 WhitespaceCount = FMath::TruncToInt(WhitespaceWidth / WhitespaceCharSize);
			for (int32 i = 0; i < WhitespaceCount; ++i)
			{
				LabelBuffer.Append("\u200A");
			}
		}
		if (FCStringAnsi::Strstr(Label, "##"))
		{
			LabelBuffer.Append(Label);
		}
		else
		{
			LabelBuffer.Appendf("%s###%s", Label);
		}

		if (TickContext->MainMenuBar_bTickingRightAlignedItems && !TickContext->AllocateSpaceForRightAlignedMenuItem(*LabelBuffer))
		{
			return false;
		}

		if (bIsHorizontalMenu)
		{
			ImGui::SetCursorPosX(ImGui::GetCursorPosX() - HalfItemSpacing);
			ImGui::BeginGroup();
			ImGui::SetCursorPosX(ImGui::GetCursorPosX() + HalfItemSpacing);
		}
		else
		{
			ImGui::BeginGroup();
		}

		float CursorPosX = ImGui::GetCursorPosX();
		const bool bOpen = ImGui::BeginMenu(*LabelBuffer);
		const float MenuItemExtentMaxX = ImGui::GetItemRectMax().x;
		if (bOpen)
		{
			MenuFunc();
			ImGui::EndMenu();
		}

		if (ExpandedIcon.Size.x > 0)
		{
			ImGui::SameLine();
			ImGui::SetCursorPosX(CursorPosX);
			ImGui::SetCursorPos(ImGui::GetCursorPos() + IconOffset);
			if (bOpen)
			{
				ImGui::Image(ExpandedIcon.Id, ExpandedIcon.Size, ExpandedIcon.UV0, ExpandedIcon.UV1);
			}
			else
			{
				ImGui::Image(CollapsedIcon.Id, CollapsedIcon.Size, CollapsedIcon.UV0, CollapsedIcon.UV1);
			}

			// make sure LastItemRectMax matches menu item's rect
			if (bIsHorizontalMenu)
			{
				ImGui::Dummy(ImVec2(MenuItemExtentMaxX - ImGui::GetItemRectMax().x - ItemSpacing, 0.f));
			}
		}
		ImGui::EndGroup();
		if (bIsHorizontalMenu)
		{
			ImGui::SetCursorPosX(ImGui::GetCursorPosX() - HalfItemSpacing);
		}

		return bOpen;
	}

	template <typename MenuCallback>
	FORCEINLINE bool SubMenu(FImGuiTickContext* TickContext, const char* Label, MenuCallback MenuFunc, const FImGuiImageBindingParams& Icon, ImVec2 IconOffset = ImVec2(0.f, 0.f))
	{
		return SubMenu(TickContext, Label, MenuFunc, Icon, Icon, IconOffset);
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////////

#define ImDrawCallback_ResetRenderState		 (ImDrawCallback)(-1)
#define ImDrawCallback_SetShaderState		 (ImDrawCallback)(-2)
#define ImDrawCallback_SetSamplerStatePoint	 (ImDrawCallback)(-3)
#define ImDrawCallback_ResetSamplerState	 (ImDrawCallback)(-4)
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

	// tick delegate handles ImGui window creation
	bool bSkipWindowCreation = false;

	// allow widget to customize menu bar item (only valid when adding widget to the main menu)
	bool bTickInMenuBar = false;

	// draw the widget from right side of the window
	bool bRightAligned = false;

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
