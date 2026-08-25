// Copyright 2024-26 Amit Kumar Mehar. All Rights Reserved.

#if IMGUI_ALLOW_LOCAL_DRAWING

#if WITH_ENGINE
#include "RHI.h"
#include "ImGuiShaders.h"
#include "RHIStaticStates.h"
#include "RenderGraphUtils.h"
#include "SlateUTextureResource.h"
#include "GlobalRenderResources.h"
#include "CommonRenderResources.h"
#include "RenderCaptureInterface.h"
#include "Rendering/RenderingCommon.h"
#include "Runtime/Launch/Resources/Version.h"
#include "imgui/misc/imgui_threaded_rendering.h"
#endif

namespace ImGuiUtils
{
#if WITH_ENGINE
	static inline uint32 PackF16ToU32(const FVector2f& Value)
	{
		return uint32(FFloat16(Value.X).Encoded) | (uint32(FFloat16(Value.Y).Encoded) << 16);
	}

	class FImGuiVertexDeclaration : public FRenderResource
	{
	public:
		FVertexDeclarationRHIRef VertexDeclarationRHI;

		virtual void InitRHI(FRHICommandListBase& RHICmdList) override
		{
			struct FImGuiVertex
			{
				FVector2f	Position;
				FVector2f	UV;
				FColor		Color;
			};
			constexpr size_t VertexFormatStride = sizeof(FImGuiVertex);
			static_assert(VertexFormatStride == sizeof(ImDrawVert));

			FVertexDeclarationElementList Elements;
			Elements.Add(FVertexElement(0, STRUCT_OFFSET(FImGuiVertex, Position), VET_Float2, 0, VertexFormatStride));
			Elements.Add(FVertexElement(0, STRUCT_OFFSET(FImGuiVertex, UV), VET_Float2, 1, VertexFormatStride));
			Elements.Add(FVertexElement(0, STRUCT_OFFSET(FImGuiVertex, Color), VET_Color, 2, VertexFormatStride));
			VertexDeclarationRHI = PipelineStateCache::GetOrCreateVertexDeclaration(Elements);
		}

		virtual void ReleaseRHI() override
		{
			VertexDeclarationRHI.SafeRelease();
		}
	};
	static TGlobalResource<FImGuiVertexDeclaration, FRenderResource::EInitPhase::Pre> GImGuiVertexDeclaration;

	class FWidgetDrawer : public ICustomSlateElement
	{
		BEGIN_SHADER_PARAMETER_STRUCT(FRenderParameters, )
			RENDER_TARGET_BINDING_SLOTS()
		END_SHADER_PARAMETER_STRUCT()

	public:
		virtual ~FWidgetDrawer()
		{
			m_BoundTextures.Reset();
			m_DrawDataSnapshot.Clear();
			m_BoundTextureResources.Reset();
		}

		bool SetDrawData(ImDrawData* DrawData, double CurrentTime, FVector2f DrawRectOffset)
		{
			m_DrawDataSnapshot.SnapUsingSwap(DrawData, CurrentTime);
			DrawData = &m_DrawDataSnapshot.DrawData;

			m_DrawRectOffset = DrawRectOffset;

			UImGuiSubsystem* ImGuiSubsystem = UImGuiSubsystem::Get();
			ImGuiSubsystem->UpdateFontAtlasTextures(DrawData->Textures->Data, DrawData->Textures->Size);

			m_BoundTextures.Reset();
			m_BoundTextureResources.Reset();

			m_bHasDrawCommands = DrawData->TotalVtxCount > 0 &&
				DrawData->TotalIdxCount > 0 &&
				(DrawData->DisplaySize.x > KINDA_SMALL_NUMBER) &&
				(DrawData->DisplaySize.y > KINDA_SMALL_NUMBER);
			if (!m_bHasDrawCommands)
			{
				return false;
			}

			for (const ImDrawList* CmdList : DrawData->CmdLists)
			{
				for (const ImDrawCmd& DrawCmd : CmdList->CmdBuffer)
				{
					ImTextureID ResourceId = DrawCmd.GetTexID();
					if (m_BoundTextureResources.FindByKey(ResourceId))
						continue;

					const FImGuiTextureResource* TextureResource = ImGuiSubsystem->GetOneFrameResource(ResourceId);
					if (!TextureResource)
						continue;

					m_BoundTextureResources.Emplace(*TextureResource, TextureResource->GetSlateShaderResource(), ResourceId);
				}
			}

			m_bCaptureGpuFrame = ImGuiSubsystem->CaptureGpuFrame();

			return true;
		}

