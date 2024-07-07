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
#include "Engine/TextureRenderTarget2D.h"

#include "Input/Events.h"
#include "InputCoreTypes.h"
#include "Application/ThrottleManager.h"

#include "RenderCaptureInterface.h"

DECLARE_CYCLE_STAT(TEXT("ImGui Tick"), STAT_TickWidget, STATGROUP_ImGui);
DECLARE_CYCLE_STAT(TEXT("ImGui Render"), STAT_RenderWidget, STATGROUP_ImGui);

void SImGuiWidgetBase::Construct(const FArguments& InArgs)
{
	UImGuiSubsystem* ImGuiSubsystem = UImGuiSubsystem::Get();

	m_ImGuiContext = ImGui::CreateContext(ImGuiSubsystem->GetDefaultImGuiFontAtlas());

	ImGuiIO& io = GetImGuiIO();
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
	//io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;

	//[TODO] setting?
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

//~ GCObject Interface
void SImGuiWidgetBase::AddReferencedObjects(FReferenceCollector& Collector)
{
	Collector.AddReferencedObject(m_ImGuiRT);
}

FString SImGuiWidgetBase::GetReferencerName() const
{
	return TEXT("SImGuiWidgetBase");
}
//~ GCObject Interface

void SImGuiWidgetBase::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	SCOPE_CYCLE_COUNTER(STAT_TickWidget);

	Super::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);
	
	ImGuiIO& IO = GetImGuiIO();
	
	// new frame setup
	{
		IO.DisplaySize = { (float)AllottedGeometry.GetAbsoluteSize().X, (float)AllottedGeometry.GetAbsoluteSize().Y };
		IO.DeltaTime = InDeltaTime;

		ImGui::NewFrame();
	}

	// resize RT if needed
	{
		const int32 NewSizeX = FMath::Max(1, FMath::CeilToInt(IO.DisplaySize.x));
		const int32 NewSizeY = FMath::Max(1, FMath::CeilToInt(IO.DisplaySize.y));

		if (m_ImGuiRT->SizeX < NewSizeX || m_ImGuiRT->SizeY < NewSizeY)
		{
			m_ImGuiRT->ResizeTarget(NewSizeX, NewSizeY);
		}
	}

	TickInternal(AllottedGeometry, InCurrentTime, InDeltaTime);
}

