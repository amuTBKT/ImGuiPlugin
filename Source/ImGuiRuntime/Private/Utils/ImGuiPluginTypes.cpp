// Copyright 2024-26 Amit Kumar Mehar. All Rights Reserved.

#include "ImGuiSubsystem.h"
#include "Styling/SlateTypes.h"
#include "Styling/StyleColors.h"

namespace FImGui
{
	#define DEFINE_PRIVATE_ACCESS(Class, Property)												\
	namespace PrivateAccess																		\
	{																							\
		template<typename>																		\
		struct TClass_ ## Property;																\
																								\
		template<>																				\
		struct TClass_ ## Property<Class>														\
		{																						\
			template<auto PropertyPtr>															\
			struct TProperty_ ## Property														\
			{																					\
				friend const auto& Property(const Class& Object)								\
				{																				\
					return Object.*PropertyPtr;													\
				}																				\
			};																					\
		};																						\
		template struct TClass_ ## Property<Class>::TProperty_ ## Property<&Class::Property>;	\
		const auto& Property(const Class& Object);												\
	}

	DEFINE_PRIVATE_ACCESS(FSlateColor, ColorUseRule);
	DEFINE_PRIVATE_ACCESS(FSlateColor, ColorTableId);
	DEFINE_PRIVATE_ACCESS(FSlateColor, SpecifiedColor);

	ImVec4 GetSlateColorVec4(const FSlateColor& Color, const FImGuiTickContext* TickContext)
	{
		switch (PrivateAccess::ColorUseRule(Color))
		{
		default:
		case ESlateColorStylingMode::UseColor_UseStyle:
		case ESlateColorStylingMode::UseColor_Foreground:
		case ESlateColorStylingMode::UseColor_Foreground_Subdued:
			return ImVec4(1.f, 1.f, 1.f, 1.f);
			break;

		case ESlateColorStylingMode::UseColor_Specified:
			return ImColor(PrivateAccess::SpecifiedColor(Color).ToFColorSRGB().DWColor());
			break;

		case ESlateColorStylingMode::UseColor_ColorTable:
			return BitCast<ImVec4>((*TickContext->SlateColorsVec4)[(uint32)PrivateAccess::ColorTableId(Color)]);
			break;
		}
	}
	ImU32 GetSlateColorU32(const FSlateColor& Color, const FImGuiTickContext* TickContext)
	{
		switch (PrivateAccess::ColorUseRule(Color))
		{
		default:
		case ESlateColorStylingMode::UseColor_UseStyle:
		case ESlateColorStylingMode::UseColor_Foreground:
		case ESlateColorStylingMode::UseColor_Foreground_Subdued:
			return 0xFFFFFFFF;
			break;

		case ESlateColorStylingMode::UseColor_Specified:
			return PrivateAccess::SpecifiedColor(Color).ToFColorSRGB().DWColor();
			break;

		case ESlateColorStylingMode::UseColor_ColorTable:
			return (*TickContext->SlateColorsU32)[(uint32)PrivateAccess::ColorTableId(Color)];
			break;
		}
	}

