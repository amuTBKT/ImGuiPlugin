// Copyright 2024-26 Amit Kumar Mehar. All Rights Reserved.

#pragma once

#include "Misc/Optional.h"
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

	// main menu bar
	bool  MainMenuBar_bIsTicking = false;
	bool  MainMenuBar_bTickingRightAlignedItems = false;
	float MainMenuBar_Height = 0.f;
	float MainMenuBar_RightDirOffsetX = 0.f;
	float MainMenuBar_RightDirOffsetY = 0.f;
	float MainMenuBar_RightDirCursorPosX = 0.f;
	// allow callback to modify enabled state (alternative to UImGuiSubsystem::GetMainMenuWidgetActiveState(...))
	bool* MainMenuBar_CurrentItemEnabledState = nullptr;

	// Unreal theme colors (mirrors EStyleColor)
	const TArray<FLinearColor>* SlateColorsVec4 = nullptr;
	const TArray<uint32>*		SlateColorsU32 = nullptr;

	// this is different from `AllocateSpaceForRightAlignedMenuWidget` as ImGui::BeginMenu has some custom logic to handle item spacing
	bool AllocateSpaceForRightAlignedMenuItem(const char* Label)
	{
		if (!ensure(MainMenuBar_bIsTicking && MainMenuBar_bTickingRightAlignedItems))
		{
			return false;
		}
		if (ImGui::GetCurrentWindow()->DC.LayoutType == ImGuiLayoutType_Vertical)
		{
			// no need to offset cursor
			return true;
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
		if (ImGui::GetCurrentWindow()->DC.LayoutType == ImGuiLayoutType_Vertical)
		{
			// no need to offset cursor
			return true;
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

// scoped style edits
struct FImGuiScopedStyleEdit : FNoncopyable
{
	template <typename... Args>
	FImGuiScopedStyleEdit(Args&&... args)
	{
		static_assert(sizeof...(args) % 2 == 0);
		ProcessStyleVarsImpl(std::forward_as_tuple(std::forward<Args>(args)...), std::make_index_sequence<sizeof...(args) / 2>{});
	}

	~FImGuiScopedStyleEdit()
	{
		ImGui::PopStyleVar(StyleVarCount);
		ImGui::PopStyleColor(StyleColorCount);
	}

private:
	template <typename Tuple, size_t... Is>
	void ProcessStyleVarsImpl(Tuple&& t, std::index_sequence<Is...>)
	{
		(PushVarImpl(std::get<Is * 2>(t), std::get<Is * 2 + 1>(t)), ...);
	}

	// style vars
	void PushVarImpl(ImGuiStyleVar_ idx, float var)
	{
		StyleVarCount++;
		ImGui::PushStyleVar(idx, var);
	}
	void PushVarImpl(ImGuiStyleVar_ idx, ImVec2&& var)
	{
		StyleVarCount++;
		ImGui::PushStyleVar(idx, MoveTemp(var));
	}
	void PushVarImpl(ImGuiStyleVar_ idx, const ImVec2& var)
	{
		StyleVarCount++;
		ImGui::PushStyleVar(idx, var);
	}

	// style colors
	void PushVarImpl(ImGuiCol_ idx, uint32 col)
	{
		StyleColorCount++;
		ImGui::PushStyleColor(idx, col);
	}
	void PushVarImpl(ImGuiCol_ idx, const ImColor& col)
	{
		StyleColorCount++;
		ImGui::PushStyleColor(idx, (ImVec4)col);
	}
	void PushVarImpl(ImGuiCol_ idx, ImVec4&& col)
	{
		StyleColorCount++;
		ImGui::PushStyleColor(idx, MoveTemp(col));
	}
	void PushVarImpl(ImGuiCol_ idx, const ImVec4& col)
	{
		StyleColorCount++;
		ImGui::PushStyleColor(idx, col);
	}
	void PushVarImpl(ImGuiCol_ idx, const FColor& col)
	{
		PushVarImpl(idx, col.DWColor());
	}
	void PushVarImpl(ImGuiCol_ idx, const FLinearColor& col)
	{
		PushVarImpl(idx, col.ToFColorSRGB());
	}

	int32 StyleColorCount = 0;
	int32 StyleVarCount = 0;
};

// params used to create image widgets, works for slate icons too (they are atlased)
// NOTE: using ImVec2(0,0) for invalid image (maps to 2x2 white pixel), can switch to ImGui::GetFontTexUvWhitePixel() if this setup breaks at some point
struct FImGuiImageBindingParams
{
	ImVec2 Size = ImVec2(0.f, 0.f);
	ImVec2 UV0  = ImVec2(0.f, 0.f);
	ImVec2 UV1  = ImVec2(0.f, 0.f);
	ImTextureRef Id;

	ImTextureID GetTexID()	const { return Id.GetTexID(); }
	bool		IsValid()	const { return GetTexID() != ImTextureID_Invalid; }
};

///////////////////////////////////////////////////////////////////////////////////////////////////////

struct FButtonStyle;
struct FSpinBoxStyle;
struct FCheckBoxStyle;
struct FSearchBoxStyle;
struct FComboButtonStyle;

namespace FImGui
{
	FORCEINLINE FImGuiTickContext* GetTickContext(ImGuiContext* Context)
	{
		return FImGuiTickContext::GetTickContextFromImGuiContext(Context);
	}
	FORCEINLINE FImGuiTickContext* GetCurrentTickContext()
	{
		return FImGuiTickContext::GetTickContextFromImGuiContext(ImGui::GetCurrentContext());
	}

	template <typename StyleColor>
	FORCEINLINE ImVec4 GetSlateColorVec4(const FImGuiTickContext* TickContext, StyleColor SlateColorId)
	{
		return BitCast<ImVec4>((*TickContext->SlateColorsVec4)[static_cast<int32>(SlateColorId)]);
	}
	template <typename StyleColor>
	FORCEINLINE ImU32  GetSlateColorU32(const FImGuiTickContext* TickContext, StyleColor SlateColorId)
	{
		return (*TickContext->SlateColorsU32)[static_cast<int32>(SlateColorId)];
	}

	// utility function to allow adding icon to menu item
	FORCEINLINE bool MenuItem(FImGuiTickContext* TickContext, const char* Label, bool bIsActive, const FImGuiImageBindingParams& Icon, ImVec2 IconOffset = ImVec2(0.f, 0.f))
	{
		const float ItemSpacing = ImGui::GetStyle().ItemSpacing.x;
		const float HalfItemSpacing = ItemSpacing * 0.5f;
		const float FramePadding = ImGui::GetStyle().FramePadding.x;
		const bool bIsHorizontalMenu = ImGui::GetCurrentWindow()->DC.LayoutType == ImGuiLayoutType_Horizontal;

		const bool bIsIconOnlyItem = FCStringAnsi::Strstr(Label, "###") == Label;
		ensureMsgf(bIsHorizontalMenu || !bIsIconOnlyItem, TEXT("Icon only menu item (%hs) should only be used with horizontal menus."), Label);

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
		checkf(LabelBuffer.GetAllocatedSize() == 0, TEXT("LabelBuffer overflow detected! Try to keep menu item labels short."));

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

		const bool bIsIconOnlyItem = FCStringAnsi::Strstr(Label, "###") == Label;
		ensureMsgf(bIsHorizontalMenu || !bIsIconOnlyItem, TEXT("Icon only menu item (%hs) should only be used with horizontal menus."), Label);

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
		if (FCStringAnsi::Strstr(Label, "###"))
		{
			LabelBuffer.Append(Label);
		}
		else
		{
			LabelBuffer.Appendf("%s###%s", Label, Label);
		}
		checkf(LabelBuffer.GetAllocatedSize() == 0, TEXT("LabelBuffer overflow detected! Try to keep menu item labels short."));

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

	FORCEINLINE bool ImageButton(const char* str_id, const FImGuiImageBindingParams& image, const ImVec4& bg_col = ImVec4(0, 0, 0, 0), const ImVec4& tint_col = ImVec4(1, 1, 1, 1))
	{
		return ImGui::ImageButton(str_id, image.Id, image.Size, image.UV0, image.UV1, bg_col, tint_col);
	}
	FORCEINLINE void Image(const FImGuiImageBindingParams& image, const ImVec4& tint_col = ImVec4(1, 1, 1, 1))
	{
		ImGui::ImageWithBg(image.Id, image.Size, image.UV0, image.UV1, /*bg_col=*/ImVec4(0, 0, 0, 0), tint_col);
	}

	IMGUIRUNTIME_API void DrawSlateBrush(const FImGuiTickContext* TickContext, const FSlateBrush& Brush, ImVec2 RectMin, ImVec2 RectMax, TOptional<ImVec4> Tint = {}, TOptional<ImU32> BorderColor = {});

	IMGUIRUNTIME_API bool Button(FImGuiTickContext* TickContext, const char* Label, const FButtonStyle& ButtonStyle, const ImVec4& ContentPadding);
	FORCEINLINE bool Button(FImGuiTickContext* TickContext, const char* Label, const FButtonStyle& ButtonStyle)
	{
		ImVec2 FramePadding = ImGui::GetStyle().FramePadding;
		return Button(TickContext, Label, ButtonStyle, ImVec4(FramePadding.x, FramePadding.y, FramePadding.x, FramePadding.y));
	}

	IMGUIRUNTIME_API bool Button(FImGuiTickContext* TickContext, const char* Id, const FImGuiImageBindingParams& Image, const FButtonStyle& ButtonStyle, const ImVec4& ContentPadding);
	FORCEINLINE bool Button(FImGuiTickContext* TickContext, const char* Id, const FImGuiImageBindingParams& Image, const FButtonStyle& ButtonStyle)
	{
		ImVec2 FramePadding = ImGui::GetStyle().FramePadding;
		return Button(TickContext, Id, Image, ButtonStyle, ImVec4(FramePadding.x, FramePadding.y, FramePadding.x, FramePadding.y));
	}

	IMGUIRUNTIME_API bool CheckBox(FImGuiTickContext* TickContext, const char* Label, bool* Value, const FCheckBoxStyle& CheckBoxStyle);
	IMGUIRUNTIME_API bool ToggleButton(FImGuiTickContext* TickContext, const char* Label, bool* Value, const FImGuiImageBindingParams& Image, const FCheckBoxStyle& CheckBoxStyle);
	FORCEINLINE bool RadioButton(FImGuiTickContext* TickContext, const char* Label, bool* Value, const FCheckBoxStyle& CheckBoxStyle)
	{
		bool bTempValue = *Value;
		bool bPressed = CheckBox(TickContext, Label, &bTempValue, CheckBoxStyle);
		if (!(*Value) && bPressed)
		{
			*Value = bTempValue;
			return true;
		}
		return false;
	}

	IMGUIRUNTIME_API bool ComboButton(FImGuiTickContext* TickContext, const char* Label, const char* PreviewText, const FComboButtonStyle& ComboButtonStyle);
	IMGUIRUNTIME_API bool ComboButton(FImGuiTickContext* TickContext, const char* Label, const FImGuiImageBindingParams& Image, const FComboButtonStyle& ComboButtonStyle);

	IMGUIRUNTIME_API void TextBlock(FImGuiTickContext* TickContext, const char* Text);
	IMGUIRUNTIME_API bool EditableTextBox(FImGuiTickContext* TickContext, const char* Label, const char* Hint, char* Buffer, size_t BufferSize, ImGuiInputTextFlags Flags = 0, ImGuiInputTextCallback Callback = nullptr, void* UserData = nullptr);
	IMGUIRUNTIME_API bool SearchTextBox(FImGuiTickContext* TickContext, const char* Label, const char* Hint, char* Buffer, size_t BufferSize, const FSearchBoxStyle& SearchBoxStyle, bool bSetFocus = false, bool bActivateOnNavFocus = false, ImGuiInputTextFlags Flags = 0, ImGuiInputTextCallback Callback = nullptr, void* UserData = nullptr);

	IMGUIRUNTIME_API bool SpinBox(FImGuiTickContext* TickContext, const char* Label, float& v, const FSpinBoxStyle& SpinBoxStyle, float v_speed = 1.f, float v_min = 0.f, float v_max = 0.f, const char* format = "%.3f", ImGuiSliderFlags flags = ImGuiSliderFlags_ClampOnInput);
	IMGUIRUNTIME_API bool SpinBox(FImGuiTickContext* TickContext, const char* Label, double& v, const FSpinBoxStyle& SpinBoxStyle, double v_speed = 1., double v_min = 0., double v_max = 0., const char* format = "%.3f", ImGuiSliderFlags flags = ImGuiSliderFlags_ClampOnInput);
	IMGUIRUNTIME_API bool SpinBox(FImGuiTickContext* TickContext, const char* Label, int32& v, const FSpinBoxStyle& SpinBoxStyle, int32 v_speed = 1, int32 v_min = 0, int32 v_max = 0, const char* format = "%d", ImGuiSliderFlags flags = ImGuiSliderFlags_ClampOnInput);
	IMGUIRUNTIME_API bool SpinBox(FImGuiTickContext* TickContext, const char* Label, uint32& v, const FSpinBoxStyle& SpinBoxStyle, uint32 v_speed = 1, uint32 v_min = 0, uint32 v_max = 0, const char* format = "%d", ImGuiSliderFlags flags = ImGuiSliderFlags_ClampOnInput);

	FORCEINLINE bool BeginCombo(FImGuiTickContext* TickContext, const char* Label, const char* PreviewText, const FComboButtonStyle& ComboButtonStyle)
	{
		const ImGuiID PopupId = ImHashStr("##ComboPopup", 0, ImGui::GetID(Label));
		bool bIsPopupOpen = ImGui::IsPopupOpen(PopupId, ImGuiPopupFlags_None);
		const bool bPressed = ComboButton(TickContext, Label, PreviewText, ComboButtonStyle);
		if (bPressed && !bIsPopupOpen)
		{
			ImGui::OpenPopupEx(PopupId, ImGuiPopupFlags_None);
		}

		return ImGui::BeginComboPopup(PopupId, ImRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax()), ImGuiComboFlags_None);
	}
	FORCEINLINE void EndCombo() { ImGui::EndCombo(); }

	IMGUIRUNTIME_API void PushComboListViewDropdownStyle(FImGuiTickContext* TickContext, bool bDarkTheme);
	IMGUIRUNTIME_API void PopComboListViewDropdownStyle(FImGuiTickContext* TickContext);
	IMGUIRUNTIME_API void BeginComboListViewDropdownBody(FImGuiTickContext* TickContext);
	IMGUIRUNTIME_API void EndComboListViewDropdownBody(FImGuiTickContext* TickContext);

	FORCEINLINE void SetTooltip(const char* fmt, ...)
	{
		FImGuiScopedStyleEdit TooltipStyle
		{
			ImGuiStyleVar_WindowPadding,	ImVec2(8.f, 8.f),
			ImGuiCol_PopupBg,				0xF0FFFFFF,
			ImGuiCol_Text,					0xFF000000,
		};

		va_list args;
		va_start(args, fmt);
		ImGui::SetTooltipV(fmt, args);
		va_end(args);
	}
	FORCEINLINE void SetItemTooltip(const char* fmt, ...)
	{
		FImGuiScopedStyleEdit TooltipStyle
		{
			ImGuiStyleVar_WindowPadding,	ImVec2(8.f, 8.f),
			ImGuiCol_PopupBg,				0xF0FFFFFF,
			ImGuiCol_Text,					0xFF000000,
		};

		va_list args;
		va_start(args, fmt);
		ImGui::SetItemTooltipV(fmt, args);
		va_end(args);
	}

	// variants without FImGuiTickContext parameter
	template <typename StyleColor>
	FORCEINLINE ImVec4 GetSlateColorVec4(StyleColor SlateColorId)
	{
		return GetSlateColorVec4(FImGui::GetCurrentTickContext(), static_cast<int32>(SlateColorId));
	}
	template <typename StyleColor>
	FORCEINLINE ImU32 GetSlateColorU32(StyleColor SlateColorId)
	{
		return GetSlateColorU32(FImGui::GetCurrentTickContext(), static_cast<int32>(SlateColorId));
	}
	FORCEINLINE bool Button(const char* Label, const FButtonStyle& ButtonStyle, const ImVec4& ContentPadding)
	{
		return Button(FImGui::GetCurrentTickContext(), Label, ButtonStyle, ContentPadding);
	}
	FORCEINLINE bool Button(const char* Label, const FButtonStyle& ButtonStyle)
	{
		return Button(FImGui::GetCurrentTickContext(), Label, ButtonStyle);
	}
	FORCEINLINE bool Button(const char* Id, const FImGuiImageBindingParams& Image, const FButtonStyle& ButtonStyle, const ImVec4& ContentPadding)
	{
		return Button(FImGui::GetCurrentTickContext(), Id, Image, ButtonStyle, ContentPadding);
	}
	FORCEINLINE bool Button(const char* Id, const FImGuiImageBindingParams& Image, const FButtonStyle& ButtonStyle)
	{
		return Button(FImGui::GetCurrentTickContext(), Id, Image, ButtonStyle);
	}
	FORCEINLINE bool CheckBox(const char* Label, bool* Value, const FCheckBoxStyle& CheckBoxStyle)
	{
		return CheckBox(FImGui::GetCurrentTickContext(), Label, Value, CheckBoxStyle);
	}
	FORCEINLINE bool ToggleButton(const char* Label, bool* Value, const FImGuiImageBindingParams& Image, const FCheckBoxStyle& CheckBoxStyle)
	{
		return ToggleButton(FImGui::GetCurrentTickContext(), Label, Value, Image, CheckBoxStyle);
	}
	FORCEINLINE bool RadioButton(const char* Label, bool* Value, const FCheckBoxStyle& CheckBoxStyle)
	{
		return RadioButton(FImGui::GetCurrentTickContext(), Label, Value, CheckBoxStyle);
	}
	FORCEINLINE bool ComboButton(const char* Label, const char* PreviewText, const FComboButtonStyle& ComboButtonStyle)
	{
		return ComboButton(FImGui::GetCurrentTickContext(), Label, PreviewText, ComboButtonStyle);
	}
	FORCEINLINE bool ComboButton(const char* Label, const FImGuiImageBindingParams& Image, const FComboButtonStyle& ComboButtonStyle)
	{
		return ComboButton(FImGui::GetCurrentTickContext(), Label, Image, ComboButtonStyle);
	}
	FORCEINLINE void TextBlock(const char* Text)
	{
		TextBlock(FImGui::GetCurrentTickContext(), Text);
	}
	FORCEINLINE bool EditableTextBox(const char* Label, const char* Hint, char* Buffer, size_t BufferSize, ImGuiInputTextFlags Flags = 0, ImGuiInputTextCallback Callback = nullptr, void* UserData = nullptr)
	{
		return EditableTextBox(FImGui::GetCurrentTickContext(), Label, Hint, Buffer, BufferSize, Flags, Callback, UserData);
	}
	FORCEINLINE bool SearchTextBox(const char* Label, const char* Hint, char* Buffer, size_t BufferSize, const FSearchBoxStyle& SearchBoxStyle, bool bSetFocus = false, bool bActivateOnNavFocus = false, ImGuiInputTextFlags Flags = 0, ImGuiInputTextCallback Callback = nullptr, void* UserData = nullptr)
	{
		return SearchTextBox(FImGui::GetCurrentTickContext(), Label, Hint, Buffer, BufferSize, SearchBoxStyle, bSetFocus, bActivateOnNavFocus, Flags, Callback, UserData);
	}
	FORCEINLINE bool BeginCombo(const char* Label, const char* PreviewText, const FComboButtonStyle& ComboButtonStyle)
	{
		return BeginCombo(FImGui::GetCurrentTickContext(), Label, PreviewText, ComboButtonStyle);
	}
	FORCEINLINE void PushComboListViewDropdownStyle(bool bDarkTheme)
	{
		PushComboListViewDropdownStyle(FImGui::GetCurrentTickContext(), bDarkTheme);
	}
	FORCEINLINE void PopComboListViewDropdownStyle()
	{
		PopComboListViewDropdownStyle(FImGui::GetCurrentTickContext());
	}
	FORCEINLINE void BeginComboListViewDropdownBody()
	{
		BeginComboListViewDropdownBody(FImGui::GetCurrentTickContext());
	}
	FORCEINLINE void EndComboListViewDropdownBody()
	{
		EndComboListViewDropdownBody(FImGui::GetCurrentTickContext());
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
	DisableTextureAlpha  = 1u << 1,  // disable texture alpha
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
	// Widget init function (optional)
	void(*InitFunction)(void);

	/*
	*  Widget/Window closed callback (optional)
	*  For standalone widgets : When slate widget is destroyed
	*  For menu widgets		  : When ImGui window is closed
	*/
	void(*OnWindowClosed)(void);

	// ImGui widget tick function
	void(*TickFunction)(FImGuiTickContext* Context);

	// optional icon to use for the widget menu item
	FSlateIcon WidgetIcon;

	// full path to widget (example: "Tools.MyWidget")
	const char* WidgetPath = nullptr;

	// widget tooltip
	const char* WidgetDescription = nullptr;

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
		return TickFunction && WidgetPath;
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