int32 SImGuiWidgetBase::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyClippingRect,
	FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& WidgetStyle, bool bParentEnabled) const
{
	SCOPE_CYCLE_COUNTER(STAT_RenderWidget);

	ImGuiIO& IO = GetImGuiIO();

	ImGui::Render();

	ImDrawData* DrawData = ImGui::GetDrawData();
	if (DrawData->DisplaySize.x > 0.0f && DrawData->DisplaySize.y > 0.0f)
	{
		if (DrawData->TotalVtxCount > 0 && DrawData->TotalIdxCount > 0)
		{
			UImGuiSubsystem* ImGuiSubsystem = UImGuiSubsystem::Get();
			
			RenderCaptureInterface::FScopedCapture RenderCapture(ImGuiSubsystem->CaptureGpuFrame(), TEXT("ImGuiWidget"));

			struct FRenderData
			{
				struct FDrawListDeleter
				{
					void operator()(ImDrawList* Ptr) { IM_FREE(Ptr); }
				};
				using FDrawListPtr = TUniquePtr<ImDrawList, FDrawListDeleter>;
				struct FTextureInfo
				{
					FTextureRHIRef TextureRHI = nullptr;
					FSamplerStateRHIRef SamplerRHI = nullptr;
					bool IsSRGB = false;
				};
				TArray<FDrawListPtr> DrawLists;
				TArray<FTextureInfo> BoundTextures;
				ImVec2 DisplayPos = {};
				ImVec2 DisplaySize = {};
				int32 TotalVtxCount = 0;
				int32 TotalIdxCount = 0;
				int32 MissingTextureIndex = INDEX_NONE;

				FRenderData() = default;
				FRenderData(const FRenderData&) = delete;
				FRenderData(FRenderData&&) = delete;
				FRenderData& operator=(const FRenderData&) = delete;
				FRenderData& operator=(FRenderData&&) = delete;					
				~FRenderData() = default;
			};
			FRenderData* RenderData = new FRenderData();
			RenderData->DisplayPos = DrawData->DisplayPos;
			RenderData->DisplaySize = DrawData->DisplaySize;
			RenderData->TotalVtxCount = DrawData->TotalVtxCount;
			RenderData->TotalIdxCount = DrawData->TotalIdxCount;
			RenderData->DrawLists.SetNum(DrawData->CmdListsCount);
			for (int32 ListIndex = 0; ListIndex < DrawData->CmdListsCount; ++ListIndex)
			{
				//[TODO] Clone allocates from heap, might be worth keeping some static buffer around and manually copying...
				RenderData->DrawLists[ListIndex] = FRenderData::FDrawListPtr(DrawData->CmdLists[ListIndex]->CloneOutput());
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

			ENQUEUE_RENDER_COMMAND(RenderImGui)(
				[RenderData, RT_Resource = m_ImGuiRT->GetResource()](FRHICommandListImmediate& RHICmdList)
				{
					FRHIResourceCreateInfo VertexCreateInfo(TEXT("ImGui_VertexBuffer"));
					FBufferRHIRef VertexBuffer = RHICmdList.CreateVertexBuffer(RenderData->TotalVtxCount * sizeof(ImDrawVert), BUF_Volatile | BUF_ByteAddressBuffer | BUF_ShaderResource, VertexCreateInfo);
					if (ImDrawVert* VertexDst = (ImDrawVert*)RHICmdList.LockBuffer(VertexBuffer, 0, RenderData->TotalVtxCount * sizeof(ImDrawVert), RLM_WriteOnly))
					{
						for (FRenderData::FDrawListPtr& CmdList : RenderData->DrawLists)
						{
							FMemory::Memcpy(VertexDst, CmdList->VtxBuffer.Data, CmdList->VtxBuffer.Size * sizeof(ImDrawVert));
							VertexDst += CmdList->VtxBuffer.Size;
						}
						RHICmdList.UnlockBuffer(VertexBuffer);
					}
					FShaderResourceViewRHIRef VertexBufferSRV = RHICmdList.CreateShaderResourceView(VertexBuffer, sizeof(uint32), PF_R32_UINT);

					FRHIResourceCreateInfo IndexCreateInfo(TEXT("ImGui_IndexBuffer"));
					FBufferRHIRef IndexBuffer = RHICmdList.CreateIndexBuffer(sizeof(ImDrawIdx), RenderData->TotalIdxCount * sizeof(ImDrawIdx), BUF_Volatile, IndexCreateInfo);
					if (ImDrawIdx* IndexDst = (ImDrawIdx*)RHICmdList.LockBuffer(IndexBuffer, 0, RenderData->TotalIdxCount * sizeof(ImDrawIdx), RLM_WriteOnly))
					{
						for (FRenderData::FDrawListPtr& CmdList : RenderData->DrawLists)
						{
							FMemory::Memcpy(IndexDst, CmdList->IdxBuffer.Data, CmdList->IdxBuffer.Size * sizeof(ImDrawIdx));
							IndexDst += CmdList->IdxBuffer.Size;
						}
						RHICmdList.UnlockBuffer(IndexBuffer);
					}

					FRHIRenderPassInfo RPInfo(RT_Resource->TextureRHI, MakeRenderTargetActions(ERenderTargetLoadAction::EClear, ERenderTargetStoreAction::EStore));
					RHICmdList.Transition(FRHITransitionInfo(RT_Resource->TextureRHI, ERHIAccess::Unknown, ERHIAccess::RTV));
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
						GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GEmptyVertexDeclaration.VertexDeclarationRHI;
						GraphicsPSOInit.BoundShaderState.VertexShaderRHI = VertexShader.GetVertexShader();
						GraphicsPSOInit.BoundShaderState.PixelShaderRHI = PixelShader.GetPixelShader();
						GraphicsPSOInit.PrimitiveType = PT_TriangleList;
						SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit, 0);

						auto CalculateProjectionMatrix = [&]() -> FMatrix44f
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
							return ProjectionMatrix;
						};

						RHICmdList.SetViewport(0.f, 0.f, 0.f, RenderData->DisplaySize.x, RenderData->DisplaySize.y, 1.f);

						FMatrix44f ProjectionMatrix = CalculateProjectionMatrix();
						uint32_t RenderStateOverrides = 0;

						// since we merged the vertex and index buffers, we need to track global offset
						uint32 GlobalVertexOffset = 0;
						uint32 GlobalIndexOffset = 0;
						for (FRenderData::FDrawListPtr& CmdList : RenderData->DrawLists)
						{
							for (const ImDrawCmd& DrawCmd : CmdList->CmdBuffer)
							{
								if (DrawCmd.UserCallback != NULL)
								{
									if (DrawCmd.UserCallback == ImDrawCallback_ResetRenderState)
									{
										RHICmdList.SetViewport(0.f, 0.f, 0.f, RenderData->DisplaySize.x, RenderData->DisplaySize.y, 1.f);

										ProjectionMatrix = CalculateProjectionMatrix();
										RenderStateOverrides = 0;
									}
									else if (DrawCmd.UserCallback == ImDrawCallback_SetRenderState)
									{
										RenderStateOverrides = static_cast<uint32>(reinterpret_cast<uintptr_t>(DrawCmd.UserCallbackData));
									}
									else
									{
										DrawCmd.UserCallback(CmdList.Get(), &DrawCmd);
									}
								}
								else
								{
									const ImVec2& ClipRectOffset = RenderData->DisplayPos;
									RHICmdList.SetScissorRect(
										true,
										DrawCmd.ClipRect.x - ClipRectOffset.x,
										DrawCmd.ClipRect.y - ClipRectOffset.y,
										DrawCmd.ClipRect.z - ClipRectOffset.x,
										DrawCmd.ClipRect.w - ClipRectOffset.y);
									
									uint32 TextureIndex = UImGuiSubsystem::ImGuiIDToIndex(DrawCmd.TextureId);
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

									SetShaderParametersLegacyVS(
										RHICmdList,
										VertexShader,
										VertexBufferSRV,
										ProjectionMatrix,
										DrawCmd.VtxOffset + GlobalVertexOffset);

									RHICmdList.DrawIndexedPrimitive(IndexBuffer, DrawCmd.VtxOffset + GlobalVertexOffset, 0, DrawCmd.ElemCount, DrawCmd.IdxOffset + GlobalIndexOffset, DrawCmd.ElemCount / 3, 1);
								}
							}
							GlobalIndexOffset += CmdList->IdxBuffer.Size;
							GlobalVertexOffset += CmdList->VtxBuffer.Size;
						}

						delete RenderData;
					}
					RHICmdList.EndRenderPass();
					RHICmdList.Transition(FRHITransitionInfo(RT_Resource->TextureRHI, ERHIAccess::RTV, ERHIAccess::SRVMask));
				}
			);

			const FSlateRenderTransform WidgetOffsetTransform = FTransform2f(1.f, { 0.f, 0.f });
			const FSlateRect DrawRect = AllottedGeometry.GetRenderBoundingRect();

			const FVector2f V0 = (FVector2f)DrawRect.GetTopLeft();
			const FVector2f V1 = (FVector2f)DrawRect.GetTopRight();
			const FVector2f V2 = (FVector2f)DrawRect.GetBottomRight();
			const FVector2f V3 = (FVector2f)DrawRect.GetBottomLeft();

			// adjust UVs based on RT vs Viewport scaling
			const float UVMinX = 0.f;
			const float UVMinY = 0.f;
			const float UVMaxX = (float)(DrawRect.Right - DrawRect.Left) / (float)m_ImGuiRT->SizeX;
			const float UVMaxY = (float)(DrawRect.Bottom - DrawRect.Top) / (float)m_ImGuiRT->SizeY;

			const TArray<FSlateVertex> Vertices =
			{
				FSlateVertex::Make<ESlateVertexRounding::Disabled>(WidgetOffsetTransform, V0, FVector2f{ UVMinX, UVMinY }, FColor::White),
				FSlateVertex::Make<ESlateVertexRounding::Disabled>(WidgetOffsetTransform, V1, FVector2f{ UVMaxX, UVMinY }, FColor::White),
				FSlateVertex::Make<ESlateVertexRounding::Disabled>(WidgetOffsetTransform, V2, FVector2f{ UVMaxX, UVMaxY }, FColor::White),
				FSlateVertex::Make<ESlateVertexRounding::Disabled>(WidgetOffsetTransform, V3, FVector2f{ UVMinX, UVMaxY }, FColor::White)
			};
			const TArray<uint32> Indices =
			{
				0, 1, 2,
				0, 2, 3,
			};

			OutDrawElements.PushClip(FSlateClippingZone{ MyClippingRect });
			FSlateDrawElement::MakeCustomVerts(OutDrawElements, LayerId, m_ImGuiSlateBrush.GetRenderingResource(), Vertices, Indices, nullptr, 0, 0, ESlateDrawEffect::NoGamma);
			OutDrawElements.PopClip();
		}
	}

	return Super::OnPaint(Args, AllottedGeometry, MyClippingRect, OutDrawElements, LayerId, WidgetStyle, bParentEnabled);
}

