// Copyright 2024 Amit Kumar Mehar. All Rights Reserved.

#include "SImGuiWidgets.h"
#include "ImGuiShaders.h"
#include "ImGuiSubsystem.h"

#include "RHI.h"
#include "RHIStaticStates.h"
#include "Engine/Texture2D.h"
#include "RenderGraphUtils.h"
#include "GlobalRenderResources.h"
#include "CommonRenderResources.h"
#include "SlateUTextureResource.h"
#include "RenderCaptureInterface.h"
#include "Engine/TextureRenderTarget2D.h"

#include "Input/Events.h"
#include "InputCoreTypes.h"
#include "Application/ThrottleManager.h"
#include "Runtime/Launch/Resources/Version.h"
#include "Framework/Application/SlateApplication.h"

class FImGuiVertexDeclaration : public FRenderResource
{
public:
	FVertexDeclarationRHIRef VertexDeclarationRHI;

	virtual ~FImGuiVertexDeclaration() {}

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
		Elements.Add(FVertexElement(0, STRUCT_OFFSET(FImGuiVertex, Position),	VET_Float2, 0, VertexFormatStride));
		Elements.Add(FVertexElement(0, STRUCT_OFFSET(FImGuiVertex, UV),			VET_Float2, 1, VertexFormatStride));
		Elements.Add(FVertexElement(0, STRUCT_OFFSET(FImGuiVertex, Color),		VET_Color,	2, VertexFormatStride));
		VertexDeclarationRHI = PipelineStateCache::GetOrCreateVertexDeclaration(Elements);
	}

	virtual void ReleaseRHI() override
	{
		VertexDeclarationRHI.SafeRelease();
	}
};
static TGlobalResource<FImGuiVertexDeclaration, FRenderResource::EInitPhase::Pre> GImGuiVertexDeclaration;

void SImGuiWidgetBase::Construct(const FArguments& InArgs, bool UseTranslucentBackground)
{
	UImGuiSubsystem* ImGuiSubsystem = UImGuiSubsystem::Get();

	m_ImGuiContext = ImGui::CreateContext(ImGuiSubsystem->GetSharedFontAtlas());

	ImGuiIO& IO = GetImGuiIO();
	IO.IniFilename = ImGuiSubsystem->GetIniFilePath();

	IO.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
	IO.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
	IO.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;

	IO.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset;
	IO.BackendFlags |= ImGuiBackendFlags_RendererHasTextures;

	// TODO: setting?
	ImGui::StyleColorsDark();

	// allocate rendering resources
	m_ImGuiRT = NewObject<UTextureRenderTarget2D>();
	m_ImGuiRT->bCanCreateUAV = false;
	m_ImGuiRT->bAutoGenerateMips = false;
	m_ImGuiRT->Filter = TextureFilter::TF_Nearest;
	m_ImGuiRT->RenderTargetFormat = ETextureRenderTargetFormat::RTF_RGBA8;
	m_ImGuiRT->ClearColor = FLinearColor(0.f, 0.f, 0.f, 0.f);
	m_ImGuiRT->InitAutoFormat(32, 32);
	m_ImGuiRT->UpdateResourceImmediate(true);

	m_ImGuiSlateBrush.SetResourceObject(m_ImGuiRT);

	// only need to clear the RT when using translucent window, otherwise ImGui fullscreen widget pass should clear the RT.
	m_ClearRenderTargetEveryFrame = UseTranslucentBackground;
}

SImGuiWidgetBase::~SImGuiWidgetBase()
{
	if (m_ImGuiContext)
	{
		ImGui::DestroyContext(m_ImGuiContext);
		m_ImGuiContext = nullptr;
	}
}

ImGuiIO& SImGuiWidgetBase::GetImGuiIO() const
{
	checkf(m_ImGuiContext, TEXT("ImGuiContext is invalid!"));

	ImGui::SetCurrentContext(m_ImGuiContext);
	return ImGui::GetIO();
}

void SImGuiWidgetBase::AddReferencedObjects(FReferenceCollector& Collector)
{
	Collector.AddReferencedObject(m_ImGuiRT);
}

FString SImGuiWidgetBase::GetReferencerName() const
{
	return TEXT("SImGuiWidgetBase");
}

void SImGuiWidgetBase::Tick(const FGeometry& WidgetGeometry, const double CurrentTime, const float DeltaTime)
{
	DECLARE_SCOPE_CYCLE_COUNTER(TEXT("Tick Widget"), STAT_ImGui_TickWidget, STATGROUP_ImGui);

	Super::Tick(WidgetGeometry, CurrentTime, DeltaTime);
	
	if (!m_ImGuiTickedByInputProcessing)
	{
		FImGuiTickContext TickContext{};
		TickContext.ImGuiContext = m_ImGuiContext;
		if (FSlateApplication::Get().IsDragDropping())
		{
			TickContext.DragDropOperation = FSlateApplication::Get().GetDragDroppingContent();
		}
		/*FImGuiTickResult TickResult = */TickImGui(&WidgetGeometry, &TickContext);
	}
	m_ImGuiTickedByInputProcessing = false;
}

