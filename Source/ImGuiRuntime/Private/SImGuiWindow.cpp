#include "SImGuiWindow.h"
#include "ImGuiShaders.h"
#include "ImGuiRuntimeModule.h"

#include "RHI.h"
#include "Engine/Texture2D.h"
#include "RenderGraphUtils.h"
#include "CommonRenderResources.h"
#include "SlateUTextureResource.h"
#include "Engine/TextureRenderTarget2D.h"

#include "Input/Events.h"
#include "InputCoreTypes.h"

DECLARE_CYCLE_STAT(TEXT("ImGui Tick"), STAT_TickWidget, STATGROUP_ImGui);
DECLARE_CYCLE_STAT(TEXT("ImGui Render"), STAT_RenderWidget, STATGROUP_ImGui);

void SImGuiWindow::Construct(const FArguments& InArgs)
{
	m_ImGuiContext = ImGui::CreateContext();

	ImGuiIO& io = GetImGuiIO();
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
	//io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
	//io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;

	//[TODO] setting?
	ImGui::StyleColorsDark();

	//[TODO] not using Imgui font, we use texture asset instead
	io.Fonts->Build();

	FImGuiRuntimeModule& ImGuiRuntimeModule = FModuleManager::GetModuleChecked<FImGuiRuntimeModule>("ImGuiRuntime");
	io.Fonts->TexID = ImGuiRuntimeModule.GetDefaultFontTextureID();

	// allocate rendering resources
	m_ImGuiRT = NewObject<UTextureRenderTarget2D>();
	m_ImGuiRT->bCanCreateUAV = false;
	m_ImGuiRT->bAutoGenerateMips = false;
	m_ImGuiRT->RenderTargetFormat = ETextureRenderTargetFormat::RTF_RGBA8;
	m_ImGuiRT->ClearColor = FLinearColor(0.f, 0.f, 0.f, 0.f);
	m_ImGuiRT->InitAutoFormat(32, 32);
	m_ImGuiRT->UpdateResourceImmediate(true);

	m_ImGuiSlateBrush.SetResourceObject(m_ImGuiRT);
}

SImGuiWindow::~SImGuiWindow()
{
	if (m_ImGuiContext)
	{
		ImGui::DestroyContext(m_ImGuiContext);
		m_ImGuiContext = nullptr;
	}
}

//~ GCObject Interface
void SImGuiWindow::AddReferencedObjects(FReferenceCollector& Collector)
{
	Collector.AddReferencedObject(m_ImGuiRT);
}

FString SImGuiWindow::GetReferencerName() const
{
	return TEXT("SImGuiWindow");
}
//~ GCObject Interface

void SImGuiWindow::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	SCOPE_CYCLE_COUNTER(STAT_TickWidget);

	Super::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);
	
	ImGuiIO& IO = GetImGuiIO();
	
	//[TODO] our RT size is in int and Viewport size is in float, can sometimes cause blurring. set imgui framebuffer scale...?

	// new frame setup
	{
		IO.DisplaySize = { (float)AllottedGeometry.GetAbsoluteSize().X, (float)AllottedGeometry.GetAbsoluteSize().Y };
		IO.DeltaTime = InDeltaTime;		

		ImGui::NewFrame();
	}

	// resize RT if needed
	{
		const int32 NewSizeX = (int32)AllottedGeometry.GetAbsoluteSize().X;
		const int32 NewSizeY = (int32)AllottedGeometry.GetAbsoluteSize().Y;

		//TODO: ideally should only resize if NewSize > CurrentSize
		if (m_ImGuiRT->SizeX != NewSizeX || m_ImGuiRT->SizeY != NewSizeY)
		{
			m_ImGuiRT->ResizeTarget(NewSizeX, NewSizeY);
		}
	}

	TickInternal(AllottedGeometry, InCurrentTime, InDeltaTime);
}