	// TODO: Based on FSlateElementBatcher::AddBoxElements, requires cleanup
	void DrawSlateBrush(
		const FImGuiTickContext* TickContext, const FSlateBrush& Brush, ImVec2 RectMin, ImVec2 RectMax,
		TOptional<ImVec4> Tint, TOptional<ImU32> BorderColor)
	{

		ImVec2 RectSize = RectMax - RectMin;
		if (ImMin(RectSize.x, RectSize.y) <= KINDA_SMALL_NUMBER)
		{
			return;
		}

		if (Brush.GetDrawType() == ESlateBrushDrawType::NoDrawType)
		{
			if (BorderColor.IsSet())
			{
				ImGui::GetWindowDrawList()->AddRect(RectMin, RectMax, BorderColor.GetValue(), ImGui::GetStyle().FrameRounding, 1.f);
			}
			return;
		}

		float Rounding = 0.f;
		if (Brush.GetDrawType() == ESlateBrushDrawType::RoundedBox)
		{
			if (Brush.OutlineSettings.RoundingType == ESlateBrushRoundingType::HalfHeightRadius)
			{
				Rounding = RectSize.y * 0.5f;
			}
			else
			{
				// TODO: non uniform rounding not handled atm
				//ensure(Brush.OutlineSettings.CornerRadii.X == Brush.OutlineSettings.CornerRadii.Y &&
				//	Brush.OutlineSettings.CornerRadii.X == Brush.OutlineSettings.CornerRadii.Z &&
				//	Brush.OutlineSettings.CornerRadii.X == Brush.OutlineSettings.CornerRadii.W);
				Rounding = Brush.OutlineSettings.CornerRadii.X;
			}
		}

		uint32 ImageColor;
		if (Tint.IsSet())
		{
			ImageColor = ImGui::GetColorU32(GetSlateColorVec4(Brush.TintColor, TickContext) * Tint.GetValue());
		}
		else
		{
			ImageColor = ImGui::GetColorU32(GetSlateColorU32(Brush.TintColor, TickContext));
		}

		ImDrawList* DrawList = ImGui::GetWindowDrawList();

		switch (Brush.GetDrawType())
		{
		default:
		case ESlateBrushDrawType::Box:
		case ESlateBrushDrawType::RoundedBox:
		{
			if (Brush.GetImageType() != ESlateBrushImageType::NoImage)
			{
				auto Image = UImGuiSubsystem::Get()->RegisterOneFrameResource(&Brush, FVector2f(RectSize.x, RectSize.y));

				FVector2f StartUV = FVector2f(Image.UV0.x, Image.UV0.y);
				FVector2f EndUV = FVector2f(Image.UV1.x, Image.UV1.y);
				FVector2f SizeUV = EndUV - StartUV;

				float TextureWidth = 1.f;
				float TextureHeight = 1.f;
				if (Brush.GetRenderingResource().GetResourceProxy())
				{
					TextureWidth = Brush.GetRenderingResource().GetResourceProxy()->ActualSize.X;
					TextureHeight = Brush.GetRenderingResource().GetResourceProxy()->ActualSize.Y;
				}
				FVector2f HalfTexel = FVector2f(0.f, 0.f) / FVector2f(TextureWidth, TextureHeight);; //TODO: not handled

				const ESlateBrushTileType::Type TilingRule = Brush.GetTiling();
				const bool bTileHorizontal = (TilingRule == ESlateBrushTileType::Both || TilingRule == ESlateBrushTileType::Horizontal);
				const bool bTileVertical = (TilingRule == ESlateBrushTileType::Both || TilingRule == ESlateBrushTileType::Vertical);

				const ESlateBrushMirrorType::Type MirroringRule = Brush.GetMirroring();
				const bool bMirrorHorizontal = (MirroringRule == ESlateBrushMirrorType::Both || MirroringRule == ESlateBrushMirrorType::Horizontal);
				const bool bMirrorVertical = (MirroringRule == ESlateBrushMirrorType::Both || MirroringRule == ESlateBrushMirrorType::Vertical);

				float HorizontalTiling = bTileHorizontal ? RectSize.x / TextureWidth : 1.0f;
				float VerticalTiling = bTileVertical ? RectSize.y / TextureHeight : 1.0f;
				const FVector2f Tiling(HorizontalTiling, VerticalTiling);

				const FVector2f TopLeft = FVector2f(RectMin.x, RectMin.y);
				const FVector2f BotRight = TopLeft + FVector2f(RectSize.x, RectSize.y);
				const FVector2f TopRight = FVector2f(BotRight.X, TopLeft.Y);
				const FVector2f BotLeft = FVector2f(TopLeft.X, BotRight.Y);

				const FMargin& Margin = Brush.GetMargin();
				if (Margin.Left != 0.0f || Margin.Top != 0.0f || Margin.Right != 0.0f || Margin.Bottom != 0.0f)
				{
					float LeftMarginU = (Margin.Left > 0.0f)
						? StartUV.X + Margin.Left * SizeUV.X + HalfTexel.X
						: StartUV.X;
					float TopMarginV = (Margin.Top > 0.0f)
						? StartUV.Y + Margin.Top * SizeUV.Y + HalfTexel.Y
						: StartUV.Y;
					float RightMarginU = (Margin.Right > 0.0f)
						? EndUV.X - Margin.Right * SizeUV.X + HalfTexel.X
						: EndUV.X;
					float BottomMarginV = (Margin.Bottom > 0.0f)
						? EndUV.Y - Margin.Bottom * SizeUV.Y + HalfTexel.Y
						: EndUV.Y;

					if (bMirrorHorizontal || bMirrorVertical)
					{
						const FVector2f UVMin = StartUV;
						const FVector2f UVMax = EndUV;

						if (bMirrorHorizontal)
						{
							StartUV.X = UVMax.X - (StartUV.X - UVMin.X);
							EndUV.X = UVMax.X - (EndUV.X - UVMin.X);
							LeftMarginU = UVMax.X - (LeftMarginU - UVMin.X);
							RightMarginU = UVMax.X - (RightMarginU - UVMin.X);
						}
						if (bMirrorVertical)
						{
							StartUV.Y = UVMax.Y - (StartUV.Y - UVMin.Y);
							EndUV.Y = UVMax.Y - (EndUV.Y - UVMin.Y);
							TopMarginV = UVMax.Y - (TopMarginV - UVMin.Y);
							BottomMarginV = UVMax.Y - (BottomMarginV - UVMin.Y);
						}
					}

					float LeftMarginX = TextureWidth * Margin.Left;
					float TopMarginY = TextureHeight * Margin.Top;
					float RightMarginX = RectSize.x - TextureWidth * Margin.Right;
					float BottomMarginY = RectSize.y - TextureHeight * Margin.Bottom;

					// If the margins are overlapping the margins are too big or the button is too small
					// so clamp margins to half of the box size
					if (RightMarginX < LeftMarginX)
					{
						LeftMarginX = RectSize.x / 2.f;
						RightMarginX = LeftMarginX;
					}

					if (BottomMarginY < TopMarginY)
					{
						TopMarginY = RectSize.y / 2.f;
						BottomMarginY = TopMarginY;
					}

					FVector2f Position = TopLeft;
					FVector2f EndPos = BotRight;

					DrawList->PushTexture(Image.Id);

					DrawList->PrimReserve(54, 16);
					auto IndexStart = DrawList->_VtxCurrentIdx;

					DrawList->PrimWriteIdx(IndexStart + 0);
					DrawList->PrimWriteIdx(IndexStart + 1);
					DrawList->PrimWriteIdx(IndexStart + 2);
					DrawList->PrimWriteIdx(IndexStart + 2);
					DrawList->PrimWriteIdx(IndexStart + 1);
					DrawList->PrimWriteIdx(IndexStart + 3);

					DrawList->PrimWriteIdx(IndexStart + 2);
					DrawList->PrimWriteIdx(IndexStart + 3);
					DrawList->PrimWriteIdx(IndexStart + 4);
					DrawList->PrimWriteIdx(IndexStart + 4);
					DrawList->PrimWriteIdx(IndexStart + 3);
					DrawList->PrimWriteIdx(IndexStart + 5);

					DrawList->PrimWriteIdx(IndexStart + 4);
					DrawList->PrimWriteIdx(IndexStart + 5);
					DrawList->PrimWriteIdx(IndexStart + 6);
					DrawList->PrimWriteIdx(IndexStart + 6);
					DrawList->PrimWriteIdx(IndexStart + 5);
					DrawList->PrimWriteIdx(IndexStart + 7);

					// Middle
					DrawList->PrimWriteIdx(IndexStart + 1);
					DrawList->PrimWriteIdx(IndexStart + 8);
					DrawList->PrimWriteIdx(IndexStart + 3);
					DrawList->PrimWriteIdx(IndexStart + 3);
					DrawList->PrimWriteIdx(IndexStart + 8);
					DrawList->PrimWriteIdx(IndexStart + 9);

					DrawList->PrimWriteIdx(IndexStart + 3);
					DrawList->PrimWriteIdx(IndexStart + 9);
					DrawList->PrimWriteIdx(IndexStart + 5);
					DrawList->PrimWriteIdx(IndexStart + 5);
					DrawList->PrimWriteIdx(IndexStart + 9);
					DrawList->PrimWriteIdx(IndexStart + 10);

					DrawList->PrimWriteIdx(IndexStart + 5);
					DrawList->PrimWriteIdx(IndexStart + 10);
					DrawList->PrimWriteIdx(IndexStart + 7);
					DrawList->PrimWriteIdx(IndexStart + 7);
					DrawList->PrimWriteIdx(IndexStart + 10);
					DrawList->PrimWriteIdx(IndexStart + 11);

					// Bottom
					DrawList->PrimWriteIdx(IndexStart + 8);
					DrawList->PrimWriteIdx(IndexStart + 12);
					DrawList->PrimWriteIdx(IndexStart + 9);
					DrawList->PrimWriteIdx(IndexStart + 9);
					DrawList->PrimWriteIdx(IndexStart + 12);
					DrawList->PrimWriteIdx(IndexStart + 13);

					DrawList->PrimWriteIdx(IndexStart + 9);
					DrawList->PrimWriteIdx(IndexStart + 13);
					DrawList->PrimWriteIdx(IndexStart + 10);
					DrawList->PrimWriteIdx(IndexStart + 10);
					DrawList->PrimWriteIdx(IndexStart + 13);
					DrawList->PrimWriteIdx(IndexStart + 14);

					DrawList->PrimWriteIdx(IndexStart + 10);
					DrawList->PrimWriteIdx(IndexStart + 14);
					DrawList->PrimWriteIdx(IndexStart + 11);
					DrawList->PrimWriteIdx(IndexStart + 11);
					DrawList->PrimWriteIdx(IndexStart + 14);
					DrawList->PrimWriteIdx(IndexStart + 15);

					LeftMarginX += Position.X;
					RightMarginX += Position.X;
					TopMarginY += Position.Y;
					BottomMarginY += Position.Y;

					DrawList->PrimWriteVtx(ImVec2(Position.X, Position.Y), ImVec2(StartUV.X, StartUV.Y), ImageColor);
					DrawList->PrimWriteVtx(ImVec2(Position.X, TopMarginY), ImVec2(StartUV.X, TopMarginV), ImageColor);
					DrawList->PrimWriteVtx(ImVec2(LeftMarginX, Position.Y), ImVec2(LeftMarginU, StartUV.Y), ImageColor);
					DrawList->PrimWriteVtx(ImVec2(LeftMarginX, TopMarginY), ImVec2(LeftMarginU, TopMarginV), ImageColor);
					DrawList->PrimWriteVtx(ImVec2(RightMarginX, Position.Y), ImVec2(RightMarginU, StartUV.Y), ImageColor);
					DrawList->PrimWriteVtx(ImVec2(RightMarginX, TopMarginY), ImVec2(RightMarginU, TopMarginV), ImageColor);
					DrawList->PrimWriteVtx(ImVec2(EndPos.X, Position.Y), ImVec2(EndUV.X, StartUV.Y), ImageColor);
					DrawList->PrimWriteVtx(ImVec2(EndPos.X, TopMarginY), ImVec2(EndUV.X, TopMarginV), ImageColor);

					DrawList->PrimWriteVtx(ImVec2(Position.X, BottomMarginY), ImVec2(StartUV.X, BottomMarginV), ImageColor);
					DrawList->PrimWriteVtx(ImVec2(LeftMarginX, BottomMarginY), ImVec2(LeftMarginU, BottomMarginV), ImageColor);
					DrawList->PrimWriteVtx(ImVec2(RightMarginX, BottomMarginY), ImVec2(RightMarginU, BottomMarginV), ImageColor);
					DrawList->PrimWriteVtx(ImVec2(EndPos.X, BottomMarginY), ImVec2(EndUV.X, BottomMarginV), ImageColor);
					DrawList->PrimWriteVtx(ImVec2(Position.X, EndPos.Y), ImVec2(StartUV.X, EndUV.Y), ImageColor);
					DrawList->PrimWriteVtx(ImVec2(LeftMarginX, EndPos.Y), ImVec2(LeftMarginU, EndUV.Y), ImageColor);
					DrawList->PrimWriteVtx(ImVec2(RightMarginX, EndPos.Y), ImVec2(RightMarginU, EndUV.Y), ImageColor);
					DrawList->PrimWriteVtx(ImVec2(EndPos.X, EndPos.Y), ImVec2(EndUV.X, EndUV.Y), ImageColor);

					DrawList->PopTexture();
				}
				else
				{
					if (bMirrorHorizontal || bMirrorVertical)
					{
						const FVector2f UVMin = StartUV;
						const FVector2f UVMax = EndUV;

						if (bMirrorHorizontal)
						{
							StartUV.X = UVMax.X - (StartUV.X - UVMin.X);
							EndUV.X = UVMax.X - (EndUV.X - UVMin.X);
						}
						if (bMirrorVertical)
						{
							StartUV.Y = UVMax.Y - (StartUV.Y - UVMin.Y);
							EndUV.Y = UVMax.Y - (EndUV.Y - UVMin.Y);
						}
					}

					DrawList->AddImage(Image.Id, RectMin, RectMax, ImVec2(StartUV.X, StartUV.Y), ImVec2(EndUV.X, EndUV.Y), ImageColor);
				}
			}
			else
			{
				DrawList->AddRectFilled(RectMin, RectMax, ImageColor, Rounding);
			}
		}break;
		case ESlateBrushDrawType::Image:
		{
			if (/*ensure*/(Brush.GetImageType() != ESlateBrushImageType::NoImage))
			{
				auto Image = UImGuiSubsystem::Get()->RegisterOneFrameResource(&Brush, FVector2f(RectSize.x, RectSize.y));
				DrawList->AddImage(Image.Id, RectMin, RectMax, Image.UV0, Image.UV1, ImageColor);
			}
			else
			{
				DrawList->AddRectFilled(RectMin, RectMax, ImageColor, Rounding);
			}
		}break;
		case ESlateBrushDrawType::Border:
		{
			check(Brush.ImageType == ESlateBrushImageType::FullColor);
			DrawList->AddRect(RectMin, RectMax, ImageColor, Rounding);
		}break;
		}

		if (Brush.OutlineSettings.Width > 0.f)
		{
			ImU32 Color = BorderColor.IsSet() ? BorderColor.GetValue() : GetSlateColorU32(Brush.OutlineSettings.Color, TickContext);
			DrawList->AddRect(RectMin, RectMax, Color, Rounding, Brush.OutlineSettings.Width);
		}
		else if (BorderColor.IsSet())
		{
			DrawList->AddRect(RectMin, RectMax, BorderColor.GetValue(), ImGui::GetStyle().FrameRounding, 1.f);
		}
	}