int32 SImGuiWidgetBase::OnPaint(const FPaintArgs& Args, const FGeometry& WidgetGeometry, const FSlateRect& ClippingRect,
	FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& WidgetStyle, bool bParentEnabled) const
{
	DECLARE_SCOPE_CYCLE_COUNTER(TEXT("Render Widget"), STAT_ImGui_RenderWidget, STATGROUP_ImGui);

	UImGuiSubsystem* ImGuiSubsystem = UImGuiSubsystem::Get();

	ImGuiIO& IO = GetImGuiIO();

	ImGui::Render();

	ImDrawData* DrawData = ImGui::GetDrawData();

	for (ImTextureData* TexData : *DrawData->Textures)
	{
		ImGuiSubsystem->UpdateTextureData(TexData);
	}

	if (DrawData->DisplaySize.x > 0.0f && DrawData->DisplaySize.y > 0.0f)
	{
		if (DrawData->TotalVtxCount > 0 && DrawData->TotalIdxCount > 0)
		{
			RenderCaptureInterface::FScopedCapture RenderCapture(ImGuiSubsystem->CaptureGpuFrame(), TEXT("ImGuiWidget"));

			struct FRenderData : FNoncopyable
			{
				struct FTextureInfo
				{
					FTextureRHIRef TextureRHI = nullptr;
					FSamplerStateRHIRef SamplerRHI = nullptr;
					bool IsSRGB = false;
				};
				TArray<ImDrawVert> VertexData;
				TArray<ImDrawIdx> IndexData;
				TArray<ImDrawCmd> DrawCommands;
				TArray<FTextureInfo> BoundTextures;
				ImVec2 DisplayPos = {};
				ImVec2 DisplaySize = {};
				int32 TotalVtxCount = 0;
				int32 TotalIdxCount = 0;
				int32 MissingTextureIndex = INDEX_NONE;
				bool bClearRenderTarget = false;
			};
			FRenderData* RenderData = new FRenderData();
			RenderData->DisplayPos = DrawData->DisplayPos;
			RenderData->DisplaySize = DrawData->DisplaySize;
			RenderData->TotalVtxCount = DrawData->TotalVtxCount;
			RenderData->TotalIdxCount = DrawData->TotalIdxCount;
			RenderData->VertexData.Reserve(RenderData->TotalVtxCount);
			RenderData->IndexData.Reserve(RenderData->TotalIdxCount);
			RenderData->DrawCommands.Reserve(DrawData->CmdListsCount * 4);
			
			uint32 GlobalVertexOffset = 0;
			uint32 GlobalIndexOffset = 0;
			for (const ImDrawList* CmdList : DrawData->CmdLists)
			{
				RenderData->IndexData.Append(CmdList->IdxBuffer.Data, CmdList->IdxBuffer.Size);
				RenderData->VertexData.Append(CmdList->VtxBuffer.Data, CmdList->VtxBuffer.Size);
				
				for (const ImDrawCmd& DrawCmd : CmdList->CmdBuffer)
				{
					auto& Cmd = RenderData->DrawCommands.Add_GetRef(DrawCmd);
					Cmd.VtxOffset += GlobalVertexOffset;
					Cmd.IdxOffset += GlobalIndexOffset;
				}
				GlobalIndexOffset += CmdList->IdxBuffer.Size;
				GlobalVertexOffset += CmdList->VtxBuffer.Size;
			}

			RenderData->MissingTextureIndex = ImGuiSubsystem->GetMissingImageTextureIndex();
			RenderData->BoundTextures.Reserve(ImGuiSubsystem->GetOneFrameResources().Num());
			for (const FImGuiTextureResource& TextureResource : ImGuiSubsystem->GetOneFrameResources())
			{
				auto& TextureInfo = RenderData->BoundTextures.AddDefaulted_GetRef();

				FSlateShaderResource* ShaderResource = TextureResource.GetSlateShaderResource();
				if (ShaderResource)
				{
					ESlateShaderResource::Type ResourceType = ShaderResource->GetType();

					if (ResourceType == ESlateShaderResource::Type::TextureObject)
					{
						FSlateBaseUTextureResource* TextureObjectResource = static_cast<FSlateBaseUTextureResource*>(ShaderResource);
						if (FTextureResource* Resource = TextureObjectResource->GetTextureObject()->GetResource())
						{
							TextureInfo.TextureRHI = Resource->TextureRHI;
							TextureInfo.SamplerRHI = Resource->SamplerStateRHI;
							TextureInfo.IsSRGB = Resource->bSRGB;
						}
					}
					else if (ResourceType == ESlateShaderResource::Type::NativeTexture)
					{
						if (FRHITexture* NativeTextureRHI = ((TSlateTexture<FTextureRHIRef>*)ShaderResource)->GetTypedResource())
						{
							TextureInfo.TextureRHI = NativeTextureRHI;
							TextureInfo.IsSRGB = EnumHasAnyFlags(NativeTextureRHI->GetFlags(), ETextureCreateFlags::SRGB);
						}
					}
				}

				if (TextureInfo.TextureRHI == nullptr)
				{
					TextureInfo.TextureRHI = GBlackTexture->TextureRHI;
				}
				if (TextureInfo.SamplerRHI == nullptr)
				{
					TextureInfo.SamplerRHI = TStaticSamplerState<SF_Bilinear, AM_Wrap, AM_Wrap, AM_Wrap>::GetRHI();
				}
			}

			RenderData->bClearRenderTarget = m_ClearRenderTargetEveryFrame;

			ENQUEUE_RENDER_COMMAND(RenderImGui)(
				[RenderData, RT_Resource = m_ImGuiRT->GetResource()](FRHICommandListImmediate& RHICmdList)
				{
#if ((ENGINE_MAJOR_VERSION * 100u + ENGINE_MINOR_VERSION) > 505) //(Version > 5.5)
					FRHIBufferCreateDesc VertexBufferDesc =
						FRHIBufferCreateDesc::CreateVertex<ImDrawVert>(TEXT("ImGui_VertexBuffer"), RenderData->TotalVtxCount)
						.AddUsage(EBufferUsageFlags::Volatile | EBufferUsageFlags::VertexBuffer)
						.SetInitialState(ERHIAccess::VertexOrIndexBuffer)
						.SetInitActionNone();
					FBufferRHIRef VertexBuffer = RHICmdList.CreateBuffer(VertexBufferDesc);
#else
					FRHIResourceCreateInfo VertexBufferCreateInfo(TEXT("ImGui_VertexBuffer"));
					FBufferRHIRef VertexBuffer = RHICmdList.CreateBuffer(
						RenderData->TotalVtxCount * sizeof(ImDrawVert), EBufferUsageFlags::Volatile | EBufferUsageFlags::VertexBuffer,
						sizeof(ImDrawVert), ERHIAccess::VertexOrIndexBuffer, VertexBufferCreateInfo);
#endif
					if (ImDrawVert* VertexDst = (ImDrawVert*)RHICmdList.LockBuffer(VertexBuffer, 0u, RenderData->VertexData.NumBytes(), RLM_WriteOnly))
					{
						FMemory::Memcpy(VertexDst, RenderData->VertexData.GetData(), RenderData->VertexData.NumBytes());
						RHICmdList.UnlockBuffer(VertexBuffer);
					}

#if ((ENGINE_MAJOR_VERSION * 100u + ENGINE_MINOR_VERSION) > 505) //(Version > 5.5)
					FRHIBufferCreateDesc IndexBufferDesc =
						FRHIBufferCreateDesc::CreateIndex<ImDrawIdx>(TEXT("ImGui_IndexBuffer"), RenderData->TotalIdxCount)
						.AddUsage(EBufferUsageFlags::Volatile | EBufferUsageFlags::IndexBuffer)
						.SetInitialState(ERHIAccess::VertexOrIndexBuffer)
						.SetInitActionNone();
					FBufferRHIRef IndexBuffer = RHICmdList.CreateBuffer(IndexBufferDesc);
#else
					FRHIResourceCreateInfo IndexBufferCreateInfo(TEXT("ImGui_IndexBuffer"));
					FBufferRHIRef IndexBuffer = RHICmdList.CreateBuffer(
						RenderData->TotalIdxCount * sizeof(ImDrawIdx), EBufferUsageFlags::Volatile | EBufferUsageFlags::IndexBuffer,
						sizeof(ImDrawIdx), ERHIAccess::VertexOrIndexBuffer, IndexBufferCreateInfo);
#endif
					if (ImDrawIdx* IndexDst = (ImDrawIdx*)RHICmdList.LockBuffer(IndexBuffer, 0u, RenderData->IndexData.NumBytes(), RLM_WriteOnly))
					{
						FMemory::Memcpy(IndexDst, RenderData->IndexData.GetData(), RenderData->IndexData.NumBytes());
						RHICmdList.UnlockBuffer(IndexBuffer);
					}

					const ERenderTargetLoadAction LoadAction = RenderData->bClearRenderTarget ? ERenderTargetLoadAction::EClear : ERenderTargetLoadAction::ENoAction;
					FRHIRenderPassInfo RPInfo(RT_Resource->TextureRHI, MakeRenderTargetActions(LoadAction, ERenderTargetStoreAction::EStore));
					RHICmdList.Transition(FRHITransitionInfo(RT_Resource->TextureRHI, ERHIAccess::SRVGraphicsPixel, ERHIAccess::RTV));
					RHICmdList.BeginRenderPass(RPInfo, TEXT("RenderImGui"));
					{
						SCOPED_DRAW_EVENT(RHICmdList, RenderImGui);

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

						auto UpdateVertexShaderParameters = [&]() -> void
						{
							const float L = RenderData->DisplayPos.x;
							const float R = RenderData->DisplayPos.x + RenderData->DisplaySize.x;
							const float T = RenderData->DisplayPos.y;
							const float B = RenderData->DisplayPos.y + RenderData->DisplaySize.y;
							FMatrix44f ProjectionMatrix =
							{
								{ 2.0f / (R - L)	, 0.0f			   , 0.0f, 0.0f },
								{ 0.0f				, 2.0f / (T - B)   , 0.0f, 0.0f },
								{ 0.0f				, 0.0f			   , 0.5f, 0.0f },
								{ (R + L) / (L - R)	, (T + B) / (B - T), 0.5f, 1.0f },
							};

							SetShaderParametersLegacyVS(
								RHICmdList,
								VertexShader,
								ProjectionMatrix);
						};

						uint32_t RenderStateOverrides = 0;

						RHICmdList.SetViewport(0.f, 0.f, 0.f, RenderData->DisplaySize.x, RenderData->DisplaySize.y, 1.f);
						RHICmdList.SetScissorRect(false, 0.f, 0.f, 0.f, 0.f);

						UpdateVertexShaderParameters();

						for (const ImDrawCmd& DrawCmd : RenderData->DrawCommands)
						{
							if (DrawCmd.UserCallback != NULL)
							{
								if (DrawCmd.UserCallback == ImDrawCallback_ResetRenderState)
								{
									RHICmdList.SetViewport(0.f, 0.f, 0.f, RenderData->DisplaySize.x, RenderData->DisplaySize.y, 1.f);
									RHICmdList.SetScissorRect(false, 0.f, 0.f, 0.f, 0.f);

									RenderStateOverrides = 0;

									UpdateVertexShaderParameters();
								}
								else if (DrawCmd.UserCallback == ImDrawCallback_SetRenderState)
								{
									RenderStateOverrides = static_cast<uint32>(reinterpret_cast<uintptr_t>(DrawCmd.UserCallbackData));

									//UpdateVertexShaderParameters(); No VS state exposed atm.
								}
								else
								{
									DrawCmd.UserCallback(&RHICmdList, DrawCmd.UserCallbackData, DrawCmd.UserCallbackDataSize);

									// TODO: add a flag to tell whether callback modified the render state here?
									{
										SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit, 0);
										RHICmdList.SetStreamSource(0, VertexBuffer, 0);

										RHICmdList.SetViewport(0.f, 0.f, 0.f, RenderData->DisplaySize.x, RenderData->DisplaySize.y, 1.f);
										RHICmdList.SetScissorRect(false, 0.f, 0.f, 0.f, 0.f);

										UpdateVertexShaderParameters();
									}
								}
							}
							else
							{
								const ImVec2& ClipRectOffset = RenderData->DisplayPos;
								RHICmdList.SetScissorRect(
									true,
									FMath::Max(0.f, DrawCmd.ClipRect.x - ClipRectOffset.x),							// >=Viewport.MinX
									FMath::Max(0.f, DrawCmd.ClipRect.y - ClipRectOffset.y),							// >=Viewport.MinY
									FMath::Min(RenderData->DisplaySize.x, DrawCmd.ClipRect.z - ClipRectOffset.x),	// <=Viewport.MaxX
									FMath::Min(RenderData->DisplaySize.y, DrawCmd.ClipRect.w - ClipRectOffset.y));	// <=Viewport.MaxY
									
								uint32 TextureIndex = UImGuiSubsystem::ImGuiIDToIndex(DrawCmd.GetTexID());
								if (!(RenderData->BoundTextures.IsValidIndex(TextureIndex)/* && RenderData->BoundTextures[Index].IsValid()*/))
								{
									TextureIndex = RenderData->MissingTextureIndex;
								}

								SetShaderParametersLegacyPS(
									RHICmdList,
									PixelShader,
									RenderData->BoundTextures[TextureIndex].TextureRHI,
									RenderData->BoundTextures[TextureIndex].SamplerRHI,
									RenderData->BoundTextures[TextureIndex].IsSRGB,
									RenderStateOverrides);

								RHICmdList.DrawIndexedPrimitive(IndexBuffer, DrawCmd.VtxOffset, 0, DrawCmd.ElemCount, DrawCmd.IdxOffset, DrawCmd.ElemCount / 3, 1);
							}
						}

						delete RenderData;
					}
					RHICmdList.EndRenderPass();
					RHICmdList.Transition(FRHITransitionInfo(RT_Resource->TextureRHI, ERHIAccess::RTV, ERHIAccess::SRVGraphicsPixel));
				});

			const FSlateRenderTransform WidgetOffsetTransform = FTransform2f(1.f, { 0.f, 0.f });
			const FSlateRect DrawRect = WidgetGeometry.GetRenderBoundingRect();

			const FVector2f V0 = (FVector2f)DrawRect.GetTopLeft();
			const FVector2f V1 = (FVector2f)DrawRect.GetTopRight();
			const FVector2f V2 = (FVector2f)DrawRect.GetBottomRight();
			const FVector2f V3 = (FVector2f)DrawRect.GetBottomLeft();

			// adjust UVs based on RT vs Viewport scaling
			const float UVMinX = 0.f;
			const float UVMinY = 0.f;
			const float UVMaxX = (DrawRect.Right - DrawRect.Left) / (float)m_ImGuiRT->SizeX;
			const float UVMaxY = (DrawRect.Bottom - DrawRect.Top) / (float)m_ImGuiRT->SizeY;

			TArray<FSlateVertex> Vertices =
			{
				FSlateVertex::Make<ESlateVertexRounding::Disabled>(WidgetOffsetTransform, V0, FVector2f{ UVMinX, UVMinY }, FColor::White),
				FSlateVertex::Make<ESlateVertexRounding::Disabled>(WidgetOffsetTransform, V1, FVector2f{ UVMaxX, UVMinY }, FColor::White),
				FSlateVertex::Make<ESlateVertexRounding::Disabled>(WidgetOffsetTransform, V2, FVector2f{ UVMaxX, UVMaxY }, FColor::White),
				FSlateVertex::Make<ESlateVertexRounding::Disabled>(WidgetOffsetTransform, V3, FVector2f{ UVMinX, UVMaxY }, FColor::White)
			};
			TArray<uint32> Indices =
			{
				0, 1, 2,
				0, 2, 3,
			};

			OutDrawElements.PushClip(FSlateClippingZone{ ClippingRect });
			FSlateDrawElement::MakeCustomVerts(OutDrawElements, LayerId, m_ImGuiSlateBrush.GetRenderingResource(), Vertices, Indices, nullptr, 0, 0, ESlateDrawEffect::NoGamma);
			OutDrawElements.PopClip();
		}
	}

	return Super::OnPaint(Args, WidgetGeometry, ClippingRect, OutDrawElements, LayerId, WidgetStyle, bParentEnabled);
}

