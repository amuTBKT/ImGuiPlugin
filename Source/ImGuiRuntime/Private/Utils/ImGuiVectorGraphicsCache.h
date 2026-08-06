#pragma once

#include "ImGuiPluginTypes.h"
#include "Styling/SlateBrush.h"

THIRD_PARTY_INCLUDES_START
#include <nanosvg.h>
#include <nanosvgrast.h>
THIRD_PARTY_INCLUDES_END

#include "Misc/FileHelper.h"

namespace ImGuiUtils
{
	static constexpr int32 ImageCountBudget = 32;
	static constexpr int32 ImageMaxUnusedFrameCount = 240;

	class FImGuiVectorGraphicsCache
	{
		struct FVectorCacheKey
		{
			int32			TexID;
			FName			BrushName;
			FIntPoint		PixelSize;

			FVectorCacheKey(int32 InTexID, FName InBrushName, FVector2f LocalSize, float DrawScale)
				: TexID(InTexID)
				, BrushName(InBrushName)
				, PixelSize((LocalSize* DrawScale).IntPoint())
			{
				KeyHash = HashCombine(GetTypeHash(TexID), HashCombine(GetTypeHash(BrushName), GetTypeHash(PixelSize)));
			}

			bool operator==(const FVectorCacheKey& Other) const
			{
				return KeyHash == Other.KeyHash;
			}

			friend inline uint32 GetTypeHash(const FVectorCacheKey& Key)
			{
				return Key.KeyHash;
			}

		private:
			uint32 KeyHash;
		};

		struct FVectorCacheImage
		{
			ImFontAtlasRectId RectId = 0;
			int32 LastUsedFrameIndex = 0;
		};

		struct FRasterRequest
		{
			ImFontAtlasRectId RectId;
			FName BrushResourceName;
		};

	public:
		FImGuiVectorGraphicsCache(TSharedPtr<ImFontAtlas, ESPMode::NotThreadSafe> InFontAtlas)
			: FontAtlas(InFontAtlas)
		{
		}
		~FImGuiVectorGraphicsCache()
		{
			Clear();
		}

		void OnBeginFrame()
		{
			FontAtlasFrameCount = FontAtlas->Builder->FrameCount;

			ReleaseUnusedImages(/*bForceClearAll=*/false);
		}

		void ReleaseUnusedImages(bool bForceClearAll)
		{
			const bool bReleaseImages = bForceClearAll || (CachedImages.Num() > ImageCountBudget);
			if (!bReleaseImages)
			{
				return;
			}

			for (auto It = CachedImages.CreateIterator(); It; ++It)
			{
				if (bForceClearAll || (It->Value.LastUsedFrameIndex < (FontAtlasFrameCount - ImageMaxUnusedFrameCount)))
				{
					FontAtlas->RemoveCustomRect(It->Value.RectId);
					It.RemoveCurrent();
				}
			}
		}

		// we cannot tell if ImFontAltas was cleared, so call this before clearing ImFontAtlas
		// otherwise we'll end up with invalid rect ids
		void Clear()
		{
			ReleaseUnusedImages(/*bForceClearAll=*/true);
		}

		void RasterizeImages()
		{
			DECLARE_SCOPE_CYCLE_COUNTER(TEXT("SVG RasterizeImages"), STAT_ImGui_RasterizeSVG, STATGROUP_ImGui);

			for (const auto& Request : RasterRequests)
			{
				ImFontAtlasRect AtlasRect;
				if (!FontAtlas->GetCustomRect(Request.RectId, &AtlasRect))
				{
					continue;
				}

				FString SVGString;
				if (FFileHelper::LoadFileToString(SVGString, *Request.BrushResourceName.ToString()))
				{
					// TODO: can probably cache `NSVGimage`
					NSVGimage* Image = nsvgParse(TCHAR_TO_ANSI(*SVGString), "px", 96.f);
					if (Image)
					{
						NSVGrasterizer* Rasterizer = nsvgCreateRasterizer();

						const float SVGScaleX = (float)AtlasRect.w / Image->width;
						const float SVGScaleY = (float)AtlasRect.h / Image->height;

						const int32 Stride = FontAtlas->TexData->Width * 4;
						nsvgRasterizeFull(Rasterizer, Image, 0, 0, SVGScaleX, SVGScaleY, (uint8*)FontAtlas->TexData->GetPixelsAt(AtlasRect.x, AtlasRect.y), AtlasRect.w, AtlasRect.w, Stride);

						nsvgDeleteRasterizer(Rasterizer);

						nsvgDelete(Image);
					}
				}
			}
			RasterRequests.Reset();
		}

		FImGuiImageBindingParams GetOrLoadBrush(const FSlateBrush& Brush, FVector2f LocalSize, float DrawScale)
		{
			DECLARE_SCOPE_CYCLE_COUNTER(TEXT("SVG GetResourceHandle"), STAT_ImGui_LookupSVG, STATGROUP_ImGui);

			if (!ensure(Brush.GetImageType() == ESlateBrushImageType::Vector))
			{
				return {};
			}

			ImFontAtlasRect AtlasRect;
			{
				FVectorCacheKey CacheKey(FontAtlas->TexData->UniqueID, Brush.GetResourceName(), LocalSize, DrawScale);
				FVectorCacheImage* CachedImage = CachedImages.Find(CacheKey);
				if (CachedImage)
				{
					if (FontAtlas->GetCustomRect(CachedImage->RectId, &AtlasRect))
					{
						CachedImage->LastUsedFrameIndex = FontAtlasFrameCount;
					}
					else
					{
						CachedImage = nullptr;
					}
				}

				if (!CachedImage)
				{
					ImFontAtlasRectId RectId = FontAtlas->AddCustomRect(CacheKey.PixelSize.X, CacheKey.PixelSize.Y, &AtlasRect);
					CachedImages.Add(MoveTemp(CacheKey), { RectId, FontAtlasFrameCount });

					RasterRequests.Emplace(RectId, Brush.GetResourceName());

					// TODO: Looks like this needs to happen immediately otherwise texture data is not uploaded to NetImGui server
					// ideally should be able to batch and rasterize in parallel (I doubt it can be moved to background threads)
					RasterizeImages();
				}
			}

			FImGuiImageBindingParams Params{};
			Params.Id = FontAtlas->TexRef;
			Params.Size = ImVec2(AtlasRect.w, AtlasRect.h);
			Params.UV0 = AtlasRect.uv0;
			Params.UV1 = AtlasRect.uv1;
			return Params;
		}

	private:
		TArray<FRasterRequest> RasterRequests;
		TMap<FVectorCacheKey, FVectorCacheImage> CachedImages;
		TSharedPtr<ImFontAtlas, ESPMode::NotThreadSafe> FontAtlas;
		int32 FontAtlasFrameCount = 0;
	};
}