	bool IsItemHighlighted(ImGuiID ItemId)
	{
		ImGuiContext& g = *ImGui::GetCurrentContext();
		return g.NavCursorVisible && g.NavId == ItemId && ((g.LastItemData.ItemFlags & ImGuiItemFlags_NoNav) == 0);
	}
	bool IsItemHighlighted() { return IsItemHighlighted(ImGui::GetItemID()); }

	void ApplyItemWidthOverride(float& ItemWidth)
	{
		ImGuiContext& g = *ImGui::GetCurrentContext();
		if (g.NextItemData.HasFlags & ImGuiNextItemDataFlags_HasWidth)
		{
			g.NextItemData.HasFlags &= ~ImGuiNextItemDataFlags_HasWidth;
			ItemWidth = g.NextItemData.Width;
		}
	}

	// same as ImGui::InvisibleButton but returns whether the button was culled + no nav highlight
	bool InvisibleButton(const char* str_id, bool& pressed, bool& hovered, const ImVec2& size_arg, ImGuiButtonFlags flags = ImGuiButtonFlags_NoHoveredOnFocus|ImGuiButtonFlags_EnableNav, const ImVec2& offset = ImVec2(0.f, 0.f))
	{
		ImGuiContext& g = *GImGui;
		ImGuiWindow* window = ImGui::GetCurrentWindow();
		if (window->SkipItems)
			return false;

		// Ensure zero-size fits to contents
		ImVec2 size = ImGui::CalcItemSize(ImVec2(size_arg.x != 0.0f ? size_arg.x : -FLT_MIN, size_arg.y != 0.0f ? size_arg.y : -FLT_MIN), 0.0f, 0.0f);

		const ImGuiID id = window->GetID(str_id);
		const ImRect bb(window->DC.CursorPos + offset, window->DC.CursorPos + offset + size);
		ImGui::ItemSize(size);
		if (!ImGui::ItemAdd(bb, id, NULL, (flags & ImGuiButtonFlags_EnableNav) ? ImGuiItemFlags_None : ImGuiItemFlags_NoNav))
			return false;

		bool held;
		pressed = ImGui::ButtonBehavior(bb, id, &hovered, &held, flags);

		return true;
	}