SImGuiWidgetBase::FImGuiTickResult SImGuiWidgetBase::TickImGui(const FGeometry* WidgetGeometry, FImGuiTickContext* TickContext)
{
	ImGuiIO& IO = GetImGuiIO();

	if (WidgetGeometry)
	{
		IO.DisplaySize.x = FMath::CeilToInt(WidgetGeometry->GetAbsoluteSize().X);
		IO.DisplaySize.y = FMath::CeilToInt(WidgetGeometry->GetAbsoluteSize().Y);
		
		// resize RT if needed
		{
			const int32 NewSizeX = FMath::Max(1, FMath::CeilToInt(IO.DisplaySize.x));
			const int32 NewSizeY = FMath::Max(1, FMath::CeilToInt(IO.DisplaySize.y));

			if (m_ImGuiRT->SizeX < NewSizeX || m_ImGuiRT->SizeY < NewSizeY)
			{
				m_ImGuiRT->ResizeTarget(NewSizeX, NewSizeY);
			}
		}
	}
	IO.DeltaTime = FSlateApplication::Get().GetDeltaTime();

	ImGui::NewFrame();

	if (TickContext->DragDropOperation.IsValid() && m_IsDragOverActive)
	{
		// disable widgets from reacting to mouse events (hover/tooltips etc)
		ImGui::SetActiveID(-1, nullptr);
	}

	TickImGuiInternal(TickContext);
	
	FImGuiTickResult TickResult{};
	TickResult.bWasDragOperationHandled = (TickContext->bApplyDragDropOperation && TickContext->bWasDragDropOperationHandled);
	return TickResult;
}