#pragma region SLATE_INPUT
static ImGuiKey FKeyToImGuiKey(FName Keyname)
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
	[TODO] These are not added....
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

static int32 FMouseKeyToImGuiKey(FKey MouseKey)
{
	int32 MouseButton = 0;
	if (MouseKey == EKeys::LeftMouseButton)
	{
		MouseButton = 0;
	}
	else if (MouseKey == EKeys::RightMouseButton)
	{
		MouseButton = 1;
	}
	else if (MouseKey == EKeys::MiddleMouseButton)
	{
		MouseButton = 2;
	}
	return MouseButton;
}

void SImGuiWidgetBase::AddMouseButtonEvent(ImGuiIO& IO, FKey MouseKey, bool IsDown)
{
	IO.AddMouseButtonEvent(FMouseKeyToImGuiKey(MouseKey), IsDown);
}

void SImGuiWidgetBase::AddKeyEvent(ImGuiIO& IO, FKeyEvent KeyEvent, bool IsDown)
{
	const ImGuiKey ImGuiKey = FKeyToImGuiKey(KeyEvent.GetKey().GetFName());
	if (ImGuiKey != ImGuiKey_None)
	{
		IO.AddKeyEvent(ImGuiKey, IsDown);
	}

	IO.AddKeyEvent(ImGuiKey_ModShift, KeyEvent.GetModifierKeys().IsShiftDown());
	IO.AddKeyEvent(ImGuiKey_ModCtrl, KeyEvent.GetModifierKeys().IsControlDown());
	IO.AddKeyEvent(ImGuiKey_ModAlt, KeyEvent.GetModifierKeys().IsAltDown());
}

