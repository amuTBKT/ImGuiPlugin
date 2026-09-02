#pragma once

#include "ImGuiPluginTypes.h"
#include "Misc/FileHelper.h"
#include "Styling/SlateBrush.h"

THIRD_PARTY_INCLUDES_START
#include <nanosvg.h>
#include <nanosvgrast.h>
THIRD_PARTY_INCLUDES_END

#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "Modules/ModuleManager.h"

namespace ImGuiUtils
{
	static constexpr int32 ImageCountBudget = 32;
	static constexpr int32 ImageMaxUnusedFrameCount = 240;

	class FImGuiImageCache
	{
		struct FImageKey
		{
			FImageKey(const FName& InBrushName, const FIntPoint& InSize)
				: BrushName(InBrushName)
				, PixelSize(InSize)
			{
				KeyHash = HashCombine(GetTypeHash(BrushName), GetTypeHash(PixelSize));
			}

			bool operator==(const FImageKey& Other) const
			{
				return BrushName == Other.BrushName && PixelSize == Other.PixelSize;
			}

			friend inline uint32 GetTypeHash(const FImageKey& Key)
			{
				return Key.KeyHash;
			}

		private:
			FName		BrushName;
			FIntPoint	PixelSize;
			uint32		KeyHash;
		};

		struct FCachedImage
		{
			ImFontAtlasRectId RectId = 0;
			int32 LastUsedFrameIndex = 0;
		};

	public:
		FImGuiImageCache(TSharedPtr<ImFontAtlas, ESPMode::NotThreadSafe> InFontAtlas)
			: FontAtlas(InFontAtlas)
		{
		}
		~FImGuiImageCache()
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

		FORCEINLINE static bool CanLoadBrush(const FSlateBrush& Brush)
		{
			return (Brush.GetImageType() != ESlateBrushImageType::NoImage) && !Brush.IsDynamicallyLoaded() && !::IsValid(Brush.GetResourceObject()) && !Brush.GetResourceName().IsNone();
		}

		FImGuiImageBindingParams GetOrLoadBrush(const FSlateBrush& Brush, FVector2f LocalSize, float DrawScale)
		{
			DECLARE_SCOPE_CYCLE_COUNTER(TEXT("ImageCache::GetResourceHandle"), STAT_ImGui_LookupImage, STATGROUP_ImGui);

			FVector2f DrawSize = LocalSize * DrawScale;
			ImFontAtlasRect AtlasRect;
			{
				// non vector image don't need scaling so cache at 1x1 (cannot determine the size without loading it first)
				const FIntPoint SizeForCaching = (Brush.GetImageType() == ESlateBrushImageType::Vector) ? DrawSize.IntPoint() : FIntPoint(1, 1);
				FImageKey CacheKey(Brush.GetResourceName(), SizeForCaching);
				FCachedImage* CachedImage = CachedImages.Find(CacheKey);
				if (CachedImage)
				{
					if (FontAtlas->GetCustomRect(CachedImage->RectId, &AtlasRect))
					{
						CachedImage->LastUsedFrameIndex = FontAtlasFrameCount;
					}
					else
					{
						ensureMsgf(false, TEXT("Invalid cached image! Did you clear ImFontAtlas without calling FImGuiImageCache::Clear first?"));
						CachedImage = nullptr;
					}
				}

				if (!CachedImage)
				{
					constexpr int32 BytesPerPixel = 4;

					ImFontAtlasRectId RectId = ImFontAtlasRectId_Invalid;
					if (Brush.GetImageType() == ESlateBrushImageType::Vector)
					{
						// based on `FSlateSVGRasterizer::RasterizeSVGFromFile`
						FString SVGString;
						if (FFileHelper::LoadFileToString(SVGString, *Brush.GetResourceName().ToString()))
						{
							// TODO: can probably cache `NSVGimage`
							NSVGimage* Image = nsvgParse(TCHAR_TO_ANSI(*SVGString), "px", 96.f);
							if (Image)
							{
								FIntPoint RasterSize = DrawSize.IntPoint();
								RectId = FontAtlas->AddCustomRect(RasterSize.X, RasterSize.Y, &AtlasRect);
								if (RectId != ImFontAtlasRectId_Invalid)
								{
									NSVGrasterizer* Rasterizer = nsvgCreateRasterizer();

									const float SVGScaleX = (float)AtlasRect.w / Image->width;
									const float SVGScaleY = (float)AtlasRect.h / Image->height;

									const int32 Stride = FontAtlas->TexData->Width * BytesPerPixel;
									nsvgRasterizeFull(Rasterizer, Image, 0, 0, SVGScaleX, SVGScaleY, (uint8*)FontAtlas->TexData->GetPixelsAt(AtlasRect.x, AtlasRect.y), AtlasRect.w, AtlasRect.w, Stride);

									nsvgDeleteRasterizer(Rasterizer);
								}
								nsvgDelete(Image);
							}
						}
					}
					else
					{
						// based on `FSlateRHIResourceManager::LoadTexture`
						TArray<uint8> RawFileData;
						if (FFileHelper::LoadFileToArray(RawFileData, *Brush.GetResourceName().ToString()))
						{
							IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>(FName("ImageWrapper"));

							EImageFormat ImageFormat = ImageWrapperModule.DetectImageFormat(RawFileData.GetData(), RawFileData.Num());
							if (ImageFormat == EImageFormat::Invalid)
							{
								ImageFormat = EImageFormat::PNG;
							}
							TSharedPtr<IImageWrapper> ImageWrapper = ImageWrapperModule.CreateImageWrapper(ImageFormat);

							if (ImageWrapper.IsValid() && ImageWrapper->SetCompressed(RawFileData.GetData(), RawFileData.Num()))
							{
								const int32 Width  = ImageWrapper->GetWidth();
								const int32 Height = ImageWrapper->GetHeight();
								const int32 Stride = Width * BytesPerPixel;

								TArray<uint8> DecodedImage;
								if (ImageWrapper->GetRaw(ERGBFormat::BGRA, 8, DecodedImage))
								{
									RectId = FontAtlas->AddCustomRect(Width, Height, &AtlasRect);
									if (RectId != ImFontAtlasRectId_Invalid)
									{
										for (int32 Y = 0; Y < Height; ++Y)
										{
											uint8* Dst = (uint8*)FontAtlas->TexData->GetPixelsAt(AtlasRect.x, AtlasRect.y + Y);
											const uint8* Src = &DecodedImage[Stride * Y];
											FMemory::Memcpy(Dst, Src, Stride);
										}
									}
								}
							}
						}
					}

					if (RectId != ImFontAtlasRectId_Invalid)
					{
						CachedImages.Add(MoveTemp(CacheKey), { RectId, FontAtlasFrameCount });
					}
				}
			}

			FImGuiImageBindingParams Params{};
			Params.Id = FontAtlas->TexRef;
			Params.Size = ImVec2(DrawSize.X, DrawSize.Y);
			Params.UV0 = AtlasRect.uv0;
			Params.UV1 = AtlasRect.uv1;
			return Params;
		}

	private:
		TMap<FImageKey, FCachedImage> CachedImages;
		TSharedPtr<ImFontAtlas, ESPMode::NotThreadSafe> FontAtlas;
		int32 FontAtlasFrameCount = 0;
	};
}