#pragma region SLATE_INPUT
FORCEINLINE static ImGuiKey GetImGuiKeyFromFKey(FName Keyname)
{
#define CONVERSION_OP(Key) { EKeys::Key.GetFName(), ImGuiKey_##Key }
#define CONVERSION_OP1(Key, ImGuiKeyId) { EKeys::Key.GetFName(), ImGuiKeyId }
	// not an exhaustive mapping, some keys are missing :^|
	static const TMap<FName, ImGuiKey> FKeyToImGuiKey =
	{
		CONVERSION_OP(A), CONVERSION_OP(B), CONVERSION_OP(C), CONVERSION_OP(D), CONVERSION_OP(E), CONVERSION_OP(F), CONVERSION_OP(G),
		CONVERSION_OP(H), CONVERSION_OP(I), CONVERSION_OP(J), CONVERSION_OP(K), CONVERSION_OP(L), CONVERSION_OP(M), CONVERSION_OP(N), CONVERSION_OP(O), CONVERSION_OP(P),
		CONVERSION_OP(Q), CONVERSION_OP(R), CONVERSION_OP(S), CONVERSION_OP(T), CONVERSION_OP(U), CONVERSION_OP(V),
		CONVERSION_OP(W), CONVERSION_OP(X), CONVERSION_OP(Y), CONVERSION_OP(Z),
		
		CONVERSION_OP(F1),
		CONVERSION_OP(F2),
		CONVERSION_OP(F3),
		CONVERSION_OP(F4),
		CONVERSION_OP(F5),
		CONVERSION_OP(F6),
		CONVERSION_OP(F7),
		CONVERSION_OP(F8),
		CONVERSION_OP(F9),
		CONVERSION_OP(F10),
		CONVERSION_OP(F11),
		CONVERSION_OP(F12),
		CONVERSION_OP(Enter),
		CONVERSION_OP(Insert),
		CONVERSION_OP(Delete),
		CONVERSION_OP(Escape),
		CONVERSION_OP(Tab),
		
		CONVERSION_OP(PageUp),
		CONVERSION_OP(PageDown),
		CONVERSION_OP(Home),
		CONVERSION_OP(End),
		CONVERSION_OP(NumLock),
		CONVERSION_OP(ScrollLock),
		CONVERSION_OP(CapsLock),
		CONVERSION_OP(RightBracket),
		CONVERSION_OP(LeftBracket),
		CONVERSION_OP(Backslash),
		CONVERSION_OP(Slash),
		CONVERSION_OP(Semicolon),
		CONVERSION_OP(Period),
		CONVERSION_OP(Comma),
		CONVERSION_OP(Apostrophe),
		CONVERSION_OP(Pause),
		
		CONVERSION_OP1(Zero, ImGuiKey_0),
		CONVERSION_OP1(One, ImGuiKey_1),
		CONVERSION_OP1(Two, ImGuiKey_2),
		CONVERSION_OP1(Three, ImGuiKey_3),
		CONVERSION_OP1(Four, ImGuiKey_4),
		CONVERSION_OP1(Five, ImGuiKey_5),
		CONVERSION_OP1(Six, ImGuiKey_6),
		CONVERSION_OP1(Seven, ImGuiKey_7),
		CONVERSION_OP1(Eight, ImGuiKey_8),
		CONVERSION_OP1(Nine, ImGuiKey_9),
		
		CONVERSION_OP1(NumPadZero, ImGuiKey_Keypad0),
		CONVERSION_OP1(NumPadOne, ImGuiKey_Keypad1),
		CONVERSION_OP1(NumPadTwo, ImGuiKey_Keypad2),
		CONVERSION_OP1(NumPadThree, ImGuiKey_Keypad3),
		CONVERSION_OP1(NumPadFour, ImGuiKey_Keypad4),
		CONVERSION_OP1(NumPadFive, ImGuiKey_Keypad5),
		CONVERSION_OP1(NumPadSix, ImGuiKey_Keypad6),
		CONVERSION_OP1(NumPadSeven, ImGuiKey_Keypad7),
		CONVERSION_OP1(NumPadEight, ImGuiKey_Keypad8),
		CONVERSION_OP1(NumPadNine, ImGuiKey_Keypad9),
		CONVERSION_OP1(LeftShift, ImGuiKey_LeftShift),
		CONVERSION_OP1(LeftControl, ImGuiKey_LeftCtrl),
		CONVERSION_OP1(LeftAlt, ImGuiKey_LeftAlt),
		CONVERSION_OP1(RightShift, ImGuiKey_RightShift),
		CONVERSION_OP1(RightControl, ImGuiKey_RightCtrl),
		CONVERSION_OP1(RightAlt, ImGuiKey_RightAlt),
		CONVERSION_OP1(SpaceBar, ImGuiKey_Space),
		CONVERSION_OP1(BackSpace, ImGuiKey_Backspace),
		CONVERSION_OP1(Up, ImGuiKey_UpArrow),
		CONVERSION_OP1(Down, ImGuiKey_DownArrow),
		CONVERSION_OP1(Left, ImGuiKey_LeftArrow),
		CONVERSION_OP1(Right, ImGuiKey_RightArrow),
		CONVERSION_OP1(Subtract, ImGuiKey_KeypadSubtract),
		CONVERSION_OP1(Add, ImGuiKey_KeypadAdd),
		CONVERSION_OP1(Multiply, ImGuiKey_KeypadMultiply),
		CONVERSION_OP1(Divide, ImGuiKey_KeypadDivide),
		CONVERSION_OP1(Decimal, ImGuiKey_KeypadDecimal),
		CONVERSION_OP1(Equals, ImGuiKey_Equal),
	};
#undef CONVERSION_OP1
#undef CONVERSION_OP

	/*
	TODO: These are not added....
	switch (KeyCode)
	{
	case VK_OEM_3: return ImGuiKey_GraveAccent;
	case VK_LWIN: return ImGuiKey_LeftSuper;
	case VK_RWIN: return ImGuiKey_RightSuper;
	case VK_APPS: return ImGuiKey_Menu;
	default: return ImGuiKey_None;
	}
	*/

	const ImGuiKey* Key = FKeyToImGuiKey.Find(Keyname);
	return Key ? *Key : ImGuiKey_None;
}