int32 SImGuiWindow::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyClippingRect,
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
			FImGuiRuntimeModule& ImGuiRuntimeModule = FModuleManager::GetModuleChecked<FImGuiRuntimeModule>("ImGuiRuntime");
		
			struct FRenderData
			{
				struct FDrawListDeleter
				{
					void operator()(ImDrawList* Ptr) { IM_FREE(Ptr); }
				};
				using FDrawListPtr = TUniquePtr<ImDrawList, FDrawListDeleter>;

				TArray<FDrawListPtr> DrawLists;
				TArray<FTextureRHIRef> BoundTextures;
				ImVec2 DisplayPos = {};
				ImVec2 DisplaySize = {};
				int32 TotalVtxCount = 0;
				int32 TotalIdxCount = 0;

				FRenderData() = default;
				FRenderData(const FRenderData&) = delete;
				FRenderData(FRenderData&&) = delete;
					
				~FRenderData() = default;

				FRenderData& operator=(const FRenderData&) = delete;
				FRenderData& operator=(FRenderData&&) = delete;
			};
			FRenderData* RenderData = new FRenderData();
			RenderData->DisplayPos = DrawData->DisplayPos;
			RenderData->DisplaySize = DrawData->DisplaySize;
			RenderData->TotalVtxCount = DrawData->TotalVtxCount;
			RenderData->TotalIdxCount = DrawData->TotalIdxCount;
			RenderData->DrawLists.SetNum(DrawData->CmdListsCount);
			for (int32 ListIndex = 0; ListIndex < DrawData->CmdListsCount; ++ListIndex)
			{
				RenderData->DrawLists[ListIndex] = FRenderData::FDrawListPtr(DrawData->CmdLists[ListIndex]->CloneOutput());
			}

			RenderData->BoundTextures.Reserve(ImGuiRuntimeModule.GetResourceHandles().Num());
			for (const FSlateResourceHandle& ResourceHandle : ImGuiRuntimeModule.GetResourceHandles())
			{
				FTextureRHIRef TextureRHI = nullptr;

				if (ResourceHandle.GetResourceProxy() && ResourceHandle.GetResourceProxy()->Resource)
				{
					FSlateShaderResource* ShaderResource = ResourceHandle.GetResourceProxy()->Resource;
					ESlateShaderResource::Type Type = ShaderResource->GetType();

					if (Type == ESlateShaderResource::Type::TextureObject)
					{
						FSlateBaseUTextureResource* TextureObjectResource = static_cast<FSlateBaseUTextureResource*>(ShaderResource);
						if (FTextureResource* Resource = TextureObjectResource->GetTextureObject()->GetResource())
						{
							TextureRHI = Resource->TextureRHI;
						}
					}
					else if (Type == ESlateShaderResource::Type::NativeTexture)
					{
						if (FRHITexture* NativeTextureRHI = ((TSlateTexture<FTexture2DRHIRef>*)ShaderResource)->GetTypedResource())
						{
							TextureRHI = NativeTextureRHI;
						}
					}
				}

				RenderData->BoundTextures.Add(TextureRHI.IsValid() ? TextureRHI : GBlackTexture->TextureRHI);
			}

			ENQUEUE_RENDER_COMMAND(RenderImGui)(
				[RenderData, RT_Resource = m_ImGuiRT->GetResource()](FRHICommandListImmediate& RHICmdList)
				{
					FRHIResourceCreateInfo VertexCreateInfo(TEXT("ImGui_VertexBuffer"));
					FBufferRHIRef VertexBuffer = RHICreateIndexBuffer(sizeof(uint32), RenderData->TotalVtxCount * sizeof(ImDrawVert), BUF_Static|BUF_ByteAddressBuffer|BUF_ShaderResource, VertexCreateInfo);
					if (ImDrawVert* VertexDst = (ImDrawVert*)RHILockBuffer(VertexBuffer, 0, RenderData->TotalVtxCount * sizeof(ImDrawVert), RLM_WriteOnly))
					{
						for (FRenderData::FDrawListPtr& CmdList : RenderData->DrawLists)
						{
							FMemory::Memcpy(VertexDst, CmdList->VtxBuffer.Data, CmdList->VtxBuffer.Size * sizeof(ImDrawVert));
							VertexDst += CmdList->VtxBuffer.Size;
						}
						RHIUnlockBuffer(VertexBuffer);
					}
					FShaderResourceViewRHIRef VertexBufferSRV = RHICreateShaderResourceView(VertexBuffer, sizeof(uint32), PF_R32_UINT);

					FRHIResourceCreateInfo IndexCreateInfo(TEXT("ImGui_IndexBuffer"));
					FBufferRHIRef IndexBuffer = RHICreateIndexBuffer(sizeof(ImDrawIdx), RenderData->TotalIdxCount * sizeof(ImDrawIdx), BUF_Static, IndexCreateInfo);
					if (ImDrawIdx* IndexDst = (ImDrawIdx*)RHILockBuffer(IndexBuffer, 0, RenderData->TotalIdxCount * sizeof(ImDrawIdx), RLM_WriteOnly))
					{
						for (FRenderData::FDrawListPtr& CmdList : RenderData->DrawLists)
						{
							FMemory::Memcpy(IndexDst, CmdList->IdxBuffer.Data, CmdList->IdxBuffer.Size * sizeof(ImDrawIdx));
							IndexDst += CmdList->IdxBuffer.Size;
						}
						RHIUnlockBuffer(IndexBuffer);
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

						auto SetViewportState = [&]()
						{
							const float L = RenderData->DisplayPos.x;
							const float R = RenderData->DisplayPos.x + RenderData->DisplaySize.x;
							const float T = RenderData->DisplayPos.y;
							const float B = RenderData->DisplayPos.y + RenderData->DisplaySize.y;
							const FMatrix44f ProjectionMatrix =
							{
								{ 2.0f / (R - L)	, 0.0f			   , 0.0f, 0.0f },
								{ 0.0f				, 2.0f / (T - B)   , 0.0f, 0.0f },
								{ 0.0f				, 0.0f			   , 0.5f, 0.0f },
								{ (R + L) / (L - R)	, (T + B) / (B - T), 0.5f, 1.0f },
							};

							RHICmdList.SetViewport(0.f, 0.f, 0.f, RenderData->DisplaySize.x, RenderData->DisplaySize.y, 1.f);
							VertexShader->SetProjectionMatrix(RHICmdList, ProjectionMatrix);
						};

						SetViewportState();

						VertexShader->SetVertexBuffer(RHICmdList, VertexBufferSRV);
						PixelShader->SetTextureSampler(RHICmdList, TStaticSamplerState<SF_Bilinear, AM_Wrap, AM_Wrap, AM_Wrap>::GetRHI());

						const ImVec2 ClipRectOffset = RenderData->DisplayPos;

						// since we merged the vertex and index buffers, we need to track global offset
						uint32 GlobalVertexOffset = 0;
						uint32 GlobalIndexOffset = 0;
						for (FRenderData::FDrawListPtr& CmdList : RenderData->DrawLists)
						{
							for (const ImDrawCmd& DrawCmd : CmdList->CmdBuffer)
							{
								if (DrawCmd.UserCallback != NULL)
								{
									// User callback, registered via ImDrawList::AddCallback()
									// (ImDrawCallback_ResetRenderState is a special callback value used by the user to request the renderer to reset render state.)
									if (DrawCmd.UserCallback == ImDrawCallback_ResetRenderState)
									{
										SetViewportState();
									}
									else
									{
										DrawCmd.UserCallback(CmdList.Get(), &DrawCmd);
									}
								}
								else
								{
									RHICmdList.SetScissorRect(
										true,
										DrawCmd.ClipRect.x - ClipRectOffset.x,
										DrawCmd.ClipRect.y - ClipRectOffset.y,
										DrawCmd.ClipRect.z - ClipRectOffset.x,
										DrawCmd.ClipRect.w - ClipRectOffset.y);

									VertexShader->SetVertexOffset(RHICmdList, DrawCmd.VtxOffset + GlobalVertexOffset);
										
									const uint32 Index = FImGuiRuntimeModule::ImGuiIDToIndex(DrawCmd.TextureId);
									check(RenderData->BoundTextures.IsValidIndex(Index) && RenderData->BoundTextures[Index].IsValid());
									PixelShader->SetTexture(RHICmdList, RenderData->BoundTextures[Index]);

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

			const FVector2D V0 = DrawRect.GetTopLeft();
			const FVector2D V1 = DrawRect.GetTopRight();
			const FVector2D V2 = DrawRect.GetBottomRight();
			const FVector2D V3 = DrawRect.GetBottomLeft();

			const TArray<FSlateVertex> Vertices =
			{
				FSlateVertex::Make<ESlateVertexRounding::Disabled>(WidgetOffsetTransform, FVector2f{ (float)V0.X, (float)V0.Y }, FVector2f{ 0.f, 0.f }, FColor::White),
				FSlateVertex::Make<ESlateVertexRounding::Disabled>(WidgetOffsetTransform, FVector2f{ (float)V1.X, (float)V1.Y }, FVector2f{ 1.f, 0.f }, FColor::White),
				FSlateVertex::Make<ESlateVertexRounding::Disabled>(WidgetOffsetTransform, FVector2f{ (float)V2.X, (float)V2.Y }, FVector2f{ 1.f, 1.f }, FColor::White),
				FSlateVertex::Make<ESlateVertexRounding::Disabled>(WidgetOffsetTransform, FVector2f{ (float)V3.X, (float)V3.Y }, FVector2f{ 0.f, 1.f }, FColor::White)
			};
			const TArray<uint32> Indices =
			{
				0, 1, 2,
				0, 2, 3,
			};

			OutDrawElements.PushClip(FSlateClippingZone{ MyClippingRect });
			FSlateDrawElement::MakeCustomVerts(OutDrawElements, LayerId, m_ImGuiSlateBrush.GetRenderingResource(), Vertices, Indices, nullptr, 0, 0, ESlateDrawEffect::IgnoreTextureAlpha | ESlateDrawEffect::NoGamma | ESlateDrawEffect::NoBlending);
			OutDrawElements.PopClip();
		}
	}

	return Super::OnPaint(Args, AllottedGeometry, MyClippingRect, OutDrawElements, LayerId, WidgetStyle, bParentEnabled);
}

#pragma region SLATE_INPUT
static ImGuiKey FKeyToImGuiKey(FName Keyname)
{
#define LITERAL_TRANSLATION(Key) { EKeys::##Key.GetFName(), ImGuiKey_##Key }
	// not an exhaustive mapping, some keys are missing :^|
	static const TMap<FName, ImGuiKey> FKeyToImGuiKey =
	{
		LITERAL_TRANSLATION(A), LITERAL_TRANSLATION(B), LITERAL_TRANSLATION(C), LITERAL_TRANSLATION(D), LITERAL_TRANSLATION(E), LITERAL_TRANSLATION(F),
		LITERAL_TRANSLATION(G), LITERAL_TRANSLATION(H), LITERAL_TRANSLATION(I), LITERAL_TRANSLATION(J), LITERAL_TRANSLATION(K), LITERAL_TRANSLATION(L),
		LITERAL_TRANSLATION(M), LITERAL_TRANSLATION(N), LITERAL_TRANSLATION(O), LITERAL_TRANSLATION(P), LITERAL_TRANSLATION(Q), LITERAL_TRANSLATION(R),
		LITERAL_TRANSLATION(S), LITERAL_TRANSLATION(T), LITERAL_TRANSLATION(U), LITERAL_TRANSLATION(V), LITERAL_TRANSLATION(W), LITERAL_TRANSLATION(X),
		LITERAL_TRANSLATION(Y), LITERAL_TRANSLATION(Z),
		LITERAL_TRANSLATION(F1), LITERAL_TRANSLATION(F2), LITERAL_TRANSLATION(F3), LITERAL_TRANSLATION(F4),
		LITERAL_TRANSLATION(F5), LITERAL_TRANSLATION(F6), LITERAL_TRANSLATION(F7), LITERAL_TRANSLATION(F8),
		LITERAL_TRANSLATION(F9), LITERAL_TRANSLATION(F10), LITERAL_TRANSLATION(F11), LITERAL_TRANSLATION(F12),
		LITERAL_TRANSLATION(Enter), LITERAL_TRANSLATION(Insert), LITERAL_TRANSLATION(Delete), LITERAL_TRANSLATION(Escape), LITERAL_TRANSLATION(Tab),
		LITERAL_TRANSLATION(PageUp), LITERAL_TRANSLATION(PageDown), LITERAL_TRANSLATION(Home), LITERAL_TRANSLATION(End),
		LITERAL_TRANSLATION(NumLock), LITERAL_TRANSLATION(ScrollLock), LITERAL_TRANSLATION(CapsLock),
		LITERAL_TRANSLATION(RightBracket), LITERAL_TRANSLATION(LeftBracket), LITERAL_TRANSLATION(Backslash), LITERAL_TRANSLATION(Slash),
		LITERAL_TRANSLATION(Semicolon), LITERAL_TRANSLATION(Period), LITERAL_TRANSLATION(Comma), LITERAL_TRANSLATION(Apostrophe), LITERAL_TRANSLATION(Pause),
		{ EKeys::Zero.GetFName(), ImGuiKey_0 }, { EKeys::One.GetFName(), ImGuiKey_1 }, { EKeys::Two.GetFName(), ImGuiKey_2 },
		{ EKeys::Three.GetFName(), ImGuiKey_3 }, { EKeys::Four.GetFName(), ImGuiKey_4 }, { EKeys::Five.GetFName(), ImGuiKey_5 },
		{ EKeys::Six.GetFName(), ImGuiKey_6 }, { EKeys::Seven.GetFName(), ImGuiKey_7 }, { EKeys::Eight.GetFName(), ImGuiKey_8 }, { EKeys::Nine.GetFName(), ImGuiKey_9 },
		{ EKeys::NumPadZero.GetFName(), ImGuiKey_Keypad0 }, { EKeys::NumPadOne.GetFName(), ImGuiKey_Keypad1 }, { EKeys::NumPadTwo.GetFName(), ImGuiKey_Keypad2 },
		{ EKeys::NumPadThree.GetFName(), ImGuiKey_Keypad3 }, { EKeys::NumPadFour.GetFName(), ImGuiKey_Keypad4 }, { EKeys::NumPadFive.GetFName(), ImGuiKey_Keypad5 },
		{ EKeys::NumPadSix.GetFName(), ImGuiKey_Keypad6 }, { EKeys::NumPadSeven.GetFName(), ImGuiKey_Keypad7 }, { EKeys::NumPadEight.GetFName(), ImGuiKey_Keypad8 },
		{ EKeys::NumPadNine.GetFName(), ImGuiKey_Keypad9 },
		{ EKeys::LeftShift.GetFName(), ImGuiKey_LeftShift }, { EKeys::LeftControl.GetFName(), ImGuiKey_LeftCtrl }, { EKeys::LeftAlt.GetFName(), ImGuiKey_LeftAlt },
		{ EKeys::RightShift.GetFName(), ImGuiKey_RightShift }, { EKeys::RightControl.GetFName(), ImGuiKey_RightCtrl }, { EKeys::RightAlt.GetFName(), ImGuiKey_RightAlt },
		{ EKeys::SpaceBar.GetFName(), ImGuiKey_Space }, { EKeys::BackSpace.GetFName(), ImGuiKey_Backspace },
		{ EKeys::Up.GetFName(), ImGuiKey_UpArrow }, { EKeys::Down.GetFName(), ImGuiKey_DownArrow },
		{ EKeys::Left.GetFName(), ImGuiKey_LeftArrow }, { EKeys::Right.GetFName(), ImGuiKey_RightArrow },
		{ EKeys::Subtract.GetFName(), ImGuiKey_KeypadSubtract }, { EKeys::Add.GetFName(), ImGuiKey_KeypadAdd },
		{ EKeys::Multiply.GetFName(), ImGuiKey_KeypadMultiply }, { EKeys::Divide.GetFName(), ImGuiKey_KeypadDivide },
		{ EKeys::Decimal.GetFName(), ImGuiKey_KeypadDecimal }, { EKeys::Equals.GetFName(), ImGuiKey_Equal },
	};
#undef LITERAL_TRANSLATION

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

void SImGuiWindow::AddMouseButtonEvent(FKey MouseKey, bool IsDown)
{
	ImGuiIO& IO = GetImGuiIO();
	IO.AddMouseButtonEvent(FMouseKeyToImGuiKey(MouseKey), IsDown);
}

void SImGuiWindow::AddKeyEvent(FKeyEvent KeyEvent, bool IsDown)
{
	ImGuiIO& IO = GetImGuiIO();

	const ImGuiKey ImGuiKey = FKeyToImGuiKey(KeyEvent.GetKey().GetFName());
	if (ImGuiKey != ImGuiKey_None)
	{
		IO.AddKeyEvent(ImGuiKey, IsDown);
	}

	IO.AddKeyEvent(ImGuiKey_ModShift, KeyEvent.GetModifierKeys().IsShiftDown());
	IO.AddKeyEvent(ImGuiKey_ModCtrl, KeyEvent.GetModifierKeys().IsControlDown());
	IO.AddKeyEvent(ImGuiKey_ModAlt, KeyEvent.GetModifierKeys().IsAltDown());
}

FReply SImGuiWindow::OnKeyChar(const FGeometry& MyGeometry, const FCharacterEvent& CharacterEvent)
{
	ImGuiIO& IO = GetImGuiIO();
	IO.AddInputCharacterUTF16(CharacterEvent.GetCharacter());

	return FReply::Handled();
}

FReply SImGuiWindow::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& KeyEvent)
{
	ImGuiIO& IO = GetImGuiIO();
	AddKeyEvent(KeyEvent, true);

	return FReply::Handled();
}

FReply SImGuiWindow::OnKeyUp(const FGeometry& MyGeometry, const FKeyEvent& KeyEvent)
{
	ImGuiIO& IO = GetImGuiIO();
	AddKeyEvent(KeyEvent, false);

	return FReply::Handled();
}

FReply SImGuiWindow::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	AddMouseButtonEvent(MouseEvent.GetEffectingButton(), true);

	return FReply::Handled().CaptureMouse(SharedThis(this));
}

FReply SImGuiWindow::OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	AddMouseButtonEvent(MouseEvent.GetEffectingButton(), false);

	return FReply::Handled().ReleaseMouseCapture();
}

FReply SImGuiWindow::OnMouseButtonDoubleClick(const FGeometry& InMyGeometry, const FPointerEvent& MouseEvent)
{
	AddMouseButtonEvent(MouseEvent.GetEffectingButton(), true);

	return FReply::Handled();
}

FReply SImGuiWindow::OnMouseWheel(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	ImGuiIO& IO = GetImGuiIO();
	IO.AddMouseWheelEvent(0.f, MouseEvent.GetWheelDelta());

	return FReply::Handled();
}

FReply SImGuiWindow::OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	const FVector2D LocalMousePosition = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());

	ImGuiIO& IO = GetImGuiIO();
	IO.AddMousePosEvent(LocalMousePosition.X, LocalMousePosition.Y);

	return FReply::Handled();
}
#pragma endregion SLATE_INPUT

void SImGuiMainWindow::TickInternal(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	FImGuiRuntimeModule& ImGuiRuntimeModule = FModuleManager::GetModuleChecked<FImGuiRuntimeModule>("ImGuiRuntime");
	if (ImGuiRuntimeModule.GetMainWindowTickDelegate().IsBound())
	{
		ImGui::DockSpaceOverViewport(ImGui::GetMainViewport());
		ImGuiRuntimeModule.GetMainWindowTickDelegate().Broadcast(InDeltaTime);
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

void SImGuiWidgetWindow::Construct(const FArguments& InArgs)
{
	Super::Construct(Super::FArguments());

	m_OnTickDelegate = InArgs._OnTickDelegate;
}

void SImGuiWidgetWindow::TickInternal(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	const ImGuiID MainDockSpaceID = ImGui::DockSpaceOverViewport(ImGui::GetMainViewport());
	ImGui::SetNextWindowDockID(MainDockSpaceID);

	m_OnTickDelegate.ExecuteIfBound(InDeltaTime);
}