FReply SImGuiWidgetBase::OnKeyChar(const FGeometry& MyGeometry, const FCharacterEvent& CharacterEvent)
{
	ImGuiIO& IO = GetImGuiIO();
	IO.AddInputCharacterUTF16(CharacterEvent.GetCharacter());

	if (IO.WantTextInput)
	{
		return FReply::Handled();
	}
	else
	{
		return FReply::Unhandled();
	}
}

FReply SImGuiWidgetBase::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& KeyEvent)
{
	ImGuiIO& IO = GetImGuiIO();
	AddKeyEvent(IO, KeyEvent, true);
	if (IO.WantCaptureKeyboard)
	{
		return FReply::Handled();
	}
	else
	{
		return FReply::Unhandled();
	}
}

FReply SImGuiWidgetBase::OnKeyUp(const FGeometry& MyGeometry, const FKeyEvent& KeyEvent)
{
	ImGuiIO& IO = GetImGuiIO();
	AddKeyEvent(IO, KeyEvent, false);
	if (IO.WantCaptureKeyboard)
	{
		return FReply::Handled();
	}
	else
	{
		return FReply::Unhandled();
	}
}

FReply SImGuiWidgetBase::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
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

FReply SImGuiWidgetBase::OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
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

FReply SImGuiWidgetBase::OnMouseButtonDoubleClick(const FGeometry& InMyGeometry, const FPointerEvent& MouseEvent)
{
	ImGuiIO& IO = GetImGuiIO();
	AddMouseButtonEvent(IO, MouseEvent.GetEffectingButton(), true);

	return FReply::Handled();
}