void SImGuiWidgetBase::AddMouseButtonEvent(ImGuiIO& IO, FKey MouseKey, bool IsDown)
{
	int32 MouseButton = ImGuiMouseButton_Left; //MouseKey == EKeys::LeftMouseButton
	if (MouseKey == EKeys::RightMouseButton)
	{
		MouseButton = ImGuiMouseButton_Right;
	}
	else if (MouseKey == EKeys::MiddleMouseButton)
	{
		MouseButton = ImGuiMouseButton_Middle;
	}

	IO.AddMouseButtonEvent(MouseButton, IsDown);
}

void SImGuiWidgetBase::AddKeyEvent(ImGuiIO& IO, FKeyEvent KeyEvent, bool IsDown)
{
	const ImGuiKey ImGuiKey = GetImGuiKeyFromFKey(KeyEvent.GetKey().GetFName());
	if (ImGuiKey != ImGuiKey_None)
	{
		IO.AddKeyEvent(ImGuiKey, IsDown);
	}

	IO.AddKeyEvent(ImGuiMod_Shift, KeyEvent.GetModifierKeys().IsShiftDown());
	IO.AddKeyEvent(ImGuiMod_Ctrl, KeyEvent.GetModifierKeys().IsControlDown());
	IO.AddKeyEvent(ImGuiMod_Alt, KeyEvent.GetModifierKeys().IsAltDown());
}