		void SetDrawRectOffset(FVector2f DrawRectOffset)
		{
			m_DrawRectOffset = DrawRectOffset;
		}

		bool HasDrawCommands() const
		{
			return m_bHasDrawCommands;
		}

		virtual void Draw_RenderThread(FRDGBuilder& GraphBuilder, const FDrawPassInputs& Inputs) override
		{
			FRenderParameters* PassParameters = GraphBuilder.AllocParameters<FRenderParameters>();
			PassParameters->RenderTargets[0] = FRenderTargetBinding(Inputs.OutputTexture, ERenderTargetLoadAction::ELoad);

			GraphBuilder.AddPass(RDG_EVENT_NAME("RenderImGui"), PassParameters, ERDGPassFlags::Raster | ERDGPassFlags::NeverCull,
				[this](FRHICommandListImmediate& RHICmdList)
				{
					DECLARE_SCOPE_CYCLE_COUNTER(TEXT("Render Widget [RT]"), STAT_ImGui_RenderWidget_RT, STATGROUP_ImGui);

					RenderCaptureInterface::FScopedCapture Capture{ m_bCaptureGpuFrame, &RHICmdList, TEXT("ImGui") };

					const ImDrawData* DrawData = &m_DrawDataSnapshot.DrawData;

					const ImRect DrawRect = ImRect(
						ImVec2(m_DrawRectOffset.X, m_DrawRectOffset.Y),
						ImVec2(m_DrawRectOffset.X, m_DrawRectOffset.Y) + DrawData->DisplaySize);
					const ImRect ViewportRect = ImRect(
						FMath::RoundToFloat(DrawRect.Min.x), FMath::RoundToFloat(DrawRect.Min.y),
						FMath::RoundToFloat(DrawRect.Max.x), FMath::RoundToFloat(DrawRect.Max.y));

					const ImVec2 DisplayPos = ImVec2(FMath::RoundToFloat(DrawData->DisplayPos.x), FMath::RoundToFloat(DrawData->DisplayPos.y));
					const ImVec2 DisplaySize = ViewportRect.GetSize();

					for (const auto& TextureResourceInfo : m_BoundTextureResources)
					{
						FBoundTexture BoundTexture{};

						if (TextureResourceInfo.TextureResource.UsesRawTextureResource())
						{
							if (FTextureResource* TextureResource = TextureResourceInfo.TextureResource.GetTextureResource())
							{
								// update render time similar to `FSlateBaseUTextureResource::AccessRHIResource`
								TextureResource->LastRenderTime = FApp::GetCurrentTime();

								BoundTexture.TextureRHI = TextureResource->TextureRHI;
								BoundTexture.SamplerRHI = TextureResource->SamplerStateRHI;
								BoundTexture.IsSRGB = TextureResource->bSRGB;
							}
						}
						else
						{
							FSlateShaderResource* ShaderResource = TextureResourceInfo.ExpectedSlateResource;

							// validate resource against handle, this is needed when spamming slate atlas resizes/repacking
							if (TextureResourceInfo.TextureResource.UsesSlateResourceHandle())
							{
								const FSlateShaderResourceProxy* SlateResourceProxy = TextureResourceInfo.TextureResource.GetSlateShaderResourceProxy();
								FSlateShaderResource* ActualResource = SlateResourceProxy ? SlateResourceProxy->Resource : nullptr;

								if (ShaderResource != ActualResource)
								{
									if (ActualResource)
									{
										ShaderResource = ActualResource;
										// adjust UVs, this is not 100% correct
										// since UV is also written to ImGui vertices, this will only work if the draw call is a 0-1 UV quad (not merged with other slate brushes)
										BoundTexture.TexCoordOverrideMode = FUintVector2(PackF16ToU32(SlateResourceProxy->StartUV), PackF16ToU32(SlateResourceProxy->SizeUV));
									}
									else
									{
										// under heavy load/repacking (multiple render thread flushes) we don't get the resource at all
										// TODO: is there a better way to handle this? I have encountered non ImGui related editor slate crashes too (so maybe its a limitation?)
										ShaderResource = nullptr;
									}
								}
							}

							if (ShaderResource)
							{
								const ESlateShaderResource::Type ResourceType = ShaderResource->GetType();
								if (ResourceType == ESlateShaderResource::Type::TextureObject)
								{
									// NOTE: not too happy about accessing TextureObject here (reading UObject on render thread)
									// but that is how slate is using these resources atm, so might be safe-ish
									// alternative would be to resolve the texture resource when updating `m_BoundTextureResources` (on game thread)
									FSlateBaseUTextureResource* TextureObjectResource = static_cast<FSlateBaseUTextureResource*>(ShaderResource);
									if (FTextureResource* TextureResource = TextureObjectResource->GetTextureObject()->GetResource())
									{
										BoundTexture.TextureRHI = TextureObjectResource->AccessRHIResource();
										BoundTexture.SamplerRHI = TextureResource->SamplerStateRHI;
										BoundTexture.IsSRGB = TextureResource->bSRGB;
									}
								}
								else if (ResourceType == ESlateShaderResource::Type::NativeTexture)
								{
									if (FRHITexture* NativeTextureRHI = ((TSlateTexture<FTextureRHIRef>*)ShaderResource)->GetTypedResource())
									{
										BoundTexture.TextureRHI = NativeTextureRHI;
										BoundTexture.IsSRGB = EnumHasAnyFlags(NativeTextureRHI->GetFlags(), ETextureCreateFlags::SRGB);
									}
								}
							}
						}

						if (BoundTexture.TextureRHI == nullptr)
						{
							BoundTexture.TextureRHI = GBlackTexture->TextureRHI;
						}
						if (BoundTexture.SamplerRHI == nullptr)
						{
							BoundTexture.SamplerRHI = TStaticSamplerState<SF_Bilinear, AM_Wrap, AM_Wrap, AM_Wrap>::GetRHI();
						}
						BoundTexture.ResourceId = TextureResourceInfo.ResourceId;

						m_BoundTextures.Add(MoveTemp(BoundTexture));
					}

					FBoundTexture FallbackTexture{};
					FallbackTexture.TextureRHI = GWhiteTexture->TextureRHI;
					FallbackTexture.SamplerRHI = TStaticSamplerState<SF_Point>::GetRHI();

#if ((ENGINE_MAJOR_VERSION * 100u + ENGINE_MINOR_VERSION) > 505) //(Version > 5.5)
					FRHIBufferCreateDesc VertexBufferDesc =
						FRHIBufferCreateDesc::CreateVertex<ImDrawVert>(TEXT("ImGui_VertexBuffer"), DrawData->TotalVtxCount)
						.AddUsage(EBufferUsageFlags::Volatile | EBufferUsageFlags::VertexBuffer)
						.SetInitialState(ERHIAccess::VertexOrIndexBuffer)
						.SetInitActionNone();
					FBufferRHIRef VertexBuffer = RHICmdList.CreateBuffer(VertexBufferDesc);

					FRHIBufferCreateDesc IndexBufferDesc =
						FRHIBufferCreateDesc::CreateIndex<ImDrawIdx>(TEXT("ImGui_IndexBuffer"), DrawData->TotalIdxCount)
						.AddUsage(EBufferUsageFlags::Volatile | EBufferUsageFlags::IndexBuffer)
						.SetInitialState(ERHIAccess::VertexOrIndexBuffer)
						.SetInitActionNone();
					FBufferRHIRef IndexBuffer = RHICmdList.CreateBuffer(IndexBufferDesc);
#else
					FRHIResourceCreateInfo VertexBufferCreateInfo(TEXT("ImGui_VertexBuffer"));
					FBufferRHIRef VertexBuffer = RHICmdList.CreateBuffer(
						DrawData->TotalVtxCount * sizeof(ImDrawVert), EBufferUsageFlags::Volatile | EBufferUsageFlags::VertexBuffer,
						sizeof(ImDrawVert), ERHIAccess::VertexOrIndexBuffer, VertexBufferCreateInfo);

					FRHIResourceCreateInfo IndexBufferCreateInfo(TEXT("ImGui_IndexBuffer"));
					FBufferRHIRef IndexBuffer = RHICmdList.CreateBuffer(
						DrawData->TotalIdxCount * sizeof(ImDrawIdx), EBufferUsageFlags::Volatile | EBufferUsageFlags::IndexBuffer,
						sizeof(ImDrawIdx), ERHIAccess::VertexOrIndexBuffer, IndexBufferCreateInfo);
#endif

					ImDrawVert* VertexDst = (ImDrawVert*)RHICmdList.LockBuffer(VertexBuffer, 0u, DrawData->TotalVtxCount * sizeof(ImDrawVert), RLM_WriteOnly);
					ImDrawIdx* IndexDst = (ImDrawIdx*)RHICmdList.LockBuffer(IndexBuffer, 0u, DrawData->TotalIdxCount * sizeof(ImDrawIdx), RLM_WriteOnly);
					if (ensure(VertexDst && IndexDst))
					{
						for (const ImDrawList* CmdList : DrawData->CmdLists)
						{
							FMemory::Memcpy(VertexDst, CmdList->VtxBuffer.Data, CmdList->VtxBuffer.Size * sizeof(ImDrawVert));
							FMemory::Memcpy(IndexDst, CmdList->IdxBuffer.Data, CmdList->IdxBuffer.Size * sizeof(ImDrawIdx));

							VertexDst += CmdList->VtxBuffer.Size;
							IndexDst += CmdList->IdxBuffer.Size;
						}
						RHICmdList.UnlockBuffer(VertexBuffer);
						RHICmdList.UnlockBuffer(IndexBuffer);
					}

					{
						TShaderMapRef<FImGuiVS> VertexShader(GetGlobalShaderMap(GMaxRHIFeatureLevel));
						TShaderMapRef<FImGuiPS> PixelShader(GetGlobalShaderMap(GMaxRHIFeatureLevel));

						FGraphicsPipelineStateInitializer GraphicsPSOInit;
						RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);
						GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();
						GraphicsPSOInit.RasterizerState = TStaticRasterizerState<FM_Solid, CM_None>::GetRHI();
						GraphicsPSOInit.BlendState = TStaticBlendState<CW_RGBA, BO_Add, BF_SourceAlpha, BF_InverseSourceAlpha, BO_Add, BF_One, BF_One>::GetRHI();
						GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GImGuiVertexDeclaration.VertexDeclarationRHI;
						GraphicsPSOInit.BoundShaderState.VertexShaderRHI = VertexShader.GetVertexShader();
						GraphicsPSOInit.BoundShaderState.PixelShaderRHI = PixelShader.GetPixelShader();
						GraphicsPSOInit.PrimitiveType = PT_TriangleList;
						SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit, 0);

						RHICmdList.SetStreamSource(0, VertexBuffer, 0);

						auto CalculateProjectionMatrix = [&]()
							{
								const float L = DisplayPos.x;
								const float R = DisplayPos.x + DisplaySize.x;
								const float T = DisplayPos.y;
								const float B = DisplayPos.y + DisplaySize.y;
								FMatrix44f ProjectionMatrix =
								{
									{ 2.0f / (R - L)	, 0.0f			   , 0.0f, 0.0f },
									{ 0.0f				, 2.0f / (T - B)   , 0.0f, 0.0f },
									{ 0.0f				, 0.0f			   , 0.5f, 0.0f },
									{ (R + L) / (L - R)	, (T + B) / (B - T), 0.5f, 1.0f },
								};
								return ProjectionMatrix;
							};

						FMatrix44f ProjectionMatrixParam = CalculateProjectionMatrix();
						uint32_t ShaderStateOverrides = 0;
						bool bForcePointSamplerState = false;
						FRHISamplerState* PointSamplerStateRHI = TStaticSamplerState<>::GetRHI();

						RHICmdList.SetViewport(ViewportRect.Min.x, ViewportRect.Min.y, 0.f, ViewportRect.Max.x, ViewportRect.Max.y, 1.f);
						RHICmdList.SetScissorRect(false, 0.f, 0.f, 0.f, 0.f);

						uint32 GlobalVertexOffset = 0;
						uint32 GlobalIndexOffset = 0;
						for (const ImDrawList* CmdList : DrawData->CmdLists)
						{
							for (const ImDrawCmd& DrawCmd : CmdList->CmdBuffer)
							{
								if (DrawCmd.UserCallback != NULL)
								{
									if (DrawCmd.UserCallback == ImDrawCallback_ResetRenderState)
									{
										RHICmdList.SetViewport(ViewportRect.Min.x, ViewportRect.Min.y, 0.f, ViewportRect.Max.x, ViewportRect.Max.y, 1.f);
										RHICmdList.SetScissorRect(false, 0.f, 0.f, 0.f, 0.f);

										ProjectionMatrixParam = CalculateProjectionMatrix();
									}
									else if (DrawCmd.UserCallback == ImDrawCallback_SetShaderState)
									{
										ShaderStateOverrides = static_cast<uint32>(reinterpret_cast<uintptr_t>(DrawCmd.UserCallbackData));

										// No VS state exposed atm.
									}
									else if (DrawCmd.UserCallback == ImDrawCallback_SetSamplerStatePoint || DrawCmd.UserCallback == ImDrawCallback_ResetSamplerState)
									{
										bForcePointSamplerState = (DrawCmd.UserCallback == ImDrawCallback_SetSamplerStatePoint);
									}
									else
									{
										DrawCmd.UserCallback(RHICmdList, DrawRect, DrawData->OwnerViewport ? DrawData->OwnerViewport->Pos : ImVec2(0.f, 0.f), DrawCmd.UserCallbackData, DrawCmd.UserCallbackDataSize);

										// TODO: add a flag to tell whether callback modified the render state here?
										{
											SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit, 0);
											RHICmdList.SetStreamSource(0, VertexBuffer, 0);

											RHICmdList.SetViewport(ViewportRect.Min.x, ViewportRect.Min.y, 0.f, ViewportRect.Max.x, ViewportRect.Max.y, 1.f);
											RHICmdList.SetScissorRect(false, 0.f, 0.f, 0.f, 0.f);

											ProjectionMatrixParam = CalculateProjectionMatrix();
										}
									}
								}
								else
								{
									const float ScissorRectLeft = FMath::Clamp(DrawCmd.ClipRect.x - DisplayPos.x + ViewportRect.Min.x, ViewportRect.Min.x, ViewportRect.Max.x);
									const float ScissorRectTop = FMath::Clamp(DrawCmd.ClipRect.y - DisplayPos.y + ViewportRect.Min.y, ViewportRect.Min.y, ViewportRect.Max.y);
									const float ScissorRectRight = FMath::Clamp(DrawCmd.ClipRect.z - DisplayPos.x + ViewportRect.Min.x, ViewportRect.Min.x, ViewportRect.Max.x);
									const float ScissorRectBottom = FMath::Clamp(DrawCmd.ClipRect.w - DisplayPos.y + ViewportRect.Min.y, ViewportRect.Min.y, ViewportRect.Max.y);
									if (ScissorRectRight <= ScissorRectLeft || ScissorRectBottom <= ScissorRectTop)
									{
										continue;
									}

									RHICmdList.SetScissorRect(true, ScissorRectLeft, ScissorRectTop, ScissorRectRight, ScissorRectBottom);

									const FBoundTexture* Texture = m_BoundTextures.FindByKey(DrawCmd.GetTexID());
									if (!Texture)
									{
										Texture = &FallbackTexture;
									}

									SetShaderParametersLegacyVS(
										RHICmdList,
										VertexShader,
										ProjectionMatrixParam,
										Texture->TexCoordOverrideMode);

									SetShaderParametersLegacyPS(
										RHICmdList,
										PixelShader,
										Texture->TextureRHI,
										bForcePointSamplerState ? PointSamplerStateRHI : Texture->SamplerRHI.GetReference(),
										ShaderStateOverrides | (Texture->IsSRGB ? (uint32)EImGuiShaderState::OutputInSRGB : 0));

									RHICmdList.DrawIndexedPrimitive(IndexBuffer, DrawCmd.VtxOffset + GlobalVertexOffset, 0, DrawCmd.ElemCount, DrawCmd.IdxOffset + GlobalIndexOffset, DrawCmd.ElemCount / 3, 1);
								}
							}
							GlobalVertexOffset += CmdList->VtxBuffer.Size;
							GlobalIndexOffset += CmdList->IdxBuffer.Size;
						}
					}
				});
		}
	private:
		struct FTextureResourceInfo
		{
			FImGuiTextureResource TextureResource;
			// slate resource can update when resizing atlases, keep track of what the gamethread thinks the resource is
			// if it changes, we override the texture coordinates (not 100% correct, but works for most cases)
			FSlateShaderResource* ExpectedSlateResource = nullptr;
			ImTextureID ResourceId = ImTextureID_Invalid;

			bool operator==(ImTextureID InResourceId) const { return ResourceId == InResourceId; }
		};
		struct FBoundTexture
		{
			FTextureRHIRef TextureRHI = nullptr;
			FSamplerStateRHIRef SamplerRHI = nullptr;
			FUintVector2 TexCoordOverrideMode = FUintVector2::ZeroValue;
			bool IsSRGB = false;
			ImTextureID ResourceId = ImTextureID_Invalid;

			bool operator==(ImTextureID InResourceId) const { return ResourceId == InResourceId; }
		};
		TArray<FBoundTexture> m_BoundTextures;
		TArray<FTextureResourceInfo> m_BoundTextureResources;
		FVector2f m_DrawRectOffset = FVector2f::ZeroVector;
		ImDrawDataSnapshot m_DrawDataSnapshot;
		bool m_bHasDrawCommands = false;
		bool m_bCaptureGpuFrame = false;
	};