	bool Button(FImGuiTickContext* TickContext, const char* Label, const FButtonStyle& ButtonStyle, const ImVec4& ContentPadding)
	{
		FImGuiTickScope Scope{ TickContext };

		const char* LabelEnd = ImGui::FindRenderedTextEnd(Label);
		const ImVec2 LabelSize = ImGui::CalcTextSize(Label, LabelEnd);
		const ImVec2& ContentSize = LabelSize;
		ImVec2 ItemRect = ContentSize + ImVec2(
			(ContentPadding.x + ContentPadding.z) + FMath::Max(ButtonStyle.NormalPadding.Left + ButtonStyle.NormalPadding.Right, ButtonStyle.PressedPadding.Left + ButtonStyle.PressedPadding.Right),
			(ContentPadding.y + ContentPadding.w) + FMath::Max(ButtonStyle.NormalPadding.Top + ButtonStyle.NormalPadding.Bottom, ButtonStyle.PressedPadding.Top + ButtonStyle.PressedPadding.Bottom));
		ApplyItemWidthOverride(ItemRect.x);

		bool bPressed, bHovered;
		if (!InvisibleButton(Label, bPressed, bHovered, ItemRect))
		{
			return false;
		}
		bool bActive = bPressed || ImGui::IsItemActive();

		const FSlateBrush* Brush = bActive ? &ButtonStyle.Pressed : (bHovered ? &ButtonStyle.Hovered : &ButtonStyle.Normal);
		DrawSlateBrush(TickContext, *Brush, ImGui::GetItemRectMin(), ImGui::GetItemRectMax(), {}, IsItemHighlighted() ? ImGui::GetColorU32(ImGuiCol_NavCursor) : TOptional<ImU32>{});

		ImRect LabelRect = { ImGui::GetItemRectMin() + ImVec2(ContentPadding.x, ContentPadding.y), ImGui::GetItemRectMax() - ImVec2(ContentPadding.z, ContentPadding.w) };
		LabelRect.Min += bActive ? ImVec2(ButtonStyle.PressedPadding.Left, ButtonStyle.PressedPadding.Top) : ImVec2(ButtonStyle.NormalPadding.Left, ButtonStyle.NormalPadding.Top);
		LabelRect.Max -= bActive ? ImVec2(ButtonStyle.PressedPadding.Right, ButtonStyle.PressedPadding.Bottom) : ImVec2(ButtonStyle.NormalPadding.Right, ButtonStyle.NormalPadding.Bottom);
		//ImGui::GetWindowDrawList()->AddRect(LabelRect.Min, LabelRect.Max, 0xff00ff00);

		const FSlateColor& ForegroundColor = bActive ? ButtonStyle.PressedForeground : (bHovered ? ButtonStyle.HoveredForeground : ButtonStyle.NormalForeground);
		ImGui::PushStyleColor(ImGuiCol_Text, GetSlateColorVec4(ForegroundColor, TickContext));
		ImGui::RenderTextClipped(LabelRect.Min, LabelRect.Max, Label, LabelEnd, &LabelSize, ImGui::GetStyle().ButtonTextAlign, &LabelRect);
		ImGui::PopStyleColor();

		return bPressed;
	}

	bool Button(FImGuiTickContext* TickContext, const char* Id, const FImGuiImageBindingParams& Image, const FButtonStyle& ButtonStyle, const ImVec4& ContentPadding)
	{
		FImGuiTickScope Scope{ TickContext };

		const ImVec2& ContentSize = Image.Size;
		ImVec2 ItemRect = ContentSize + ImVec2(
			(ContentPadding.x + ContentPadding.z) + FMath::Max(ButtonStyle.NormalPadding.Left + ButtonStyle.NormalPadding.Right, ButtonStyle.PressedPadding.Left + ButtonStyle.PressedPadding.Right),
			(ContentPadding.y + ContentPadding.w) + FMath::Max(ButtonStyle.NormalPadding.Top + ButtonStyle.NormalPadding.Bottom, ButtonStyle.PressedPadding.Top + ButtonStyle.PressedPadding.Bottom));
		ApplyItemWidthOverride(ItemRect.x);

		bool bPressed, bHovered;
		if (!InvisibleButton(Id, bPressed, bHovered, ItemRect))
		{
			return false;
		}
		bool bActive = bPressed || ImGui::IsItemActive();

		const FSlateBrush* Brush = bActive ? &ButtonStyle.Pressed : (bHovered ? &ButtonStyle.Hovered : &ButtonStyle.Normal);
		DrawSlateBrush(TickContext, *Brush, ImGui::GetItemRectMin(), ImGui::GetItemRectMax(), {}, IsItemHighlighted() ? ImGui::GetColorU32(ImGuiCol_NavCursor) : TOptional<ImU32>{});

		ImVec2 CursorPos = (ImGui::GetItemRectMax() + ImGui::GetItemRectMin()) * 0.5f - Image.Size * 0.5f;
		if (bActive)
		{
			CursorPos.y += (ButtonStyle.PressedPadding.Top - ButtonStyle.NormalPadding.Top);
		}
		CursorPos.y = ImFloor(CursorPos.y);

		const FSlateColor& ForegroundColor = bActive ? ButtonStyle.PressedForeground : (bHovered ? ButtonStyle.HoveredForeground : ButtonStyle.NormalForeground);
		ImGui::GetWindowDrawList()->AddImage(Image.Id, CursorPos, CursorPos + Image.Size, Image.UV0, Image.UV1, ImGui::GetColorU32(GetSlateColorU32(ForegroundColor, TickContext)));

		return bPressed;
	}