FReply SImGuiWidgetBase::OnKeyChar(const FGeometry& WidgetGeometry, const FCharacterEvent& CharacterEvent)
{
	ImGuiIO& IO = GetImGuiIO();
	IO.AddInputCharacterUTF16(CharacterEvent.GetCharacter());
	return IO.WantTextInput ? FReply::Handled() : FReply::Unhandled();
}

FReply SImGuiWidgetBase::OnKeyDown(const FGeometry& WidgetGeometry, const FKeyEvent& KeyEvent)
{
	ImGuiIO& IO = GetImGuiIO();
	AddKeyEvent(IO, KeyEvent, true);
	return IO.WantCaptureKeyboard ? FReply::Handled() : FReply::Unhandled();
}

FReply SImGuiWidgetBase::OnKeyUp(const FGeometry& WidgetGeometry, const FKeyEvent& KeyEvent)
{
	ImGuiIO& IO = GetImGuiIO();
	AddKeyEvent(IO, KeyEvent, false);
	return IO.WantCaptureKeyboard ? FReply::Handled() : FReply::Unhandled();
}

void SImGuiWidgetBase::OnMouseLeave(const FPointerEvent& MouseEvent)
{
	ImGuiIO& IO = GetImGuiIO();
	IO.AddMousePosEvent(-FLT_MAX, -FLT_MAX);
}