#else
	class FWidgetDrawer
	{
	public:
		bool SetDrawData(ImDrawData* DrawData, double CurrentTime, FVector2f DrawRectOffset)
		{
			m_DrawData = DrawData;
			m_DrawRectOffset = DrawRectOffset;

			UImGuiSubsystem* ImGuiSubsystem = UImGuiSubsystem::Get();
			ImGuiSubsystem->UpdateFontAtlasTextures(DrawData->Textures->Data, DrawData->Textures->Size);

			m_bHasDrawCommands = DrawData->TotalVtxCount > 0 &&
				DrawData->TotalIdxCount > 0 &&
				(DrawData->DisplaySize.x > KINDA_SMALL_NUMBER) &&
				(DrawData->DisplaySize.y > KINDA_SMALL_NUMBER);
			if (!m_bHasDrawCommands)
			{
				return false;
			}

			return true;
		}

		void SetDrawRectOffset(FVector2f DrawRectOffset)
		{
			m_DrawRectOffset = DrawRectOffset;
		}

		bool HasDrawCommands() const
		{
			return m_bHasDrawCommands;
		}

		void DrawSlateWidget(const FGeometry& AllottedGeometry, FSlateWindowElementList& OutDrawElements, int32 LayerId)
		{
			UImGuiSubsystem* ImGuiSubsystem = UImGuiSubsystem::Get();

			const FSlateRenderTransform WidgetTransform(FVector2f(AllottedGeometry.GetAccumulatedRenderTransform().GetTranslation()) - FVector2f(m_DrawData->DisplayPos.x, m_DrawData->DisplayPos.y));

			for (const ImDrawList* CmdList : m_DrawData->CmdLists)
			{
				for (const auto& DrawCmd : CmdList->CmdBuffer)
				{
					if (DrawCmd.UserCallback != NULL)
					{
						// TODO: callbacks not supported atm
						continue;
					}
					if (DrawCmd.ElemCount == 0)
					{
						continue;
					}
					static_assert(sizeof(SlateIndex) >= sizeof(ImDrawIdx));

					SlateIndices.SetNumUninitialized(DrawCmd.ElemCount, EAllowShrinking::No);
					SlateIndices[0] = CmdList->IdxBuffer[DrawCmd.IdxOffset];
					SlateIndex MinIndex = SlateIndices[0];
					SlateIndex MaxIndex = SlateIndices[0];
					for (uint32 Index = 1; Index < DrawCmd.ElemCount; ++Index)
					{
						SlateIndices[Index] = CmdList->IdxBuffer[DrawCmd.IdxOffset + Index];
						MinIndex = FMath::Min(MinIndex, SlateIndices[Index]);
						MaxIndex = FMath::Max(MaxIndex, SlateIndices[Index]);
					}
					for (SlateIndex& Index : SlateIndices)
					{
						Index -= MinIndex;
					}

					// slate copies the vertex array so restrict size to active indices
					SlateVertices.SetNumUninitialized(MaxIndex - MinIndex + 1, EAllowShrinking::No);
					for (int32 VertIndex = 0; VertIndex < SlateVertices.Num(); ++VertIndex)
					{
						const ImDrawVert& ImGuiVertex = CmdList->VtxBuffer[DrawCmd.VtxOffset + MinIndex + VertIndex];
						FSlateVertex& SlateVertex = SlateVertices[VertIndex];

						SlateVertex.Position = WidgetTransform.TransformPoint(FVector2f{ ImGuiVertex.pos.x, ImGuiVertex.pos.y });

						SlateVertex.TexCoords[0] = ImGuiVertex.uv.x;
						SlateVertex.TexCoords[1] = ImGuiVertex.uv.y;
						SlateVertex.TexCoords[2] = 1.f;
						SlateVertex.TexCoords[3] = 1.f;

						SlateVertex.Color.Bits = ImGuiVertex.col;
						Swap(SlateVertex.Color.R, SlateVertex.Color.B);
					}

					FSlateRect ClippingRect;
					ClippingRect.Left = DrawCmd.ClipRect.x;
					ClippingRect.Top = DrawCmd.ClipRect.y;
					ClippingRect.Right = DrawCmd.ClipRect.z;
					ClippingRect.Bottom = DrawCmd.ClipRect.w;

					OutDrawElements.PushClip(FSlateClippingZone{ TransformRect(WidgetTransform, ClippingRect) });
					{
						const FImGuiTextureResource* TextureResource = ImGuiSubsystem->GetOneFrameResource(DrawCmd.GetTexID());

						// TODO: A bit unfortunate but when resizing slate brushes there's no good way to handle one frame flicker
						// The slate resource gets initialized after this function call so there's really no way to patch the vertex texcoords
						// Using FSlateBoxElement could work but there's no way to handle z-ordering (box elements are drawn before custom verts)
						// We force local image cache to bypass slate brush rendering, so hopefully should not encounter this issue
						const bool bIsResourceValid = TextureResource && TextureResource->GetSlateShaderResource();

						if (bIsResourceValid)
						{
							FSlateDrawElement::MakeCustomVerts(OutDrawElements, LayerId, TextureResource->GetResourceHandle(), SlateVertices, SlateIndices, nullptr, 0, 0);
						}
						else
						{
							FSlateDrawElement::MakeCustomVerts(OutDrawElements, LayerId, FSlateResourceHandle(), SlateVertices, SlateIndices, nullptr, 0, 0);
						}
					}
					OutDrawElements.PopClip();
				}
			}
		}

	private:
		TArray<SlateIndex> SlateIndices;
		TArray<FSlateVertex> SlateVertices;
		FVector2f m_DrawRectOffset = FVector2f::ZeroVector;
		const ImDrawData* m_DrawData = nullptr;
		bool m_bHasDrawCommands = false;
	};
#endif //#if WITH_ENGINE
}

#else

namespace ImGuiUtils
{
	class FWidgetDrawer
	{
	public:
		bool SetDrawData(ImDrawData* DrawData, double CurrentTime, FVector2f DrawRectOffset)
		{
			return false;
		}
		void SetDrawRectOffset(FVector2f DrawRectOffset)
		{
		}
		bool HasDrawCommands() const
		{
			return false;
		}
	};
}

#endif //#if IMGUI_ALLOW_LOCAL_DRAWING