	bool CheckBox(FImGuiTickContext* TickContext, const char* Label, bool* Value, const FCheckBoxStyle& CheckBoxStyle)
	{
		FImGuiTickScope Scope{ TickContext };

		const char* LabelEnd = ImGui::FindRenderedTextEnd(Label);
		const ImVec2 LabelSize = ImGui::CalcTextSize(Label, LabelEnd);
		const bool bHasContent = LabelSize.x > 0.f;

		ImVec2 ImageSize{};
		const bool bHasImage = (CheckBoxStyle.CheckBoxType != ESlateCheckBoxType::ToggleButton);
		if (bHasImage)
		{
			const auto& CheckedBrush = *Value ? CheckBoxStyle.CheckedImage : CheckBoxStyle.UncheckedImage;
			ImageSize = ImVec2(
				FMath::Max(CheckBoxStyle.BackgroundImage.GetImageSize().X, CheckedBrush.GetImageSize().X),
				FMath::Max(CheckBoxStyle.BackgroundImage.GetImageSize().Y, CheckedBrush.GetImageSize().Y));
			ImageSize = ImTrunc(ImageSize * ImGui::GetStyle().FontScaleMain);
		}
		ImVec2 ImageSizeWithPadding{};
		if (bHasImage)
		{
			ImageSizeWithPadding = ImageSize + (bHasContent ? ImVec2(CheckBoxStyle.Padding.Left + CheckBoxStyle.Padding.Right, CheckBoxStyle.Padding.Top + CheckBoxStyle.Padding.Bottom) : ImVec2(0.f, 0.f));
		}

		const ImVec2 ContentSize = ImVec2(LabelSize.x + ImageSizeWithPadding.x, ImMax(LabelSize.y, ImageSizeWithPadding.y));
		const FMargin ContentPadding = !bHasImage ? CheckBoxStyle.Padding : FMargin(0.f);
		ImVec2 ItemRect = ContentSize + ImVec2(ContentPadding.Left + ContentPadding.Right, ContentPadding.Top + ContentPadding.Bottom);
		if (CheckBoxStyle.CheckBoxType == ESlateCheckBoxType::ToggleButton)
		{
			ApplyItemWidthOverride(ItemRect.x);
		}

		float ButtonOffsetY = 0.f;
		if (CheckBoxStyle.CheckBoxType == ESlateCheckBoxType::CheckBox)
		{
			if (ItemRect.y < ImGui::GetFrameHeight())
			{
				ButtonOffsetY = ImCeil((ImGui::GetFrameHeight() - ItemRect.y) * 0.5f);
			}
		}

		bool bPressed, bHovered;
		if (!InvisibleButton(Label, bPressed, bHovered, ItemRect, ImGuiButtonFlags_NoHoveredOnFocus|ImGuiButtonFlags_EnableNav, ImVec2(0.f, ButtonOffsetY)))
		{
			return false;
		}
		if (bPressed)
		{
			*Value = !(*Value);
		}
		bool bActive = bPressed || ImGui::IsItemActive();
		bool bChecked = *Value;
		bool bUndeterminedState = (TickContext->ImguiContext->LastItemData.ItemFlags & ImGuiItemFlags_MixedValue);

		ImVec4 ForegroundColor;
		const FSlateBrush* CheckedBrush = nullptr;
		if (bUndeterminedState)
		{
			CheckedBrush = bActive ? &CheckBoxStyle.UndeterminedPressedImage : (bHovered ? &CheckBoxStyle.UndeterminedHoveredImage : &CheckBoxStyle.UndeterminedImage);
			ForegroundColor = GetSlateColorVec4(bActive ? CheckBoxStyle.UndeterminedForeground : (bHovered ? CheckBoxStyle.HoveredForeground : CheckBoxStyle.ForegroundColor), TickContext);
		}
		else if (bChecked)
		{
			CheckedBrush = bActive ? &CheckBoxStyle.CheckedPressedImage : (bHovered ? &CheckBoxStyle.CheckedHoveredImage : &CheckBoxStyle.CheckedImage);
			ForegroundColor = GetSlateColorVec4(bActive ? CheckBoxStyle.CheckedPressedForeground : (bHovered ? CheckBoxStyle.CheckedHoveredForeground : CheckBoxStyle.CheckedForeground), TickContext);
		}
		else
		{
			CheckedBrush = bActive ? &CheckBoxStyle.UncheckedPressedImage : (bHovered ? &CheckBoxStyle.UncheckedHoveredImage : &CheckBoxStyle.UncheckedImage);
			ForegroundColor = GetSlateColorVec4(bActive ? CheckBoxStyle.PressedForeground : (bHovered ? CheckBoxStyle.HoveredForeground : CheckBoxStyle.ForegroundColor), TickContext);
		}

		if (CheckBoxStyle.CheckBoxType == ESlateCheckBoxType::CheckBox)
		{
			ImVec2 CursorPos = ImGui::GetItemRectMin();

			if (ImageSize.x > 0 && ImageSize.y > 0)
			{
				const FSlateBrush* BackgroundBrush = bActive ? &CheckBoxStyle.BackgroundPressedImage : (bHovered ? &CheckBoxStyle.BackgroundHoveredImage : &CheckBoxStyle.BackgroundImage);
				CursorPos.y = ImFloor((ImGui::GetItemRectMax().y + ImGui::GetItemRectMin().y) * 0.5f - ImageSize.y * 0.5f);
				DrawSlateBrush(TickContext, *BackgroundBrush, CursorPos, CursorPos + ImVec2(ImageSize.x, ImageSize.y));
				CursorPos.y = ImFloor((ImGui::GetItemRectMax().y + ImGui::GetItemRectMin().y) * 0.5f - ImageSize.y * 0.5f);
				DrawSlateBrush(TickContext, *CheckedBrush, CursorPos, CursorPos + ImVec2(ImageSize.x, ImageSize.y), ForegroundColor);
			}

			CursorPos.x += ImageSize.x + CheckBoxStyle.Padding.Left;
			CursorPos.y = ImFloor((ImGui::GetItemRectMax().y + ImGui::GetItemRectMin().y) * 0.5f - LabelSize.y * 0.5f);

			ImGui::PushStyleColor(ImGuiCol_Text, ForegroundColor);
			ImGui::RenderText(CursorPos, Label, LabelEnd);
			ImGui::PopStyleColor();

			if (IsItemHighlighted())
			{
				ImGui::GetWindowDrawList()->AddRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax(), ImGui::GetColorU32(ImGuiCol_NavCursor), ImGui::GetStyle().FrameRounding, 1.f);
			}
		}
		else if (ensure(CheckBoxStyle.CheckBoxType == ESlateCheckBoxType::ToggleButton))
		{
			DrawSlateBrush(TickContext, *CheckedBrush, ImGui::GetItemRectMin(), ImGui::GetItemRectMax(), {}, IsItemHighlighted() ? ImGui::GetColorU32(ImGuiCol_NavCursor) : TOptional<ImU32>{});

			ImRect LabelRect = { ImGui::GetItemRectMin() + ImVec2(ContentPadding.Left, ContentPadding.Top), ImGui::GetItemRectMax() - ImVec2(ContentPadding.Right, ContentPadding.Bottom) };

			ImGui::PushStyleColor(ImGuiCol_Text, ForegroundColor);
			ImGui::RenderTextClipped(LabelRect.Min, LabelRect.Max, Label, LabelEnd, &LabelSize, ImGui::GetStyle().ButtonTextAlign, &LabelRect);
			ImGui::PopStyleColor();
		}