FReply SImGuiWidgetBase::OnMouseWheel(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	ImGuiIO& IO = GetImGuiIO();
	IO.AddMouseWheelEvent(0.f, MouseEvent.GetWheelDelta());

	// TODO: initial zoom support, can we do better than this?
	if (IO.KeyCtrl)
	{
		m_WindowScale += MouseEvent.GetWheelDelta() * 0.25f;
		m_WindowScale = FMath::Clamp(m_WindowScale, 1.f, 4.f);
		IO.FontGlobalScale = m_WindowScale;
	}

	return FReply::Handled();
}

FReply SImGuiWidgetBase::OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	const FVector2D LocalMousePosition = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());

	ImGuiIO& IO = GetImGuiIO();
	IO.AddMousePosEvent(LocalMousePosition.X * MyGeometry.Scale, LocalMousePosition.Y * MyGeometry.Scale);

	return FReply::Handled();
}

FCursorReply SImGuiWidgetBase::OnCursorQuery(const FGeometry& MyGeometry, const FPointerEvent& CursorEvent) const
{
	const static EMouseCursor::Type ImGuiToUMGCursor[] =
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
	return MouseCursor == ImGuiMouseCursor_None ? FCursorReply::Unhandled() : FCursorReply::Cursor(ImGuiToUMGCursor[MouseCursor]);
}

#pragma endregion SLATE_INPUT

void SImGuiMainWindowWidget::TickInternal(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	UImGuiSubsystem* ImGuiSubsystem = UImGuiSubsystem::Get();
	if (ImGuiSubsystem->GetMainWindowTickDelegate().IsBound())
	{
		ImGui::DockSpaceOverViewport(ImGui::GetMainViewport());
		ImGuiSubsystem->GetMainWindowTickDelegate().Broadcast(m_ImGuiContext);
	}
	else
	{
		const ImGuiID MainDockSpaceID = ImGui::DockSpaceOverViewport(ImGui::GetMainViewport());

		ImGui::SetNextWindowDockID(MainDockSpaceID);
		if (ImGui::Begin("Empty", nullptr))
		{
			ImGui::Text("Nothing to display...");
			ImGui::End();
		}
	}
}

void SImGuiWidget::Construct(const FArguments& InArgs)
{
	Super::Construct(Super::FArguments());

	m_OnTickDelegate = InArgs._OnTickDelegate;
	m_AllowUndocking = InArgs._AllowUndocking.Get();
}

void SImGuiWidget::TickInternal(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	const ImGuiID MainDockSpaceID = ImGui::DockSpaceOverViewport(ImGui::GetMainViewport(), ImGuiDockNodeFlags_PassthruCentralNode);
	if (!m_AllowUndocking)
	{
		ImGui::SetNextWindowDockID(MainDockSpaceID);
	}

	m_OnTickDelegate.ExecuteIfBound(m_ImGuiContext);
}