FReply SImGuiWidgetBase::OnMouseButtonDown(const FGeometry& WidgetGeometry, const FPointerEvent& MouseEvent)
{
	ImGuiIO& IO = GetImGuiIO();
	AddMouseButtonEvent(IO, MouseEvent.GetEffectingButton(), true);
	if (IO.WantCaptureMouse)
	{
		FSlateThrottleManager::Get().DisableThrottle(true);
		return FReply::Handled().CaptureMouse(SharedThis(this));
	}
	else
	{
		return FReply::Unhandled();
	}
}

FReply SImGuiWidgetBase::OnMouseButtonUp(const FGeometry& WidgetGeometry, const FPointerEvent& MouseEvent)
{
	ImGuiIO& IO = GetImGuiIO();
	AddMouseButtonEvent(IO, MouseEvent.GetEffectingButton(), false);

	if (HasMouseCapture())
	{
		FSlateThrottleManager::Get().DisableThrottle(false);
		return FReply::Handled().ReleaseMouseCapture();
	}
	else
	{
		return FReply::Unhandled();
	}
}

FReply SImGuiWidgetBase::OnMouseButtonDoubleClick(const FGeometry& WidgetGeometry, const FPointerEvent& MouseEvent)
{
	ImGuiIO& IO = GetImGuiIO();
	AddMouseButtonEvent(IO, MouseEvent.GetEffectingButton(), true);

	return FReply::Handled();
}

FReply SImGuiWidgetBase::OnMouseWheel(const FGeometry& WidgetGeometry, const FPointerEvent& MouseEvent)
{
	ImGuiIO& IO = GetImGuiIO();

	// TODO: initial zoom support, can we do better than this?
	if (IO.KeyCtrl)
	{
		m_WindowScale += MouseEvent.GetWheelDelta() * 0.25f;
		m_WindowScale = FMath::Clamp(m_WindowScale, 1.f, 4.f);
		ImGui::GetStyle().FontScaleMain = m_WindowScale;
	}
	else
	{
		IO.AddMouseWheelEvent(0.f, MouseEvent.GetWheelDelta());
	}

	return FReply::Handled();
}

FReply SImGuiWidgetBase::OnMouseMove(const FGeometry& WidgetGeometry, const FPointerEvent& MouseEvent)
{
	const FVector2D LocalMousePosition = WidgetGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());

	ImGuiIO& IO = GetImGuiIO();
	IO.AddMousePosEvent(LocalMousePosition.X * WidgetGeometry.Scale, LocalMousePosition.Y * WidgetGeometry.Scale);

	return FReply::Handled();
}