		return bPressed;
	}

	bool ToggleButton(FImGuiTickContext* TickContext, const char* Label, bool* Value, const FImGuiImageBindingParams& Image, const FCheckBoxStyle& CheckBoxStyle)
	{
		ensure(CheckBoxStyle.CheckBoxType == ESlateCheckBoxType::ToggleButton);

		FImGuiTickScope Scope{ TickContext };

		ImVec2 ImageSizeWithPadding = Image.Size;

		const ImVec2 ContentSize = ImVec2(ImageSizeWithPadding.x, ImageSizeWithPadding.y);
		const FMargin ContentPadding = CheckBoxStyle.Padding;
		ImVec2 ItemRect = ContentSize + ImVec2(ContentPadding.Left + ContentPadding.Right, ContentPadding.Top + ContentPadding.Bottom);
		if (CheckBoxStyle.CheckBoxType == ESlateCheckBoxType::ToggleButton)
		{
			ApplyItemWidthOverride(ItemRect.x);
		}

		bool bPressed, bHovered;
		if (!InvisibleButton(Label, bPressed, bHovered, ItemRect, ImGuiButtonFlags_NoHoveredOnFocus | ImGuiButtonFlags_EnableNav))
		{
			return false;
		}
		if (bPressed)
		{
			*Value = !(*Value);
		}
		bool bActive = bPressed || ImGui::IsItemActive();
		bool bChecked = *Value;

		ImVec4 ForegroundColor;
		const FSlateBrush* CheckedBrush = nullptr;
		if (bChecked)
		{
			CheckedBrush = bActive ? &CheckBoxStyle.CheckedPressedImage : (bHovered ? &CheckBoxStyle.CheckedHoveredImage : &CheckBoxStyle.CheckedImage);
			ForegroundColor = GetSlateColorVec4(bActive ? CheckBoxStyle.CheckedPressedForeground : (bHovered ? CheckBoxStyle.CheckedHoveredForeground : CheckBoxStyle.CheckedForeground), TickContext);
		}
		else
		{
			CheckedBrush = bActive ? &CheckBoxStyle.UncheckedPressedImage : (bHovered ? &CheckBoxStyle.UncheckedHoveredImage : &CheckBoxStyle.UncheckedImage);
			ForegroundColor = GetSlateColorVec4(bActive ? CheckBoxStyle.PressedForeground : (bHovered ? CheckBoxStyle.HoveredForeground : CheckBoxStyle.ForegroundColor), TickContext);
		}

		DrawSlateBrush(TickContext, *CheckedBrush, ImGui::GetItemRectMin(), ImGui::GetItemRectMax(), {}, IsItemHighlighted() ? ImGui::GetColorU32(ImGuiCol_NavCursor) : TOptional<ImU32>{});

		ImVec2 CursorPos = (ImGui::GetItemRectMax() + ImGui::GetItemRectMin()) * 0.5f - Image.Size * 0.5f;
		CursorPos.y = ImFloor(CursorPos.y);
		ImGui::GetWindowDrawList()->AddImage(Image.Id, CursorPos, CursorPos + Image.Size, Image.UV0, Image.UV1, ImGui::GetColorU32(ForegroundColor));

		return bPressed;
	}

	bool ComboButton(FImGuiTickContext* TickContext, const char* Label, const char* PreviewText, const FComboButtonStyle& ComboButtonStyle)
	{
		FImGuiTickScope Scope{ TickContext };

		const FButtonStyle& ButtonStyle = ComboButtonStyle.ButtonStyle;

		const char* PreviewTextEnd = ImGui::FindRenderedTextEnd(PreviewText);
		const ImVec2 PreviewTextSize = ImGui::CalcTextSize(PreviewText, PreviewTextEnd);
		const ImVec2 DownImageSize = ImTrunc(ImVec2(ComboButtonStyle.DownArrowImage.GetImageSize().X, ComboButtonStyle.DownArrowImage.GetImageSize().Y) * ImGui::GetStyle().FontScaleMain);
		const ImVec2 DownImageSizeWithPadding = DownImageSize + ImVec2(ComboButtonStyle.DownArrowPadding.Left + ComboButtonStyle.DownArrowPadding.Right, ComboButtonStyle.DownArrowPadding.Top + ComboButtonStyle.DownArrowPadding.Bottom);
		const ImVec2 ContentSize = ImVec2(PreviewTextSize.x + DownImageSizeWithPadding.x, ImMax(PreviewTextSize.y, DownImageSizeWithPadding.y));
		const ImVec2 ContentPadding = ImVec2(
			(ComboButtonStyle.ContentPadding.Left + ComboButtonStyle.ContentPadding.Right) + FMath::Max(ButtonStyle.NormalPadding.Left + ButtonStyle.NormalPadding.Right, ButtonStyle.PressedPadding.Left + ButtonStyle.PressedPadding.Right),
			(ComboButtonStyle.ContentPadding.Top + ComboButtonStyle.ContentPadding.Bottom) + FMath::Max(ButtonStyle.NormalPadding.Top + ButtonStyle.NormalPadding.Bottom, ButtonStyle.PressedPadding.Top + ButtonStyle.PressedPadding.Bottom));
		ImVec2 ItemRect = ContentSize + ContentPadding;
		ApplyItemWidthOverride(ItemRect.x);

		bool bPressed, bHovered;
		if (!InvisibleButton(Label, bPressed, bHovered, ItemRect))
		{
			return false;
		}
		bool bActive = bPressed || ImGui::IsItemActive();

		const FSlateBrush* Brush = bActive ? &ButtonStyle.Pressed : (bHovered ? &ButtonStyle.Hovered : &ButtonStyle.Normal);
		DrawSlateBrush(TickContext, *Brush, ImGui::GetItemRectMin(), ImGui::GetItemRectMax(), {}, IsItemHighlighted() ? ImGui::GetColorU32(ImGuiCol_NavCursor) : TOptional<ImU32>{});

		const ImVec4 ForegroundColor = GetSlateColorVec4(bActive ? ButtonStyle.PressedForeground : (bHovered ? ButtonStyle.HoveredForeground : ButtonStyle.NormalForeground), TickContext);

		ImRect LabelRect = { ImGui::GetItemRectMin() + ImVec2(ComboButtonStyle.ContentPadding.Left, ComboButtonStyle.ContentPadding.Top), ImGui::GetItemRectMax() - ImVec2(ComboButtonStyle.ContentPadding.Right + DownImageSizeWithPadding.x, ComboButtonStyle.ContentPadding.Bottom) };
		LabelRect.Min += bActive ? ImVec2(ButtonStyle.PressedPadding.Left, ButtonStyle.PressedPadding.Top) : ImVec2(ButtonStyle.NormalPadding.Left, ButtonStyle.NormalPadding.Top);
		LabelRect.Max -= bActive ? ImVec2(ButtonStyle.PressedPadding.Right, ButtonStyle.PressedPadding.Bottom) : ImVec2(ButtonStyle.NormalPadding.Right, ButtonStyle.NormalPadding.Bottom);

		ImGui::PushStyleColor(ImGuiCol_Text, ForegroundColor);
		ImGui::RenderTextClipped(LabelRect.Min, LabelRect.Max, PreviewText, PreviewTextEnd, &PreviewTextSize, ImGui::GetStyle().ButtonTextAlign, &LabelRect);
		ImGui::PopStyleColor();

		float OffsetY = (ComboButtonStyle.ContentPadding.Top == 0) ? -1.f : 0.f;//TODO: account for this -1.f offset
		ImVec2 CursorPos;
		CursorPos.x = ImGui::GetItemRectMax().x - ContentPadding.x * 0.5f - DownImageSize.x;
		CursorPos.y = (ImGui::GetItemRectMax() + ImGui::GetItemRectMin()).y * 0.5f - DownImageSize.y * 0.5f + OffsetY;
		CursorPos.y += bActive ? ButtonStyle.PressedPadding.Top : ButtonStyle.NormalPadding.Top;
		CursorPos.y = ImFloor(CursorPos.y);

		DrawSlateBrush(TickContext, ComboButtonStyle.DownArrowImage, CursorPos, CursorPos + DownImageSize, ForegroundColor);

		return bPressed;
	}

	bool ComboButton(FImGuiTickContext* TickContext, const char* Label, const FImGuiImageBindingParams& Image, const FComboButtonStyle& ComboButtonStyle)
	{
		FImGuiTickScope Scope{ TickContext };

		const FButtonStyle& ButtonStyle = ComboButtonStyle.ButtonStyle;

		const ImVec2 DownImageSize = ImVec2(ComboButtonStyle.DownArrowImage.GetImageSize().X, ComboButtonStyle.DownArrowImage.GetImageSize().Y);
		const ImVec2 DownImageSizeWithPadding = DownImageSize + ImVec2(ComboButtonStyle.DownArrowPadding.Left + ComboButtonStyle.DownArrowPadding.Right, ComboButtonStyle.DownArrowPadding.Top + ComboButtonStyle.DownArrowPadding.Bottom);
		const ImVec2 ContentSize = ImVec2(Image.Size.x + DownImageSizeWithPadding.x, ImMax(Image.Size.y, DownImageSizeWithPadding.y));

		ImVec2 ItemRect = ContentSize + ImVec2(
			(ComboButtonStyle.ContentPadding.Left + ComboButtonStyle.ContentPadding.Right) + FMath::Max(ButtonStyle.NormalPadding.Left + ButtonStyle.NormalPadding.Right, ButtonStyle.PressedPadding.Left + ButtonStyle.PressedPadding.Right),
			(ComboButtonStyle.ContentPadding.Top + ComboButtonStyle.ContentPadding.Bottom) + FMath::Max(ButtonStyle.NormalPadding.Top + ButtonStyle.NormalPadding.Bottom, ButtonStyle.PressedPadding.Top + ButtonStyle.PressedPadding.Bottom));

		bool bPressed, bHovered;
		if (!InvisibleButton(Label, bPressed, bHovered, ItemRect))
		{
			return false;
		}
		bool bActive = bPressed || ImGui::IsItemActive();

		const FSlateBrush* Brush = bActive ? &ButtonStyle.Pressed : (bHovered ? &ButtonStyle.Hovered : &ButtonStyle.Normal);
		DrawSlateBrush(TickContext, *Brush, ImGui::GetItemRectMin(), ImGui::GetItemRectMax(), {}, IsItemHighlighted() ? ImGui::GetColorU32(ImGuiCol_NavCursor) : TOptional<ImU32>{});

		const ImVec4 ForegroundColor = GetSlateColorVec4(bActive ? ButtonStyle.PressedForeground : (bHovered ? ButtonStyle.HoveredForeground : ButtonStyle.NormalForeground), TickContext);

		ImVec2 CursorPos = ImGui::GetItemRectMin() + ImVec2(ComboButtonStyle.ContentPadding.Left + ButtonStyle.NormalPadding.Left, 0.f);
		CursorPos.y = (ImGui::GetItemRectMax() + ImGui::GetItemRectMin()).y * 0.5f - Image.Size.y * 0.5f;
		if (bActive)
		{
			CursorPos.y += (ButtonStyle.PressedPadding.Top - ButtonStyle.NormalPadding.Top);
		}
		CursorPos.y = ImFloor(CursorPos.y);
		ImGui::GetWindowDrawList()->AddImage(Image.Id, CursorPos, CursorPos + Image.Size, Image.UV0, Image.UV1, ImGui::GetColorU32(ForegroundColor));

		float OffsetY = (ComboButtonStyle.ContentPadding.Top == 0) ? -1.f : 0.f;//TODO: account for this -1.f offset
		CursorPos.x += Image.Size.x + ComboButtonStyle.DownArrowPadding.Left;
		CursorPos.y = (ImGui::GetItemRectMax() + ImGui::GetItemRectMin()).y * 0.5f - DownImageSize.y * 0.5f + OffsetY;
		CursorPos.y += bActive ? ButtonStyle.PressedPadding.Top : ButtonStyle.NormalPadding.Top;
		CursorPos.y = ImFloor(CursorPos.y);

		DrawSlateBrush(TickContext, ComboButtonStyle.DownArrowImage, CursorPos, CursorPos + DownImageSize, ForegroundColor);

		return bPressed;
	}

	bool EditableTextBox(FImGuiTickContext* TickContext, const char* Label, const char* Hint, char* Buffer, size_t BufferSize, ImGuiInputTextFlags Flags, ImGuiInputTextCallback Callback, void* UserData)
	{
		FImGuiTickScope Scope{ TickContext };

		ImGui::PushStyleColor(ImGuiCol_NavCursor, 0);
		bool bTextChanged;
		if (Hint)
		{
			bTextChanged = ImGui::InputTextWithHint(Label, Hint, Buffer, BufferSize, Flags, Callback, UserData);
		}
		else
		{
			bTextChanged = ImGui::InputText(Label, Buffer, BufferSize, Flags, Callback, UserData);
		}
		ImGui::PopStyleColor();

		if (ImGui::IsItemActive() || IsItemHighlighted())
		{
			ImGui::GetWindowDrawList()->AddRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax(), ImGui::GetColorU32(ImGuiCol_NavCursor), ImGui::GetStyle().FrameRounding);
		}
		else
		{
			ImGui::GetWindowDrawList()->AddRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax(), ImGui::GetColorU32(ImGui::GetStyle().Colors[ImGuiCol_Border] * (ImGui::IsItemHovered() ? 1.5f : 1.f)), ImGui::GetStyle().FrameRounding);
		}

		return bTextChanged;
	}

	static void AutoActivateNextItem(const char* ItemLabel)
	{
		const ImGuiID ItemId = ImGui::GetCurrentWindow()->GetID(ItemLabel);
		ImGui::PushID(ItemId);

		const ImGuiID StorageId = ImGui::GetCurrentWindow()->GetID("AutoActivate");
		const bool bHadFocus = ImGui::GetStateStorage()->GetBool(StorageId);
		const bool bHasFocus = (ImGui::GetFocusID() == ItemId);

		if (bHasFocus && !bHadFocus)
		{
			ImGui::SetKeyboardFocusHere();
		}

		ImGui::GetStateStorage()->SetBool(StorageId, bHasFocus);
		ImGui::PopID();
	}
	bool SearchTextBox(FImGuiTickContext* TickContext, const char* Label, const char* Hint, char* Buffer, size_t BufferSize, const FSearchBoxStyle& SearchBoxStyle, bool bSetFocus, bool bActivateOnNavFocus, ImGuiInputTextFlags Flags, ImGuiInputTextCallback Callback, void* UserData)
	{
		FImGuiTickScope Scope{ TickContext };
		FImGuiNamedScope WidgetScope{ Label };

		bool bTextChanged = false;

		ImVec2 OriginalCursorPos = ImGui::GetCursorPos();
		float ItemWidth = ImGui::CalcItemWidth();

		ImGui::BeginGroup();
		{
			static const char* InputTextLabel = "##Filter";
			const ImGuiID InputTextId = ImGui::GetID(InputTextLabel);

			int32 FrameHeight = ImGui::GetFrameHeight();
			float FramePaddingY = ImGui::GetStyle().FramePadding.y;
			if ((FrameHeight & 0x1) > 0)
			{
				FramePaddingY += 0.5f;
			}

			ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0.f, FramePaddingY));
			ImGui::PushStyleVarX(ImGuiStyleVar_ItemSpacing, 0.f);

			const FSlateBrush* ImageBrush = nullptr;
			if (FCStringAnsi::Strlen(Buffer) > 0)
			{
				ImageBrush = &SearchBoxStyle.ClearImage;
			}
			else
			{
				ImageBrush = &SearchBoxStyle.GlassImage;
			}
			FVector2f ImageSize = (SearchBoxStyle.ImageSizeOverride.IsSet() ? FVector2f(SearchBoxStyle.ImageSizeOverride.GetValue()) : ImageBrush->GetImageSize()) * ImGui::GetStyle().FontScaleMain;

			// calculate InputText rect for hover test (for background image)
			ImRect InputTextRect{ ImGui::GetCursorScreenPos(), ImGui::GetCursorScreenPos() + ImVec2(ItemWidth, ImGui::GetFrameHeight()) };
			if (SearchBoxStyle.TextBoxStyle.BackgroundImageNormal.OutlineSettings.RoundingType == ESlateBrushRoundingType::HalfHeightRadius)
			{
				InputTextRect.Max.x -= ImGui::GetFrameHeight() * 0.5f;
			}
			else
			{
				InputTextRect.Max.x -= SearchBoxStyle.TextBoxStyle.BackgroundImageNormal.OutlineSettings.CornerRadii.X;
			}
			InputTextRect.Min.x += SearchBoxStyle.ImagePadding.Right + SearchBoxStyle.TextBoxStyle.Padding.Left + ImageSize.X + (SearchBoxStyle.ImagePadding.Left + (SearchBoxStyle.ImagePadding.Left + SearchBoxStyle.ImagePadding.Right) * 0.5f);

			const bool bActive = ImGui::GetActiveID() == InputTextId;
			const bool bInputTextHovered = ((GImGui->CurrentItemFlags & ImGuiItemFlags_Disabled) == 0) && ImGui::IsMouseHoveringRect(InputTextRect.Min, InputTextRect.Max);
			const FSlateBrush* BackgroundBrush = bActive ? &SearchBoxStyle.TextBoxStyle.BackgroundImageFocused : (bInputTextHovered ? &SearchBoxStyle.TextBoxStyle.BackgroundImageHovered : &SearchBoxStyle.TextBoxStyle.BackgroundImageNormal);
			DrawSlateBrush(TickContext, *BackgroundBrush, ImGui::GetCursorScreenPos(), ImGui::GetCursorScreenPos() + ImVec2(ItemWidth, ImGui::GetFrameHeight()), {}, IsItemHighlighted(InputTextId) ? ImGui::GetColorU32(ImGuiCol_NavCursor) : TOptional<ImU32>{});

			uint32 FocusedForegroundColor = GetSlateColorU32(SearchBoxStyle.TextBoxStyle.FocusedForegroundColor, TickContext);
			uint32 NormalForegroundColor = GetSlateColorU32(SearchBoxStyle.TextBoxStyle.ForegroundColor, TickContext);

			bool bClearText = false;
			{
				FSlateColor ImageTint = SearchBoxStyle.TextBoxStyle.ForegroundColor;

				// HAlign_Center
				ImGui::SetCursorPosX(ImGui::GetCursorPosX() + SearchBoxStyle.ImagePadding.Left + (SearchBoxStyle.ImagePadding.Left + SearchBoxStyle.ImagePadding.Right) * 0.5f);
				if (FCStringAnsi::Strlen(Buffer) > 0)
				{
					ImGui::SetCursorPosY(ImGui::GetCursorPosY() + ImGui::GetFrameHeight() * 0.5f - ImageSize.Y * 0.5f);
					bClearText = ImGui::InvisibleButton("Clear", ImVec2(ImageSize.X, ImageSize.Y));

					if (((GImGui->CurrentItemFlags & ImGuiItemFlags_Disabled) == 0) && ImGui::IsMouseHoveringRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax()))
					{
						ImageTint = SearchBoxStyle.TextBoxStyle.FocusedForegroundColor;
					}
				}
				else
				{
					ImGui::SetCursorPosY(ImGui::GetCursorPosY() + ImGui::GetFrameHeight() * 0.5f - ImageSize.Y * 0.5f);
					ImGui::Dummy(ImVec2(ImageSize.X, ImageSize.Y));

					if (bActive)
					{
						ImageTint = SearchBoxStyle.TextBoxStyle.FocusedForegroundColor;
					}
				}
				ImRect ImageRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax());

				ImVec2 ImagePos(ImCeil(ImageRect.Min.x), ImCeil(ImageRect.Min.y));
				DrawSlateBrush(TickContext, *ImageBrush, ImagePos, ImagePos + ImageRect.GetSize(), GetSlateColorVec4(ImageTint, TickContext));
			}
			ImGui::SameLine();
			ImGui::SetCursorPosX(ImGui::GetCursorPosX() + SearchBoxStyle.ImagePadding.Right + SearchBoxStyle.TextBoxStyle.Padding.Left);
			ImGui::SetCursorPosY(OriginalCursorPos.y);
			{
				FImGuiScopedStyleEdit StyleEdits
				{
					ImGuiStyleVar_FrameRounding, 0.f,
					ImGuiStyleVar_FrameBorderSize, 0.f,

					ImGuiCol_FrameBg, 0,
					ImGuiCol_FrameBgActive, 0,
					ImGuiCol_FrameBgHovered, 0,
					ImGuiCol_NavCursor, 0,
				};

				if (bSetFocus)
				{
					ImGui::SetKeyboardFocusHere();
				}

				if (bActivateOnNavFocus)
				{
					AutoActivateNextItem(InputTextLabel);
				}

				ImGui::PushStyleColor(ImGuiCol_Text, bActive ? FocusedForegroundColor : NormalForegroundColor);
				ImGui::SetNextItemWidth(ItemWidth - ImGui::GetFrameHeight() - 16.f);
				if (bClearText)
				{
					Buffer[0] = 0;
					ImGui::SetKeyboardFocusHere();
				}
				bTextChanged = ImGui::InputTextWithHint(InputTextLabel, "Search", Buffer, BufferSize) || bClearText;
				ImGui::PopStyleColor();
			}
			ImGui::PopStyleVar(2);

			ImGui::SameLine();
			if ((ImGui::GetCursorPosX() - OriginalCursorPos.x) < ItemWidth)
			{
				ImGui::Dummy(ImVec2(ItemWidth - (ImGui::GetCursorPosX() - OriginalCursorPos.x), 4.f));
			}
			else
			{
				ImGui::NewLine();
			}
		}
		ImGui::EndGroup();

		return bTextChanged;
	}

	void PushComboListViewDropdownStyle(FImGuiTickContext* TickContext, bool bDarkTheme)
	{
		ImVec4 BgColor;
		if (bDarkTheme)
		{
			BgColor = ImColor(0xF01A1A1A);
		}
		else
		{
			BgColor = FImGui::GetSlateColorVec4(TickContext, EStyleColor::Dropdown) * ImVec4(1.f, 1.f, 1.f, 0.94f);
		}
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(1.f, 1.f));
		ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.f);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.f, 0.f));
		ImGui::PushStyleVar(ImGuiStyleVar_PopupBorderSize, 0.f);
		ImGui::PushStyleColor(ImGuiCol_FrameBg, BgColor);
		ImGui::PushStyleColor(ImGuiCol_PopupBg, BgColor);
	}
	void PopComboListViewDropdownStyle(FImGuiTickContext* TickContext)
	{
		ImGui::PopStyleColor(2);
		ImGui::PopStyleVar(4);
	}
	void BeginComboListViewDropdownBody(FImGuiTickContext* TickContext)
	{
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2());
		ImGui::PushStyleColor(ImGuiCol_Header, FImGui::GetSlateColorVec4(TickContext, EStyleColor::SelectInactive));
		ImGui::PushStyleColor(ImGuiCol_HeaderActive, FImGui::GetSlateColorVec4(TickContext, EStyleColor::Select));
		ImGui::PushStyleColor(ImGuiCol_HeaderHovered, FImGui::GetSlateColorVec4(TickContext, EStyleColor::Select));

		// account for selectable half space padding
		ImGui::SetCursorPosY(ImGui::GetCursorPosY() + ImGui::GetStyle().ItemSpacing.y);
	}
	void EndComboListViewDropdownBody(FImGuiTickContext* TickContext)
	{
		ImGui::PopStyleVar();
		ImGui::PopStyleColor(3);

		// account for selectable half space padding
		ImGui::Dummy(ImVec2(0.f, 1.f));
	}
}
