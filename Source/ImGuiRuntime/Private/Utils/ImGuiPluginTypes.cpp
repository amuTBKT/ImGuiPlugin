// Copyright 2024-26 Amit Kumar Mehar. All Rights Reserved.

#include "ImGuiPluginTypes.h"
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

		case ESlateColorStylingMode::UseColor_Specified:
			return ImColor(PrivateAccess::SpecifiedColor(Color).ToFColorSRGB().DWColor());

		case ESlateColorStylingMode::UseColor_ColorTable:
			return BitCast<ImVec4>((*TickContext->SlateColorsVec4)[(uint32)PrivateAccess::ColorTableId(Color)]);
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

		case ESlateColorStylingMode::UseColor_Specified:
			return PrivateAccess::SpecifiedColor(Color).ToFColorSRGB().DWColor();

		case ESlateColorStylingMode::UseColor_ColorTable:
			return (*TickContext->SlateColorsU32)[(uint32)PrivateAccess::ColorTableId(Color)];
		}
	}

	FORCEINLINE static void PathRect(ImDrawList& DrawList, const ImVec2& RectMin, const ImVec2& RectMax, const ImVec4& Rounding)
	{
		const float RoundingTopLeft = Rounding.x;
		const float RoundingTopRight = Rounding.y;
		const float RoundingBottomRight = Rounding.z;
		const float RoundingBottomLeft = Rounding.w;
		DrawList.PathArcToFast(ImVec2(RectMin.x + RoundingTopLeft, RectMin.y + RoundingTopLeft), RoundingTopLeft, 6, 9);
		DrawList.PathArcToFast(ImVec2(RectMax.x - RoundingTopRight, RectMin.y + RoundingTopRight), RoundingTopRight, 9, 12);
		DrawList.PathArcToFast(ImVec2(RectMax.x - RoundingBottomRight, RectMax.y - RoundingBottomRight), RoundingBottomRight, 0, 3);
		DrawList.PathArcToFast(ImVec2(RectMin.x + RoundingBottomLeft, RectMax.y - RoundingBottomLeft), RoundingBottomLeft, 3, 6);
	}

	FORCEINLINE static void AddImageRounded(ImDrawList& DrawList, ImTextureRef Texture, const ImVec2& RectMin, const ImVec2& RectMax, const ImVec2 UV0, const ImVec2& UV1, ImU32 Color, const ImVec4& Rounding)
	{
		ImVec2 RectSize = RectMax - RectMin;
		if (ImMin(RectSize.x, RectSize.y) <= KINDA_SMALL_NUMBER)
		{
			return;
		}

		const bool bPushTextureId = DrawList._CmdHeader.TexRef != Texture;
		if (bPushTextureId)
		{
			DrawList.PushTexture(Texture);
		}

		int32 VertexStartIdx = DrawList.VtxBuffer.Size;
		PathRect(DrawList, RectMin, RectMax, Rounding);
		DrawList.PathFillConvex(Color, ImDrawFlags_JoinMiter);
		int32 VertexEndIdx = DrawList.VtxBuffer.Size;
		ImGui::ShadeVertsLinearUV(&DrawList, VertexStartIdx, VertexEndIdx, RectMin, RectMax, UV0, UV1, true);

		if (bPushTextureId)
		{
			DrawList.PopTexture();
		}
	}

	void DrawSlateBrush(
		const FImGuiTickContext* TickContext, const FSlateBrush& Brush, ImVec2 RectMin, ImVec2 RectMax,
		TOptional<ImVec4> Tint, TOptional<ImU32> BorderColor)
	{
		DECLARE_SCOPE_CYCLE_COUNTER(TEXT("DrawSlateBrush"), STAT_ImGui_DrawSlateBrush, STATGROUP_ImGui);

		ImVec2 RectSize = RectMax - RectMin;
		if (ImMin(RectSize.x, RectSize.y) <= KINDA_SMALL_NUMBER)
		{
			return;
		}

		if (Brush.GetDrawType() == ESlateBrushDrawType::NoDrawType)
		{
			if (BorderColor.IsSet())
			{
				ImGui::GetWindowDrawList()->AddRect(RectMin, RectMax, ImGui::GetColorU32(BorderColor.GetValue()), ImGui::GetStyle().FrameRounding, 1.f);
			}
			return;
		}

		ImVec4 Rounding;
		bool bUniformRounding = true;
		if (Brush.GetDrawType() == ESlateBrushDrawType::RoundedBox)
		{
			if (Brush.OutlineSettings.RoundingType == ESlateBrushRoundingType::HalfHeightRadius)
			{
				Rounding.x = ImMin(RectSize.x, RectSize.y) * 0.5f;
			}
			else
			{
				if (Brush.OutlineSettings.CornerRadii.X != Brush.OutlineSettings.CornerRadii.Y ||
					Brush.OutlineSettings.CornerRadii.X != Brush.OutlineSettings.CornerRadii.Z ||
					Brush.OutlineSettings.CornerRadii.X != Brush.OutlineSettings.CornerRadii.W)
				{
					bUniformRounding = false;
					Rounding = ImVec4(Brush.OutlineSettings.CornerRadii.X, Brush.OutlineSettings.CornerRadii.Y, Brush.OutlineSettings.CornerRadii.Z, Brush.OutlineSettings.CornerRadii.W);

					// TODO: maybe we can skip this clamping logic
					if (Rounding.x + Rounding.y > RectSize.x)
					{
						float F = (Rounding.x > 0 ? 1.f : 0.f) + (Rounding.y > 0 ? 1.f : 0.f);
						float D = (Rounding.x + Rounding.y) - RectSize.x;
						Rounding.x = ImMax(0.f, Rounding.x - D / F);
						Rounding.y = ImMax(0.f, Rounding.y - D / F);
					}
					if (Rounding.z + Rounding.w > RectSize.x)
					{
						float F = (Rounding.z > 0 ? 1.f : 0.f) + (Rounding.w > 0 ? 1.f : 0.f);
						float D = (Rounding.z + Rounding.w) - RectSize.x;
						Rounding.z = ImMax(0.f, Rounding.z - D / F);
						Rounding.w = ImMax(0.f, Rounding.w - D / F);
					}
					if (Rounding.x + Rounding.w > RectSize.y)
					{
						float F = (Rounding.x > 0 ? 1.f : 0.f) + (Rounding.w > 0 ? 1.f : 0.f);
						float D = (Rounding.x + Rounding.w) - RectSize.y;
						Rounding.x = ImMax(0.f, Rounding.x - D / F);
						Rounding.w = ImMax(0.f, Rounding.w - D / F);
					}
					if (Rounding.y + Rounding.z > RectSize.y)
					{
						float F = (Rounding.y > 0 ? 1.f : 0.f) + (Rounding.z > 0 ? 1.f : 0.f);
						float D = (Rounding.y + Rounding.z) - RectSize.y;
						Rounding.y = ImMax(0.f, Rounding.y - D / F);
						Rounding.z = ImMax(0.f, Rounding.z - D / F);
					}
				}
				else
				{
					Rounding.x = Brush.OutlineSettings.CornerRadii.X;
					float MinRectDim = ImMin(RectSize.x, RectSize.y);
					if (Rounding.x * 2.f > MinRectDim)
					{
						Rounding.x = MinRectDim * 0.5f;
					}
				}
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
		if ((ImageColor & IM_COL32_A_MASK) > 0)
		{
			switch (Brush.GetDrawType())
			{
			default:
			case ESlateBrushDrawType::Box:
			case ESlateBrushDrawType::RoundedBox:
			case ESlateBrushDrawType::Image:
			{
				// based on FSlateElementBatcher::AddBoxElements

				if (Brush.GetImageType() != ESlateBrushImageType::NoImage)
				{
					auto Image = UImGuiSubsystem::Get()->RegisterOneFrameResource(&Brush, FVector2f(RectSize.x, RectSize.y));

					ImVec2 StartUV = Image.UV0;
					ImVec2 EndUV = Image.UV1;
					ImVec2 SizeUV = EndUV - StartUV;

					float TextureWidth = 1.f;
					float TextureHeight = 1.f;
					if (auto ResourceProxy = Brush.GetRenderingResource(FVector2f(RectSize.x, RectSize.y), 1.f).GetResourceProxy())
					{
						TextureWidth = ResourceProxy->ActualSize.X != 0 ? ResourceProxy->ActualSize.X : 1.f;
						TextureHeight = ResourceProxy->ActualSize.Y != 0 ? ResourceProxy->ActualSize.Y : 1.f;
					}
					ImVec2 HalfTexel = ImVec2(0.f, 0.f) / ImVec2(TextureWidth, TextureHeight); //TODO: not handled

					const ESlateBrushTileType::Type TilingRule = Brush.GetTiling();
					const bool bTileHorizontal = (TilingRule == ESlateBrushTileType::Both || TilingRule == ESlateBrushTileType::Horizontal);
					const bool bTileVertical = (TilingRule == ESlateBrushTileType::Both || TilingRule == ESlateBrushTileType::Vertical);

					const ESlateBrushMirrorType::Type MirroringRule = Brush.GetMirroring();
					const bool bMirrorHorizontal = (MirroringRule == ESlateBrushMirrorType::Both || MirroringRule == ESlateBrushMirrorType::Horizontal);
					const bool bMirrorVertical = (MirroringRule == ESlateBrushMirrorType::Both || MirroringRule == ESlateBrushMirrorType::Vertical);

					float HorizontalTiling = bTileHorizontal ? RectSize.x / TextureWidth : 1.0f;
					float VerticalTiling = bTileVertical ? RectSize.y / TextureHeight : 1.0f;
					const ImVec2 Tiling(HorizontalTiling, VerticalTiling);

					const FMargin Margin = (Brush.GetDrawType() != ESlateBrushDrawType::Image) ? Brush.GetMargin() : FMargin(0.f);
					if (Margin.Left != 0.0f || Margin.Top != 0.0f || Margin.Right != 0.0f || Margin.Bottom != 0.0f)
					{
						float LeftMarginU = (Margin.Left > 0.0f) ? StartUV.x + Margin.Left * SizeUV.x + HalfTexel.x : StartUV.x;
						float TopMarginV = (Margin.Top > 0.0f) ? StartUV.y + Margin.Top * SizeUV.y + HalfTexel.y : StartUV.y;
						float RightMarginU = (Margin.Right > 0.0f) ? EndUV.x - Margin.Right * SizeUV.x + HalfTexel.x : EndUV.x;
						float BottomMarginV = (Margin.Bottom > 0.0f) ? EndUV.y - Margin.Bottom * SizeUV.y + HalfTexel.y : EndUV.y;

						if (bMirrorHorizontal || bMirrorVertical)
						{
							const ImVec2 UVMin = StartUV;
							const ImVec2 UVMax = EndUV;

							if (bMirrorHorizontal)
							{
								StartUV.x = UVMax.x - (StartUV.x - UVMin.x);
								EndUV.x = UVMax.x - (EndUV.x - UVMin.x);
								LeftMarginU = UVMax.x - (LeftMarginU - UVMin.x);
								RightMarginU = UVMax.x - (RightMarginU - UVMin.x);
							}
							if (bMirrorVertical)
							{
								StartUV.y = UVMax.y - (StartUV.y - UVMin.y);
								EndUV.y = UVMax.y - (EndUV.y - UVMin.y);
								TopMarginV = UVMax.y - (TopMarginV - UVMin.y);
								BottomMarginV = UVMax.y - (BottomMarginV - UVMin.y);
							}
						}

						float LeftMarginX = TextureWidth * Margin.Left;
						float TopMarginY = TextureHeight * Margin.Top;
						float RightMarginX = RectSize.x - TextureWidth * Margin.Right;
						float BottomMarginY = RectSize.y - TextureHeight * Margin.Bottom;
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

						// TODO: disable rounding when using margin, too much effort to support that :(
						bUniformRounding = true;
						Rounding.x = 0.f;
						if (bUniformRounding && Rounding.x == 0)
						{
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

							ImVec2 Position = RectMin;
							ImVec2 EndPos = RectMax;
							LeftMarginX += Position.x;
							RightMarginX += Position.x;
							TopMarginY += Position.y;
							BottomMarginY += Position.y;

							DrawList->PrimWriteVtx(Position, StartUV * Tiling, ImageColor);
							DrawList->PrimWriteVtx(ImVec2(Position.x, TopMarginY), ImVec2(StartUV.x, TopMarginV) * Tiling, ImageColor);
							DrawList->PrimWriteVtx(ImVec2(LeftMarginX, Position.y), ImVec2(LeftMarginU, StartUV.y) * Tiling, ImageColor);
							DrawList->PrimWriteVtx(ImVec2(LeftMarginX, TopMarginY), ImVec2(LeftMarginU, TopMarginV) * Tiling, ImageColor);
							DrawList->PrimWriteVtx(ImVec2(RightMarginX, Position.y), ImVec2(RightMarginU, StartUV.y) * Tiling, ImageColor);
							DrawList->PrimWriteVtx(ImVec2(RightMarginX, TopMarginY), ImVec2(RightMarginU, TopMarginV) * Tiling, ImageColor);
							DrawList->PrimWriteVtx(ImVec2(EndPos.x, Position.y), ImVec2(EndUV.x, StartUV.y) * Tiling, ImageColor);
							DrawList->PrimWriteVtx(ImVec2(EndPos.x, TopMarginY), ImVec2(EndUV.x, TopMarginV) * Tiling, ImageColor);

							DrawList->PrimWriteVtx(ImVec2(Position.x, BottomMarginY), ImVec2(StartUV.x, BottomMarginV) * Tiling, ImageColor);
							DrawList->PrimWriteVtx(ImVec2(LeftMarginX, BottomMarginY), ImVec2(LeftMarginU, BottomMarginV) * Tiling, ImageColor);
							DrawList->PrimWriteVtx(ImVec2(RightMarginX, BottomMarginY), ImVec2(RightMarginU, BottomMarginV) * Tiling, ImageColor);
							DrawList->PrimWriteVtx(ImVec2(EndPos.x, BottomMarginY), ImVec2(EndUV.x, BottomMarginV) * Tiling, ImageColor);
							DrawList->PrimWriteVtx(ImVec2(Position.x, EndPos.y), ImVec2(StartUV.x, EndUV.y) * Tiling, ImageColor);
							DrawList->PrimWriteVtx(ImVec2(LeftMarginX, EndPos.y), ImVec2(LeftMarginU, EndUV.y) * Tiling, ImageColor);
							DrawList->PrimWriteVtx(ImVec2(RightMarginX, EndPos.y), ImVec2(RightMarginU, EndUV.y) * Tiling, ImageColor);
							DrawList->PrimWriteVtx(ImVec2(EndPos.x, EndPos.y), EndUV * Tiling, ImageColor);

							DrawList->PopTexture();
						}
						else
						{
						}
					}
					else
					{
						if (bMirrorHorizontal || bMirrorVertical)
						{
							const ImVec2 UVMin = StartUV;
							const ImVec2 UVMax = EndUV;

							if (bMirrorHorizontal)
							{
								StartUV.x = UVMax.x - (StartUV.x - UVMin.x);
								EndUV.x = UVMax.x - (EndUV.x - UVMin.x);
							}
							if (bMirrorVertical)
							{
								StartUV.y = UVMax.y - (StartUV.y - UVMin.y);
								EndUV.y = UVMax.y - (EndUV.y - UVMin.y);
							}
						}

						if (bUniformRounding)
						{
							if (Rounding.x > 0.f)
							{
								DrawList->AddImageRounded(Image.Id, RectMin, RectMax, StartUV * Tiling, EndUV * Tiling, ImageColor, Rounding.x);
							}
							else
							{
								DrawList->AddImage(Image.Id, RectMin, RectMax, StartUV * Tiling, EndUV * Tiling, ImageColor);
							}
						}
						else
						{
							AddImageRounded(*DrawList, Image.Id, RectMin, RectMax, StartUV * Tiling, EndUV * Tiling, ImageColor, Rounding);
						}
					}
				}
				else
				{
					if (bUniformRounding)
					{
						DrawList->AddRectFilled(RectMin, RectMax, ImageColor, Rounding.x);
					}
					else
					{
						PathRect(*DrawList, RectMin, RectMax, Rounding);
						DrawList->PathFillConvex(ImageColor, ImDrawFlags_JoinMiter);
					}
				}
			}break;
			case ESlateBrushDrawType::Border:
			{
				// based on FSlateElementBatcher::AddBorderElement

				if (Brush.GetImageType() != ESlateBrushImageType::NoImage)
				{
					auto Image = UImGuiSubsystem::Get()->RegisterOneFrameResource(&Brush, FVector2f(RectSize.x, RectSize.y));

					float TextureWidth = 1.f;
					float TextureHeight = 1.f;
					if (auto ResourceProxy = Brush.GetRenderingResource(FVector2f(RectSize.x, RectSize.y), 1.f).GetResourceProxy())
					{
						TextureWidth = ResourceProxy->ActualSize.X != 0 ? ResourceProxy->ActualSize.X : 1.f;
						TextureHeight = ResourceProxy->ActualSize.Y != 0 ? ResourceProxy->ActualSize.Y : 1.f;
					}
					ImVec2 HalfTexel = ImVec2(0.f, 0.f) / ImVec2(TextureWidth, TextureHeight); //TODO: not handled

					ImVec2 StartUV = HalfTexel;
					ImVec2 EndUV = ImVec2(1.f, 1.f) + HalfTexel;

					ImVec2 TextureSizeLocalSpace = ImVec2((float)TextureWidth, (float)TextureHeight);

					FMargin Margin = Brush.GetMargin();

					// Determine the margins for each quad
					ImVec2 TopLeftMargin(TextureSizeLocalSpace * ImVec2(Margin.Left, Margin.Top));
					ImVec2 BotRightMargin(RectSize - TextureSizeLocalSpace * ImVec2(Margin.Right, Margin.Bottom));

					float LeftMarginX = TopLeftMargin.x;
					float TopMarginY = TopLeftMargin.y;
					float RightMarginX = BotRightMargin.x;
					float BottomMarginY = BotRightMargin.y;

					// If the margins are overlapping the margins are too big or the button is too small
					// so clamp margins to half of the box size
					if (FMath::Abs(RightMarginX) < FMath::Abs(LeftMarginX))
					{
						LeftMarginX = RectSize.x / 2.f;
						RightMarginX = LeftMarginX;
					}

					if (FMath::Abs(BottomMarginY) < FMath::Abs(TopMarginY))
					{
						TopMarginY = RectSize.y / 2.f;
						BottomMarginY = TopMarginY;
					}

					// Determine the texture coordinates for each quad
					float LeftMarginU = FMath::Abs(Margin.Left);
					float TopMarginV = FMath::Abs(Margin.Top);
					float RightMarginU = 1.0f - FMath::Abs(Margin.Right);
					float BottomMarginV = 1.0f - FMath::Abs(Margin.Bottom);

					LeftMarginU += HalfTexel.x;
					TopMarginV += HalfTexel.y;
					BottomMarginV += HalfTexel.y;
					RightMarginU += HalfTexel.x;

					// Determine the amount of tiling needed for the texture in this element.  The formula is number of pixels covered by the tiling portion of the texture / the number number of texels corresponding to the tiled portion of the texture.
					float TopTiling = 1.0f;
					float LeftTiling = 1.0f;
					float Denom = TextureSizeLocalSpace.x * (1.0f - Margin.GetTotalSpaceAlong<Orient_Horizontal>());
					if (!FMath::IsNearlyZero(Denom))
					{
						TopTiling = (RightMarginX - LeftMarginX) / Denom;
					}
					Denom = TextureSizeLocalSpace.y * (1.0f - Margin.GetTotalSpaceAlong<Orient_Vertical>());
					if (!FMath::IsNearlyZero(Denom))
					{
						LeftTiling = (BottomMarginY - TopMarginY) / Denom;
					}

					// Ensure tiling of at least 1.
					TopTiling = TopTiling >= 1.0f ? TopTiling : 1.0f;
					LeftTiling = LeftTiling >= 1.0f ? LeftTiling : 1.0f;
					float RightTiling = LeftTiling;
					float BottomTiling = TopTiling;

					ImVec2 Position = RectMin;
					ImVec2 EndPos = RectMax;
					LeftMarginX += Position.x;
					RightMarginX += Position.x;
					TopMarginY += Position.y;
					BottomMarginY += Position.y;

					DrawList->PushTexture(Image.Id);

					DrawList->PrimReserve(48, 32);
					auto IndexStart = DrawList->_VtxCurrentIdx;

					// Top
					DrawList->PrimWriteIdx(IndexStart + 0);
					DrawList->PrimWriteIdx(IndexStart + 1);
					DrawList->PrimWriteIdx(IndexStart + 2);
					DrawList->PrimWriteIdx(IndexStart + 2);
					DrawList->PrimWriteIdx(IndexStart + 1);
					DrawList->PrimWriteIdx(IndexStart + 3);

					DrawList->PrimWriteIdx(IndexStart + 4);
					DrawList->PrimWriteIdx(IndexStart + 5);
					DrawList->PrimWriteIdx(IndexStart + 6);
					DrawList->PrimWriteIdx(IndexStart + 6);
					DrawList->PrimWriteIdx(IndexStart + 5);
					DrawList->PrimWriteIdx(IndexStart + 7);

					DrawList->PrimWriteIdx(IndexStart + 8);
					DrawList->PrimWriteIdx(IndexStart + 9);
					DrawList->PrimWriteIdx(IndexStart + 10);
					DrawList->PrimWriteIdx(IndexStart + 10);
					DrawList->PrimWriteIdx(IndexStart + 9);
					DrawList->PrimWriteIdx(IndexStart + 11);

					// Middle
					DrawList->PrimWriteIdx(IndexStart + 12);
					DrawList->PrimWriteIdx(IndexStart + 13);
					DrawList->PrimWriteIdx(IndexStart + 14);
					DrawList->PrimWriteIdx(IndexStart + 14);
					DrawList->PrimWriteIdx(IndexStart + 13);
					DrawList->PrimWriteIdx(IndexStart + 15);

					DrawList->PrimWriteIdx(IndexStart + 16);
					DrawList->PrimWriteIdx(IndexStart + 17);
					DrawList->PrimWriteIdx(IndexStart + 18);
					DrawList->PrimWriteIdx(IndexStart + 18);
					DrawList->PrimWriteIdx(IndexStart + 17);
					DrawList->PrimWriteIdx(IndexStart + 19);

					// Bottom
					DrawList->PrimWriteIdx(IndexStart + 20);
					DrawList->PrimWriteIdx(IndexStart + 21);
					DrawList->PrimWriteIdx(IndexStart + 22);
					DrawList->PrimWriteIdx(IndexStart + 22);
					DrawList->PrimWriteIdx(IndexStart + 21);
					DrawList->PrimWriteIdx(IndexStart + 23);

					DrawList->PrimWriteIdx(IndexStart + 24);
					DrawList->PrimWriteIdx(IndexStart + 25);
					DrawList->PrimWriteIdx(IndexStart + 26);
					DrawList->PrimWriteIdx(IndexStart + 26);
					DrawList->PrimWriteIdx(IndexStart + 25);
					DrawList->PrimWriteIdx(IndexStart + 27);

					DrawList->PrimWriteIdx(IndexStart + 28);
					DrawList->PrimWriteIdx(IndexStart + 29);
					DrawList->PrimWriteIdx(IndexStart + 30);
					DrawList->PrimWriteIdx(IndexStart + 30);
					DrawList->PrimWriteIdx(IndexStart + 29);
					DrawList->PrimWriteIdx(IndexStart + 31);

					DrawList->PrimWriteVtx(Position, StartUV, ImageColor);
					DrawList->PrimWriteVtx(ImVec2(Position.x, TopMarginY), ImVec2(StartUV.x, TopMarginV), ImageColor);
					DrawList->PrimWriteVtx(ImVec2(LeftMarginX, Position.y), ImVec2(LeftMarginU, StartUV.y), ImageColor);
					DrawList->PrimWriteVtx(ImVec2(LeftMarginX, TopMarginY), ImVec2(LeftMarginU, TopMarginV), ImageColor);

					DrawList->PrimWriteVtx(ImVec2(LeftMarginX, Position.y), ImVec2(StartUV.x * TopTiling, StartUV.y), ImageColor);
					DrawList->PrimWriteVtx(ImVec2(LeftMarginX, TopMarginY), ImVec2(StartUV.x * TopTiling, TopMarginV), ImageColor);
					DrawList->PrimWriteVtx(ImVec2(RightMarginX, Position.y), ImVec2(EndUV.x * TopTiling, StartUV.y), ImageColor);
					DrawList->PrimWriteVtx(ImVec2(RightMarginX, TopMarginY), ImVec2(EndUV.x * TopTiling, TopMarginV), ImageColor);

					DrawList->PrimWriteVtx(ImVec2(RightMarginX, Position.y), ImVec2(RightMarginU, StartUV.y), ImageColor);
					DrawList->PrimWriteVtx(ImVec2(RightMarginX, TopMarginY), ImVec2(RightMarginU, TopMarginV), ImageColor);
					DrawList->PrimWriteVtx(ImVec2(EndPos.x, Position.y), ImVec2(EndUV.x, StartUV.y), ImageColor);
					DrawList->PrimWriteVtx(ImVec2(EndPos.x, TopMarginY), ImVec2(EndUV.x, TopMarginV), ImageColor);

					DrawList->PrimWriteVtx(ImVec2(Position.x, TopMarginY), ImVec2(StartUV.x, StartUV.y * LeftTiling), ImageColor);
					DrawList->PrimWriteVtx(ImVec2(Position.x, BottomMarginY), ImVec2(StartUV.x, EndUV.y * LeftTiling), ImageColor);
					DrawList->PrimWriteVtx(ImVec2(LeftMarginX, TopMarginY), ImVec2(LeftMarginU, StartUV.y * LeftTiling), ImageColor);
					DrawList->PrimWriteVtx(ImVec2(LeftMarginX, BottomMarginY), ImVec2(LeftMarginU, EndUV.y * LeftTiling), ImageColor);

					DrawList->PrimWriteVtx(ImVec2(RightMarginX, TopMarginY), ImVec2(RightMarginU, StartUV.y * RightTiling), ImageColor);
					DrawList->PrimWriteVtx(ImVec2(RightMarginX, BottomMarginY), ImVec2(RightMarginU, EndUV.y * RightTiling), ImageColor);
					DrawList->PrimWriteVtx(ImVec2(EndPos.x, TopMarginY), ImVec2(EndUV.x, StartUV.y * RightTiling), ImageColor);
					DrawList->PrimWriteVtx(ImVec2(EndPos.x, BottomMarginY), ImVec2(EndUV.x, EndUV.y * RightTiling), ImageColor);

					DrawList->PrimWriteVtx(ImVec2(Position.x, BottomMarginY), ImVec2(StartUV.x, BottomMarginV), ImageColor);
					DrawList->PrimWriteVtx(ImVec2(Position.x, EndPos.y), ImVec2(StartUV.x, EndUV.y), ImageColor);
					DrawList->PrimWriteVtx(ImVec2(LeftMarginX, BottomMarginY), ImVec2(LeftMarginU, BottomMarginV), ImageColor);
					DrawList->PrimWriteVtx(ImVec2(LeftMarginX, EndPos.y), ImVec2(LeftMarginU, EndUV.y), ImageColor);

					DrawList->PrimWriteVtx(ImVec2(LeftMarginX, BottomMarginY), ImVec2(StartUV.x * BottomTiling, BottomMarginV), ImageColor);
					DrawList->PrimWriteVtx(ImVec2(LeftMarginX, EndPos.y), ImVec2(StartUV.x * BottomTiling, EndUV.y), ImageColor);
					DrawList->PrimWriteVtx(ImVec2(RightMarginX, BottomMarginY), ImVec2(EndUV.x * BottomTiling, BottomMarginV), ImageColor);
					DrawList->PrimWriteVtx(ImVec2(RightMarginX, EndPos.y), ImVec2(EndUV.x * BottomTiling, EndUV.y), ImageColor);

					DrawList->PrimWriteVtx(ImVec2(RightMarginX, BottomMarginY), ImVec2(RightMarginU, BottomMarginV), ImageColor);
					DrawList->PrimWriteVtx(ImVec2(RightMarginX, EndPos.y), ImVec2(RightMarginU, EndUV.y), ImageColor);
					DrawList->PrimWriteVtx(ImVec2(EndPos.x, BottomMarginY), ImVec2(EndUV.x, BottomMarginV), ImageColor);
					DrawList->PrimWriteVtx(ImVec2(EndPos.x, EndPos.y), EndUV, ImageColor);

					DrawList->PopTexture();
				}
				else
				{
					DrawList->AddRect(RectMin, RectMax, ImageColor);
				}

			}break;
			}
		}

		if (Brush.GetDrawType() == ESlateBrushDrawType::RoundedBox && Brush.OutlineSettings.Width > 0.f)
		{
			ImU32 Color = BorderColor.IsSet() ? BorderColor.GetValue() : ImGui::GetColorU32(GetSlateColorU32(Brush.OutlineSettings.Color, TickContext));
			if (bUniformRounding)
			{
				DrawList->AddRect(RectMin, RectMax, Color, Rounding.x, Brush.OutlineSettings.Width);
			}
			else
			{
				PathRect(*DrawList, RectMin, RectMax, Rounding);
				DrawList->PathStroke(Color, Brush.OutlineSettings.Width, ImDrawFlags_Closed|ImDrawFlags_JoinMiter|ImDrawFlags_StrokeInside);
			}
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

	// used for aligning/stretching widgets vertically
	// defaulting to VAlign_Center for now
	float GetCurrentLineHeight(const FImGuiTickContext* TickContext)
	{
		float LineHeight = TickContext->ImguiContext->CurrentWindow->DC.CurrLineSize.y;
		if (LineHeight < KINDA_SMALL_NUMBER)
		{
			LineHeight = ImGui::GetFrameHeight();
		}
		return LineHeight;
	}

	float GetWidth(const FMargin& Margin) { return Margin.Left + Margin.Right; }
	float GetHeight(const FMargin& Margin) { return Margin.Top + Margin.Bottom; }

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
			(ContentPadding.x + ContentPadding.z) + ImMax(GetWidth(ButtonStyle.NormalPadding), GetWidth(ButtonStyle.PressedPadding)),
			(ContentPadding.y + ContentPadding.w) + ImMax(GetHeight(ButtonStyle.NormalPadding), GetHeight(ButtonStyle.PressedPadding)));
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
			(ContentPadding.x + ContentPadding.z) + ImMax(GetWidth(ButtonStyle.NormalPadding), GetWidth(ButtonStyle.PressedPadding)),
			(ContentPadding.y + ContentPadding.w) + ImMax(GetHeight(ButtonStyle.NormalPadding), GetHeight(ButtonStyle.PressedPadding)));
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
				ImMax(CheckBoxStyle.BackgroundImage.GetImageSize().X, CheckedBrush.GetImageSize().X),
				ImMax(CheckBoxStyle.BackgroundImage.GetImageSize().Y, CheckedBrush.GetImageSize().Y));
			ImageSize = ImTrunc(ImageSize * ImGui::GetStyle().FontScaleMain);
		}
		ImVec2 ImageSizeWithPadding{};
		if (bHasImage)
		{
			ImageSizeWithPadding = ImageSize + (bHasContent ? ImVec2(GetWidth(CheckBoxStyle.Padding), GetHeight(CheckBoxStyle.Padding)) : ImVec2(0.f, 0.f));
		}

		const ImVec2 ContentSize = ImVec2(LabelSize.x + ImageSizeWithPadding.x, ImMax(LabelSize.y, ImageSizeWithPadding.y));
		const FMargin ContentPadding = !bHasImage ? CheckBoxStyle.Padding : FMargin(0.f);
		ImVec2 ItemRect = ContentSize + ImVec2(GetWidth(ContentPadding), GetHeight(ContentPadding));
		if (CheckBoxStyle.CheckBoxType == ESlateCheckBoxType::ToggleButton)
		{
			ApplyItemWidthOverride(ItemRect.x);
		}

		float ButtonOffsetY = 0.f;
		if (CheckBoxStyle.CheckBoxType == ESlateCheckBoxType::CheckBox)
		{
			float CurrLineHeight = GetCurrentLineHeight(TickContext);
			if (ItemRect.y < CurrLineHeight)
			{
				ButtonOffsetY = ImFloor((CurrLineHeight - ItemRect.y) * 0.5f);
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
		ImVec2 ItemRect = ContentSize + ImVec2(GetWidth(ContentPadding), GetHeight(ContentPadding));
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
		const ImVec2 DownImageSizeWithPadding = DownImageSize + ImVec2(GetWidth(ComboButtonStyle.DownArrowPadding), GetHeight(ComboButtonStyle.DownArrowPadding));
		const ImVec2 ContentSize = ImVec2(PreviewTextSize.x + DownImageSizeWithPadding.x, ImMax(PreviewTextSize.y, DownImageSizeWithPadding.y));
		const ImVec2 ContentPadding = ImVec2(
			GetWidth(ComboButtonStyle.ContentPadding) + ImMax(GetWidth(ButtonStyle.NormalPadding), GetWidth(ButtonStyle.PressedPadding)),
			GetHeight(ComboButtonStyle.ContentPadding) + ImMax(GetHeight(ButtonStyle.NormalPadding), GetHeight(ButtonStyle.PressedPadding)));
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
		const ImVec2 DownImageSizeWithPadding = DownImageSize + ImVec2(GetWidth(ComboButtonStyle.DownArrowPadding), GetHeight(ComboButtonStyle.DownArrowPadding));
		const ImVec2 ContentSize = ImVec2(Image.Size.x + DownImageSizeWithPadding.x, ImMax(Image.Size.y, DownImageSizeWithPadding.y));

		ImVec2 ItemRect = ContentSize + ImVec2(
			GetWidth(ComboButtonStyle.ContentPadding) + ImMax(GetWidth(ButtonStyle.NormalPadding), GetWidth(ButtonStyle.PressedPadding)),
			GetHeight(ComboButtonStyle.ContentPadding) + ImMax(GetHeight(ButtonStyle.NormalPadding), GetHeight(ButtonStyle.PressedPadding)));

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

	void TextBlock(FImGuiTickContext* TickContext, const char* Text, const char* TextEnd)
	{
		FImGuiTickScope Scope{ TickContext };

		float CurrLineHeight = GetCurrentLineHeight(TickContext);
		ImVec2 FramePadding = ImGui::GetStyle().FramePadding;
		if (CurrLineHeight != ImGui::GetFrameHeight())
		{
			FramePadding.y = ImMax(0.f, FramePadding.y + (CurrLineHeight - ImGui::GetFrameHeight()) * 0.5f);
		}

		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, FramePadding);
		ImGui::AlignTextToFramePadding();
		ImGui::TextUnformatted(Text, TextEnd);
		ImGui::PopStyleVar();
	}

	static void PushInputTextStyling(FImGuiTickContext* TickContext)
	{
		float CurrLineHeight = GetCurrentLineHeight(TickContext);
		float FramePaddingY = ImGui::GetStyle().FramePadding.y;
		if (CurrLineHeight != ImGui::GetFrameHeight())
		{
			FramePaddingY = ImMax(0.f, FramePaddingY + (CurrLineHeight - ImGui::GetFrameHeight()) * 0.5f);
		}

		ImGui::PushStyleColor(ImGuiCol_NavCursor, 0);
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8.f, FramePaddingY));
	}
	static void PopInputTextStyling(FImGuiTickContext* TickContext)
	{
		ImGui::PopStyleVar();
		ImGui::PopStyleColor();

		const ImRect& NavRect = TickContext->ImguiContext->LastItemData.NavRect;
		if (ImGui::IsItemActive() || IsItemHighlighted())
		{
			ImGui::GetWindowDrawList()->AddRect(NavRect.Min, NavRect.Max, ImGui::GetColorU32(ImGuiCol_NavCursor), ImGui::GetStyle().FrameRounding);
		}
		else
		{
			ImGui::GetWindowDrawList()->AddRect(NavRect.Min, NavRect.Max, ImGui::GetColorU32(GetSlateColorVec4(TickContext, ImGui::IsItemHovered() ? EStyleColor::Hover : EStyleColor::InputOutline)), ImGui::GetStyle().FrameRounding);
		}
	}

	bool EditableTextBox(FImGuiTickContext* TickContext, const char* Label, const char* Hint, char* Buffer, size_t BufferSize, ImGuiInputTextFlags Flags, ImGuiInputTextCallback Callback, void* UserData)
	{
		FImGuiTickScope Scope{ TickContext };

		PushInputTextStyling(TickContext);
		bool bTextChanged = Hint ? ImGui::InputTextWithHint(Label, Hint, Buffer, BufferSize, Flags, Callback, UserData) : ImGui::InputText(Label, Buffer, BufferSize, Flags, Callback, UserData);
		PopInputTextStyling(TickContext);

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
		float CurrLineHeight = GetCurrentLineHeight(TickContext);

		ImGui::BeginGroup();
		{
			static const char* InputTextLabel = "##Filter";
			const ImGuiID InputTextId = ImGui::GetID(InputTextLabel);

			float FramePaddingY = ImGui::GetStyle().FramePadding.y;
			if (CurrLineHeight != ImGui::GetFrameHeight())
			{
				FramePaddingY = ImMax(0.f, FramePaddingY + (CurrLineHeight - ImGui::GetFrameHeight()) * 0.5f);
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
			ImRect InputTextRect{ ImGui::GetCursorScreenPos(), ImGui::GetCursorScreenPos() + ImVec2(ItemWidth, CurrLineHeight) };
			if (SearchBoxStyle.TextBoxStyle.BackgroundImageNormal.OutlineSettings.RoundingType == ESlateBrushRoundingType::HalfHeightRadius)
			{
				InputTextRect.Max.x -= CurrLineHeight * 0.5f;
			}
			else
			{
				InputTextRect.Max.x -= SearchBoxStyle.TextBoxStyle.BackgroundImageNormal.OutlineSettings.CornerRadii.X;
			}
			InputTextRect.Min.x += SearchBoxStyle.ImagePadding.Right + SearchBoxStyle.TextBoxStyle.Padding.Left + ImageSize.X + (SearchBoxStyle.ImagePadding.Left + GetWidth(SearchBoxStyle.ImagePadding.Left) * 0.5f);

			const bool bActive = ImGui::GetActiveID() == InputTextId;
			const bool bInputTextHovered = ((GImGui->CurrentItemFlags & ImGuiItemFlags_Disabled) == 0) && ImGui::IsMouseHoveringRect(InputTextRect.Min, InputTextRect.Max);
			const FSlateBrush* BackgroundBrush = bActive ? &SearchBoxStyle.TextBoxStyle.BackgroundImageFocused : (bInputTextHovered ? &SearchBoxStyle.TextBoxStyle.BackgroundImageHovered : &SearchBoxStyle.TextBoxStyle.BackgroundImageNormal);
			DrawSlateBrush(TickContext, *BackgroundBrush, ImGui::GetCursorScreenPos(), ImGui::GetCursorScreenPos() + ImVec2(ItemWidth, CurrLineHeight), {}, IsItemHighlighted(InputTextId) ? ImGui::GetColorU32(ImGuiCol_NavCursor) : TOptional<ImU32>{});

			uint32 FocusedForegroundColor = GetSlateColorU32(SearchBoxStyle.TextBoxStyle.FocusedForegroundColor, TickContext);
			uint32 NormalForegroundColor = GetSlateColorU32(SearchBoxStyle.TextBoxStyle.ForegroundColor, TickContext);

			bool bClearText = false;
			{
				FSlateColor ImageTint = SearchBoxStyle.TextBoxStyle.ForegroundColor;

				// HAlign_Center
				ImGui::SetCursorPosX(ImGui::GetCursorPosX() + SearchBoxStyle.ImagePadding.Left + GetWidth(SearchBoxStyle.ImagePadding) * 0.5f);
				if (FCStringAnsi::Strlen(Buffer) > 0)
				{
					ImGui::SetCursorPosY(ImGui::GetCursorPosY() + CurrLineHeight * 0.5f - ImageSize.Y * 0.5f);
					bClearText = ImGui::InvisibleButton("Clear", ImVec2(ImageSize.X, ImageSize.Y));

					if (((GImGui->CurrentItemFlags & ImGuiItemFlags_Disabled) == 0) && ImGui::IsMouseHoveringRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax()))
					{
						ImageTint = SearchBoxStyle.TextBoxStyle.FocusedForegroundColor;
					}
				}
				else
				{
					ImGui::SetCursorPosY(ImGui::GetCursorPosY() + CurrLineHeight * 0.5f - ImageSize.Y * 0.5f);
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
				ImGui::SetNextItemWidth(ItemWidth - CurrLineHeight - 16.f);
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

	static bool TempInputIsClampEnabled(ImGuiSliderFlags flags, ImGuiDataType data_type, const void* p_min, const void* p_max)
	{
		if ((flags & ImGuiSliderFlags_ClampOnInput) && (p_min != NULL || p_max != NULL))
		{
			const int32 clamp_range_dir = (p_min != NULL && p_max != NULL) ? ImGui::DataTypeCompare(data_type, p_min, p_max) : 0; // -1 when *p_min < *p_max, == 0 when *p_min == *p_max
			if (p_min == NULL || p_max == NULL || clamp_range_dir < 0)
				return true;
			if (clamp_range_dir == 0)
				return ImGui::DataTypeIsZero(data_type, p_min) ? ((flags & ImGuiSliderFlags_ClampZeroRange) != 0) : true;
		}
		return false;
	}
	template <typename T>
	bool SpinBoxT(FImGuiTickContext* TickContext, const char* label, ImGuiDataType data_type, T* p_data, const FSpinBoxStyle& SpinBoxStyle, const T v_speed, const T* p_min, const T* p_max, const char* format, ImGuiSliderFlags flags = ImGuiSliderFlags_None)
	{
		FImGuiTickScope Scope{ TickContext };
		FImGuiNamedScope WidgetScope{ label };

		ImGuiContext& g = *GImGui;
		ImGuiWindow* window = ImGui::GetCurrentWindow();
		if (window->SkipItems)
			return false;

		const ImGuiStyle& style = g.Style;
		FMargin ContentPadding = FMargin(style.FramePadding.x, style.FramePadding.y, style.FramePadding.x, style.FramePadding.y);

		ImRect frame_bb{};
		bool hovered = false;
		bool value_changed = false;
		{
			ImVec2 FramePadding = ImVec2(GetWidth(ContentPadding) + GetWidth(SpinBoxStyle.TextPadding), GetHeight(ContentPadding) + GetHeight(SpinBoxStyle.TextPadding)) * 0.5f;
			FImGuiScopedStyleEdit StyleEdits
			{
				ImGuiStyleVar_FramePadding, FramePadding,
				ImGuiCol_NavCursor, 0,
			};

			const ImGuiID id = window->GetID("##SpinBox");
			const float w = ImGui::CalcItemWidth();
			const float label_size_y = ImGui::CalcTextSize("").y;
			frame_bb = ImRect(window->DC.CursorPos, window->DC.CursorPos + ImVec2(w, label_size_y + style.FramePadding.y * 2.0f));

			const bool temp_input_allowed = (flags & ImGuiSliderFlags_NoInput) == 0;
			ImGui::ItemSize(frame_bb, style.FramePadding.y);
			if (!ImGui::ItemAdd(frame_bb, id, &frame_bb, temp_input_allowed ? ImGuiItemFlags_Inputable : 0))
				return false;

			if (format == NULL)
				format = ImGui::DataTypeGetInfo(data_type)->PrintFmt;

			hovered = ImGui::ItemHoverable(frame_bb, id, g.LastItemData.ItemFlags);
			bool temp_input_is_active = temp_input_allowed && ImGui::TempInputIsActive(id);
			if (!temp_input_is_active)
			{
				const bool clicked = hovered && ImGui::IsMouseClicked(0, ImGuiInputFlags_None, id);
				const bool make_active = (clicked || g.NavActivateId == id);
				if (make_active && clicked)
					ImGui::SetKeyOwner(ImGuiKey_MouseLeft, id);
				if (make_active && temp_input_allowed)
					if (g.NavActivateId == id && (g.NavActivateFlags & ImGuiActivateFlags_PreferInput))
						temp_input_is_active = true;

				if (temp_input_allowed && !temp_input_is_active)
					if (g.ActiveId == id && hovered && g.IO.MouseReleased[0] && !ImGui::IsMouseDragPastThreshold(0, g.IO.MouseDragThreshold * /*DRAG_MOUSE_THRESHOLD_FACTOR*/0.5f))
					{
						g.NavActivateId = id;
						g.NavActivateFlags = ImGuiActivateFlags_PreferInput;
						temp_input_is_active = true;
					}

				// Store initial value (not used by main lib but available as a convenience but some mods e.g. to revert)
				if (make_active)
					memcpy(&g.ActiveIdValueOnActivation, p_data, ImGui::DataTypeGetInfo(data_type)->Size);

				if (make_active && !temp_input_is_active)
				{
					ImGui::SetActiveID(id, window);
					ImGui::SetFocusID(id, window);
					ImGui::FocusWindow(window);
					g.ActiveIdUsingNavDirMask = (1 << ImGuiDir_Left) | (1 << ImGuiDir_Right);
				}
			}

			if (temp_input_is_active)
			{
				const FSlateBrush* BackgroundBrush = &SpinBoxStyle.ActiveBackgroundBrush;
				if (!BackgroundBrush->IsSet())
				{
					BackgroundBrush = &SpinBoxStyle.HoveredBackgroundBrush;
				}
				DrawSlateBrush(TickContext, *BackgroundBrush, frame_bb.Min, frame_bb.Max);

				FImGuiScopedStyleEdit TempTextInputStyle
				{
					ImGuiStyleVar_FramePadding, ImVec2(),
					ImGuiStyleVar_FrameBorderSize, 0.f,

					ImGuiCol_FrameBg, 0,
					ImGuiCol_FrameBgActive, 0,
					ImGuiCol_FrameBgHovered, 0,
				};

				ImVec2 rect_min = frame_bb.Min + ImVec2(ContentPadding.Left + SpinBoxStyle.TextPadding.Left, ContentPadding.Top + SpinBoxStyle.TextPadding.Top);
				ImVec2 rect_max = frame_bb.Max - ImVec2(ContentPadding.Right + SpinBoxStyle.TextPadding.Right, ContentPadding.Bottom + SpinBoxStyle.TextPadding.Bottom);
				const bool clamp_enabled = TempInputIsClampEnabled(flags, data_type, p_min, p_max);

				const ImVec2 Backup_CursorPosPrevLine = window->DC.CursorPosPrevLine;
				const ImVec2 Backup_PrevLineSize = window->DC.PrevLineSize;
				const float Backup_PrevLineTextBaseOffset = window->DC.PrevLineTextBaseOffset;
				value_changed = ImGui::TempInputScalar({ rect_min, rect_max }, id, "##SpinBox", data_type, p_data, format, clamp_enabled ? p_min : NULL, clamp_enabled ? p_max : NULL);
				// restore prev line data as the input widget doesn't span the entire frame rect
				window->DC.CursorPosPrevLine = Backup_CursorPosPrevLine;
				window->DC.PrevLineSize = Backup_PrevLineSize;
				window->DC.PrevLineTextBaseOffset = Backup_PrevLineTextBaseOffset;

				ImGui::SetItemKeyOwner(ImGuiKey_MouseWheelY);

				if (ImAbs(ImGui::GetIO().MouseWheel) > KINDA_SMALL_NUMBER && ImGui::IsMouseHoveringRect(rect_min, rect_max))
				{
					T new_value = (ImGui::GetIO().MouseWheel > 0.f ? (*p_data + v_speed) : (*p_data - v_speed));
					if (clamp_enabled)
					{
						new_value = ImClamp(new_value, *p_min, *p_max);
					}

					value_changed = (new_value != *p_data);
					if (value_changed)
					{
						*p_data = new_value;
						ImGui::MarkItemEdited(id);

						if (ImGuiInputTextState* state = ImGui::GetInputTextState(ImGui::GetItemID()))
						{
							state->ReloadUserBufAndMoveToEnd();
						}
					}
				}

				return value_changed;
			}

			// Drag behavior
			value_changed = ImGui::DragBehavior(id, data_type, p_data, v_speed, p_min, p_max, format, flags);
			if (value_changed)
				ImGui::MarkItemEdited(id);
		}

		const bool bActive = ImGui::IsItemActive();

		// draw frame
		const FSlateBrush* BackgroundBrush = bActive ? &SpinBoxStyle.ActiveBackgroundBrush : (hovered ? &SpinBoxStyle.HoveredBackgroundBrush : &SpinBoxStyle.BackgroundBrush);
		if (bActive && !BackgroundBrush->IsSet())
		{
			BackgroundBrush = &SpinBoxStyle.HoveredBackgroundBrush;
		}
		DrawSlateBrush(TickContext, *BackgroundBrush, frame_bb.Min, frame_bb.Max, {}, IsItemHighlighted() ? ImGui::GetColorU32(ImGuiCol_NavCursor) : TOptional<ImU32>{});

		// draw fill
		if (p_min && p_max && (*p_min != *p_max))
		{
			const FSlateBrush* FillBrush = bActive ? &SpinBoxStyle.ActiveFillBrush : (hovered ? &SpinBoxStyle.HoveredFillBrush : &SpinBoxStyle.InactiveFillBrush);
			if (hovered && !FillBrush->IsSet())
			{
				FillBrush = &SpinBoxStyle.ActiveFillBrush;
			}
			float fill_x = ImLerp(frame_bb.Min.x, frame_bb.Max.x, 1.f - ImClamp(float((double)(*p_max - *p_data) / (double)(*p_max - *p_min)), 0.f, 1.f));

			ImVec2 fill_min = frame_bb.Min + ImVec2(SpinBoxStyle.InsetPadding.Left, SpinBoxStyle.InsetPadding.Top) * 2.f;
			ImVec2 fill_max = ImVec2(fill_x, frame_bb.Max.y) - ImVec2(SpinBoxStyle.InsetPadding.Right, SpinBoxStyle.InsetPadding.Bottom) * 2.f;
			DrawSlateBrush(TickContext, *FillBrush, fill_min, fill_max);
		}

		// value as text
		char value_buf[64];
		const char* value_buf_end = value_buf + ImGui::DataTypeFormatString(value_buf, IM_COUNTOF(value_buf), data_type, p_data, format);
		ImGui::RenderTextClipped(
			frame_bb.Min + ImVec2(ContentPadding.Left + SpinBoxStyle.TextPadding.Left, ContentPadding.Top + SpinBoxStyle.TextPadding.Top),
			frame_bb.Max - ImVec2(ContentPadding.Right + SpinBoxStyle.TextPadding.Right, ContentPadding.Bottom + SpinBoxStyle.TextPadding.Bottom),
			value_buf, value_buf_end, NULL, ImVec2(0.f, 0.5f));

		// update mouse cursor
		if ((bActive && ImGui::IsMouseDragging(0, 1.f)) || hovered)
		{
			ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
		}

		return value_changed;
	}
	bool SpinBox(FImGuiTickContext* TickContext, const char* Label, int8& v, const FSpinBoxStyle& SpinBoxStyle, int8 v_speed, int8 v_min, int8 v_max, const char* format, ImGuiSliderFlags flags)
	{
		return SpinBoxT(TickContext, Label, ImGuiDataType_S8, &v, SpinBoxStyle, v_speed, &v_min, &v_max, format, flags);
	}
	bool SpinBox(FImGuiTickContext* TickContext, const char* Label, int16& v, const FSpinBoxStyle& SpinBoxStyle, int16 v_speed, int16 v_min, int16 v_max, const char* format, ImGuiSliderFlags flags)
	{
		return SpinBoxT(TickContext, Label, ImGuiDataType_S16, &v, SpinBoxStyle, v_speed, &v_min, &v_max, format, flags);
	}
	bool SpinBox(FImGuiTickContext* TickContext, const char* Label, int32& v, const FSpinBoxStyle& SpinBoxStyle, int32 v_speed, int32 v_min, int32 v_max, const char* format, ImGuiSliderFlags flags)
	{
		return SpinBoxT(TickContext, Label, ImGuiDataType_S32, &v, SpinBoxStyle, v_speed, &v_min, &v_max, format, flags);
	}
	bool SpinBox(FImGuiTickContext* TickContext, const char* Label, uint8& v, const FSpinBoxStyle& SpinBoxStyle, uint8 v_speed, uint8 v_min, uint8 v_max, const char* format, ImGuiSliderFlags flags)
	{
		return SpinBoxT(TickContext, Label, ImGuiDataType_U8, &v, SpinBoxStyle, v_speed, &v_min, &v_max, format, flags);
	}
	bool SpinBox(FImGuiTickContext* TickContext, const char* Label, uint16& v, const FSpinBoxStyle& SpinBoxStyle, uint16 v_speed, uint16 v_min, uint16 v_max, const char* format, ImGuiSliderFlags flags)
	{
		return SpinBoxT(TickContext, Label, ImGuiDataType_U16, &v, SpinBoxStyle, v_speed, &v_min, &v_max, format, flags);
	}
	bool SpinBox(FImGuiTickContext* TickContext, const char* Label, uint32& v, const FSpinBoxStyle& SpinBoxStyle, uint32 v_speed, uint32 v_min, uint32 v_max, const char* format, ImGuiSliderFlags flags)
	{
		return SpinBoxT(TickContext, Label, ImGuiDataType_U32, &v, SpinBoxStyle, v_speed, &v_min, &v_max, format, flags);
	}
	bool SpinBox(FImGuiTickContext* TickContext, const char* Label, float& v, const FSpinBoxStyle& SpinBoxStyle, float v_speed, float v_min, float v_max, const char* format, ImGuiSliderFlags flags)
	{
		return SpinBoxT(TickContext, Label, ImGuiDataType_Float, &v, SpinBoxStyle, v_speed, &v_min, &v_max, format, flags);
	}
	bool SpinBox(FImGuiTickContext* TickContext, const char* Label, double& v, const FSpinBoxStyle& SpinBoxStyle, double v_speed, double v_min, double v_max, const char* format, ImGuiSliderFlags flags)
	{
		return SpinBoxT(TickContext, Label, ImGuiDataType_Double, &v, SpinBoxStyle, v_speed, &v_min, &v_max, format, flags);
	}

	bool InputFloat(FImGuiTickContext* TickContext, const char* label, float* v, const char* format, ImGuiInputTextFlags flags)
	{
		PushInputTextStyling(TickContext);
		bool bValueChanged = ImGui::InputScalar(label, ImGuiDataType_Float, (void*)v, /*step=*/nullptr, /*step_fast=*/nullptr, format, flags);
		PopInputTextStyling(TickContext);
		return bValueChanged;
	}
	bool InputInt(FImGuiTickContext* TickContext, const char* label, int32* v, ImGuiInputTextFlags flags)
	{
		PushInputTextStyling(TickContext);
		// Hexadecimal input provided as a convenience but the flag name is awkward. Typically you'd use InputText() to parse your own data, if you want to handle prefixes.
		const char* format = (flags & ImGuiInputTextFlags_CharsHexadecimal) ? "%08X" : "%d";
		bool bValueChanged = ImGui::InputScalar(label, ImGuiDataType_S32, (void*)v, /*step=*/nullptr, /*step_fast=*/nullptr, format, flags);
		PopInputTextStyling(TickContext);
		return bValueChanged;
	}
	bool InputDouble(FImGuiTickContext* TickContext, const char* label, double* v, const char* format, ImGuiInputTextFlags flags)
	{
		PushInputTextStyling(TickContext);
		bool bValueChanged = ImGui::InputScalar(label, ImGuiDataType_Double, (void*)v, /*step=*/nullptr, /*step_fast=*/nullptr, format, flags);
		PopInputTextStyling(TickContext);
		return bValueChanged;
	}

	void PushComboListViewDropdownStyle(FImGuiTickContext* TickContext, bool bNormalDropdown, bool bDrawBorder)
	{
		ImVec4 BackgroundColor = FImGui::GetSlateColorVec4(TickContext, bNormalDropdown ? EStyleColor::Dropdown : EStyleColor::Recessed) * ImVec4(1.f, 1.f, 1.f, 0.94f);
		ImVec4 WindowBorderColor = FImGui::GetSlateColorVec4(TickContext, bNormalDropdown ? EStyleColor::DropdownOutline : EStyleColor::InputOutline) * ImVec4(1.f, 1.f, 1.f, 0.94f);

		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(1.f, 1.f));
		ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.f);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.f, 0.f));
		ImGui::PushStyleVar(ImGuiStyleVar_PopupBorderSize, bDrawBorder ? 1.f : 0.f);
		ImGui::PushStyleColor(ImGuiCol_FrameBg, BackgroundColor);
		ImGui::PushStyleColor(ImGuiCol_PopupBg, BackgroundColor);
		ImGui::PushStyleColor(ImGuiCol_Border, WindowBorderColor);
	}
	void PopComboListViewDropdownStyle(FImGuiTickContext* TickContext)
	{
		ImGui::PopStyleColor(3);
		ImGui::PopStyleVar(4);
	}
	void BeginComboListViewDropdownBody(FImGuiTickContext* TickContext)
	{
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2());
		ImGui::PushStyleColor(ImGuiCol_Header, FImGui::GetSlateColorVec4(TickContext, EStyleColor::SelectInactive));
		ImGui::PushStyleColor(ImGuiCol_HeaderActive, FImGui::GetSlateColorVec4(TickContext, EStyleColor::Primary));
		ImGui::PushStyleColor(ImGuiCol_HeaderHovered, FImGui::GetSlateColorVec4(TickContext, EStyleColor::Primary));
		ImGui::PushStyleColor(ImGuiCol_Border, FImGui::GetSlateColorVec4(TickContext, EStyleColor::InputOutline));

		// account for selectable half space padding
		ImGui::SetCursorPosY(ImGui::GetCursorPosY() + ImGui::GetStyle().ItemSpacing.y);
	}
	void EndComboListViewDropdownBody(FImGuiTickContext* TickContext)
	{
		ImGui::PopStyleVar();
		ImGui::PopStyleColor(4);

		// account for selectable half space padding
		ImGui::Dummy(ImVec2(0.f, 1.f));
	}
}