FCursorReply SImGuiWidgetBase::OnCursorQuery(const FGeometry& WidgetGeometry, const FPointerEvent& CursorEvent) const
{
	static constexpr EMouseCursor::Type ImGuiToUMGCursor[ImGuiMouseCursor_COUNT] =
	{
		EMouseCursor::Default,
		EMouseCursor::TextEditBeam,
		EMouseCursor::CardinalCross,
		EMouseCursor::ResizeUpDown,
		EMouseCursor::ResizeLeftRight,
		EMouseCursor::ResizeSouthWest,
		EMouseCursor::ResizeSouthEast,
		EMouseCursor::Hand,
		EMouseCursor::SlashedCircle,
	};

	ImGuiIO& IO = GetImGuiIO();

	const ImGuiMouseCursor MouseCursor = ImGui::GetMouseCursor();
	return (MouseCursor == ImGuiMouseCursor_None) ? FCursorReply::Unhandled() : FCursorReply::Cursor(ImGuiToUMGCursor[MouseCursor]);
}

void SImGuiWidgetBase::OnDragLeave(const FDragDropEvent& DragDropEvent)
{
	m_IsDragOverActive = false;

	ImGuiIO& IO = GetImGuiIO();
	IO.AddMousePosEvent(-FLT_MAX, -FLT_MAX);
}

FReply SImGuiWidgetBase::OnDragOver(const FGeometry& WidgetGeometry, const FDragDropEvent& DragDropEvent)
{
	m_IsDragOverActive = true;

	if (m_ImGuiTickedByInputProcessing)
	{
		// dummy ImGui render in case slate widget was throttled
		ImGuiIO& IO = GetImGuiIO();
		ImGui::Render();

		m_ImGuiTickedByInputProcessing = false;
	}

	ImGuiIO& IO = GetImGuiIO();
	const FVector2D LocalMousePosition = WidgetGeometry.AbsoluteToLocal(DragDropEvent.GetScreenSpacePosition());
	IO.AddMousePosEvent(LocalMousePosition.X * WidgetGeometry.Scale, LocalMousePosition.Y * WidgetGeometry.Scale);

	FImGuiTickContext TickContext{};
	TickContext.ImGuiContext = m_ImGuiContext;
	TickContext.DragDropOperation = DragDropEvent.GetOperation();
	FImGuiTickResult TickResult = TickImGui(&WidgetGeometry, &TickContext);

	m_ImGuiTickedByInputProcessing = true;

	return FReply::Handled();
}

FReply SImGuiWidgetBase::OnDrop(const FGeometry& WidgetGeometry, const FDragDropEvent& DragDropEvent)
{
	m_IsDragOverActive = false;

	if (m_ImGuiTickedByInputProcessing)
	{
		// dummy ImGui render in case slate widget was throttled
		ImGuiIO& IO = GetImGuiIO();
		ImGui::Render();

		m_ImGuiTickedByInputProcessing = false;
	}

	FImGuiTickContext TickContext{};
	TickContext.ImGuiContext = m_ImGuiContext;
	TickContext.DragDropOperation = DragDropEvent.GetOperation();
	TickContext.bApplyDragDropOperation = TickContext.DragDropOperation.IsValid();
	FImGuiTickResult TickResult = TickImGui(&WidgetGeometry, &TickContext);

	m_ImGuiTickedByInputProcessing = true;

	return TickResult.bWasDragOperationHandled ? FReply::Handled() : FReply::Unhandled();
}
#pragma endregion SLATE_INPUT

void SImGuiMainWindowWidget::Construct(const FArguments& InArgs)
{
	Super::Construct(InArgs, /*UseTranslucentBackground=*/false);
}

void SImGuiMainWindowWidget::TickImGuiInternal(FImGuiTickContext* TickContext)
{
	UImGuiSubsystem* ImGuiSubsystem = UImGuiSubsystem::Get();
	if (ImGuiSubsystem->GetMainWindowTickDelegate().IsBound())
	{
		const ImU32 BackgroundColor = m_ClearRenderTargetEveryFrame ? ImGui::GetColorU32(ImGuiCol_WindowBg) : (ImGui::GetColorU32(ImGuiCol_WindowBg) | 0xFF000000);
		ImGui::PushStyleColor(ImGuiCol_WindowBg, BackgroundColor);
		ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport());
		ImGui::PopStyleColor(1);
		ImGuiSubsystem->GetMainWindowTickDelegate().Broadcast(TickContext);
	}
	else
	{
		const ImU32 BackgroundColor = m_ClearRenderTargetEveryFrame ? ImGui::GetColorU32(ImGuiCol_WindowBg) : (ImGui::GetColorU32(ImGuiCol_WindowBg) | 0xFF000000);
		ImGui::PushStyleColor(ImGuiCol_WindowBg, BackgroundColor);
		const ImGuiID MainDockSpaceID = ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport());
		ImGui::PopStyleColor(1);

		ImGui::SetNextWindowDockID(MainDockSpaceID);
		if (ImGui::Begin("Empty", nullptr))
		{
			ImGui::Text("Nothing to display...");
		}
		ImGui::End();
	}
}

void SImGuiWidget::Construct(const FArguments& InArgs)
{
	Super::Construct(Super::FArguments(), /*UseTranslucentBackground=*/true);

	m_OnTickDelegate = InArgs._OnTickDelegate;
}

void SImGuiWidget::TickImGuiInternal(FImGuiTickContext* TickContext)
{
	m_OnTickDelegate.ExecuteIfBound(TickContext);
}
