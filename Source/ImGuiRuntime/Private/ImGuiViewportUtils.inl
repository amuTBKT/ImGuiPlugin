// Copyright 2024-25 Amit Kumar Mehar. All Rights Reserved.

namespace ImGuiUtils
{
	FORCEINLINE static ImGuiKey ImGuiToUnrealKey(FName Keyname)
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

	FORCEINLINE static EMouseCursor::Type ImGuiToUnrealCursor(ImGuiMouseCursor Cursor)
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
		return ImGuiToUMGCursor[Cursor];
	}

	FORCEINLINE static int32 UnrealToImGuiMouseButton(FKey MouseKey)
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
		return MouseButton;
	}

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

	static bool RenderImGuiWidgetToRenderTarget(TNonNullPtr<const ImDrawData> DrawData, TNonNullPtr<UTextureRenderTarget2D> RenderTarget, bool bClearRT)
	{
		if (DrawData->TotalVtxCount == 0 || DrawData->TotalIdxCount == 0)
		{
			return false;
		}
		if (DrawData->DisplaySize.x < KINDA_SMALL_NUMBER || DrawData->DisplaySize.y < KINDA_SMALL_NUMBER)
		{
			return false;
		}

		UImGuiSubsystem* ImGuiSubsystem = UImGuiSubsystem::Get();
		for (ImTextureData* TexData : *DrawData->Textures)
		{
			ImGuiSubsystem->UpdateTextureData(TexData);
		}

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
		RenderData->DisplayPos = ImVec2(FMath::FloorToInt(DrawData->DisplayPos.x), FMath::FloorToInt(DrawData->DisplayPos.y));
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

		RenderData->bClearRenderTarget = bClearRT;

		ENQUEUE_RENDER_COMMAND(RenderImGui)(
			[RenderData, RT_Resource = RenderTarget->GetResource()](FRHICommandListImmediate& RHICmdList)
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

		return true;
	}

	// Widget handling rendering and inputs (no Tick logic)
	class SImGuiViewportWidget final : public SLeafWidget, public FGCObject
	{
		using Super = SLeafWidget;
	public:
		SLATE_BEGIN_ARGS(SImGuiViewportWidget)
		{
		}
		SLATE_END_ARGS()

		void Construct(const FArguments& InArgs, TWeakPtr<SImGuiWidgetBase> InMainViewportWidget)
		{
			MainViewportWidget = InMainViewportWidget;

			m_ImGuiRT = NewObject<UTextureRenderTarget2D>();
			m_ImGuiRT->Filter = TextureFilter::TF_Nearest;
			m_ImGuiRT->RenderTargetFormat = ETextureRenderTargetFormat::RTF_RGBA8;
			m_ImGuiRT->ClearColor = FLinearColor(0.f, 0.f, 0.f, 0.f);
			m_ImGuiRT->InitAutoFormat(32, 32);
			m_ImGuiRT->UpdateResourceImmediate(true);

			m_ImGuiSlateBrush.SetResourceObject(m_ImGuiRT);
		}
		virtual ~SImGuiViewportWidget()
		{
		}

		virtual void AddReferencedObjects(FReferenceCollector& Collector) override
		{
			Collector.AddReferencedObject(m_ImGuiRT);
		}
		virtual FString GetReferencerName() const override
		{
			return TEXT("SImGuiViewportWidget");
		}

		virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D::ZeroVector; }

		virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& WidgetGeometry, const FSlateRect& ClippingRect,
			FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& WidgetStyle, bool bParentEnabled) const override final
		{
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

			return LayerId;
		}

		virtual bool SupportsKeyboardFocus() const override { return true; }

		void OnDrawDataGenerated(TNonNullPtr<const ImDrawData> DrawData)
		{
			// resize RT if needed
			{
				const int32 NewSizeX = FMath::CeilToInt(DrawData->DisplaySize.x);
				const int32 NewSizeY = FMath::CeilToInt(DrawData->DisplaySize.y);

				if (m_ImGuiRT->SizeX < NewSizeX || m_ImGuiRT->SizeY < NewSizeY)
				{
					m_ImGuiRT->ResizeTarget(NewSizeX, NewSizeY);
				}
			}

			RenderImGuiWidgetToRenderTarget(DrawData, m_ImGuiRT.Get(), /*bClearRT=*/false);
		}

		virtual FReply OnKeyChar(const FGeometry& WidgetGeometry, const FCharacterEvent& CharacterEvent) override
		{
			TSharedPtr<SImGuiWidgetBase> RootWidget = MainViewportWidget.Pin();
			return RootWidget ? RootWidget->OnKeyChar(RootWidget->GetCachedGeometry(), CharacterEvent) : FReply::Unhandled();
		}

		virtual FReply OnKeyDown(const FGeometry& WidgetGeometry, const FKeyEvent& KeyEvent) override
		{
			TSharedPtr<SImGuiWidgetBase> RootWidget = MainViewportWidget.Pin();
			return RootWidget ? RootWidget->OnKeyDown(MainViewportWidget.Pin()->GetCachedGeometry(), KeyEvent) : FReply::Unhandled();
		}

		virtual FReply OnKeyUp(const FGeometry& WidgetGeometry, const FKeyEvent& KeyEvent) override
		{
			TSharedPtr<SImGuiWidgetBase> RootWidget = MainViewportWidget.Pin();
			return RootWidget ? RootWidget->OnKeyUp(RootWidget->GetCachedGeometry(), KeyEvent) : FReply::Unhandled();
		}

		virtual void OnMouseLeave(const FPointerEvent& MouseEvent) override
		{
			// NOTE: ImGui windows often lag when dragged,
			// so don't call OnMouseLeave if we could potentially be moving a viewport window (check for HasMouseCapture())
			if (!HasMouseCapture())
			{
				TSharedPtr<SImGuiWidgetBase> RootWidget = MainViewportWidget.Pin();
				if (RootWidget)
				{
					RootWidget->OnMouseLeave(MouseEvent);
				}
			}
		}

		virtual FReply OnMouseButtonDown(const FGeometry& WidgetGeometry, const FPointerEvent& MouseEvent) override
		{
			TSharedPtr<SImGuiWidgetBase> RootWidget = MainViewportWidget.Pin();
			if (!RootWidget)
			{
				return FReply::Unhandled();
			}

			// NOTE: not passing through 'OnMouseButtonDown' as we would like to capture the mouse here and not the root widget

			ImGuiIO& IO = RootWidget->GetImGuiIO();
			IO.AddMouseButtonEvent(ImGuiUtils::UnrealToImGuiMouseButton(MouseEvent.GetEffectingButton()), /*down=*/true);

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

		virtual FReply OnMouseButtonUp(const FGeometry& WidgetGeometry, const FPointerEvent& MouseEvent) override
		{
			TSharedPtr<SImGuiWidgetBase> RootWidget = MainViewportWidget.Pin();
			if (!RootWidget)
			{
				return FReply::Unhandled();
			}

			// NOTE: not passing through 'OnMouseButtonUp' as we would like to release the mouse capture here and not the root widget

			ImGuiIO& IO = RootWidget->GetImGuiIO();
			IO.AddMouseButtonEvent(ImGuiUtils::UnrealToImGuiMouseButton(MouseEvent.GetEffectingButton()), /*down=*/false);

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

		virtual FReply OnMouseButtonDoubleClick(const FGeometry& WidgetGeometry, const FPointerEvent& MouseEvent) override
		{
			TSharedPtr<SImGuiWidgetBase> RootWidget = MainViewportWidget.Pin();
			return RootWidget ? RootWidget->OnMouseButtonDoubleClick(RootWidget->GetCachedGeometry(), MouseEvent) : FReply::Unhandled();
		}

		virtual FReply OnMouseWheel(const FGeometry& WidgetGeometry, const FPointerEvent& MouseEvent) override
		{
			TSharedPtr<SImGuiWidgetBase> RootWidget = MainViewportWidget.Pin();
			return RootWidget ? RootWidget->OnMouseWheel(RootWidget->GetCachedGeometry(), MouseEvent) : FReply::Unhandled();
		}

		virtual FReply OnMouseMove(const FGeometry& WidgetGeometry, const FPointerEvent& MouseEvent) override
		{
			TSharedPtr<SImGuiWidgetBase> RootWidget = MainViewportWidget.Pin();
			return RootWidget ? RootWidget->OnMouseMove(RootWidget->GetCachedGeometry(), MouseEvent) : FReply::Unhandled();
		}

		virtual FCursorReply OnCursorQuery(const FGeometry& WidgetGeometry, const FPointerEvent& CursorEvent) const override
		{
			TSharedPtr<SImGuiWidgetBase> RootWidget = MainViewportWidget.Pin();
			return RootWidget ? RootWidget->OnCursorQuery(RootWidget->GetCachedGeometry(), CursorEvent) : FCursorReply::Unhandled();
		}

		virtual void OnDragLeave(const FDragDropEvent& DragDropEvent) override
		{
			TSharedPtr<SImGuiWidgetBase> RootWidget = MainViewportWidget.Pin();
			if (RootWidget)
			{
				RootWidget->OnDragLeave(DragDropEvent);
			}
		}

		virtual FReply OnDragOver(const FGeometry& WidgetGeometry, const FDragDropEvent& DragDropEvent) override
		{
			TSharedPtr<SImGuiWidgetBase> RootWidget = MainViewportWidget.Pin();
			return RootWidget ? RootWidget->OnDragOver(RootWidget->GetCachedGeometry(), DragDropEvent) : FReply::Unhandled();
		}

		virtual FReply OnDrop(const FGeometry& WidgetGeometry, const FDragDropEvent& DragDropEvent) override
		{
			TSharedPtr<SImGuiWidgetBase> RootWidget = MainViewportWidget.Pin();
			return RootWidget ? RootWidget->OnDrop(RootWidget->GetCachedGeometry(), DragDropEvent) : FReply::Unhandled();
		}

	protected:
		FSlateBrush m_ImGuiSlateBrush;
		TObjectPtr<UTextureRenderTarget2D> m_ImGuiRT = nullptr;
		TWeakPtr<SImGuiWidgetBase> MainViewportWidget = nullptr;
	};

	struct FImGuiViewportData
	{
		TWeakPtr<SWindow> ViewportWindow = nullptr;
		TSharedPtr<SImGuiViewportWidget> ViewportWidget = nullptr;

		TWeakPtr<SWindow> MainViewportWindow = nullptr;
		TWeakPtr<SImGuiWidgetBase> MainViewportWidget = nullptr;
	};

	static void UnrealPlatform_CreateWindow(ImGuiViewport* Viewport)
	{		
		TWeakPtr<SWindow> MainViewportWindowPtr = nullptr;
		TWeakPtr<SImGuiWidgetBase> MainViewportWidgetPtr = nullptr;
		{
			ImGuiViewport* ParentViewport = ImGui::FindViewportByID(Viewport->ParentViewportId);
			if (!ParentViewport)
			{
				ParentViewport = ImGui::GetMainViewport();
			}
			if (ParentViewport)
			{
				ImGuiUtils::FImGuiViewportData* ParentViewportData = (ImGuiUtils::FImGuiViewportData*)ParentViewport->PlatformUserData;
				if (!ParentViewportData->MainViewportWindow.IsValid()) //can become invalid when moving dock tabs
				{
					ParentViewportData->MainViewportWindow = FSlateApplication::Get().FindWidgetWindow(ParentViewportData->MainViewportWidget.Pin().ToSharedRef());
				}
				MainViewportWindowPtr = ParentViewportData->MainViewportWindow;
				MainViewportWidgetPtr = ParentViewportData->MainViewportWidget;
			}
		}
		ensure(MainViewportWindowPtr.IsValid() && MainViewportWidgetPtr.IsValid());

		TSharedPtr<ImGuiUtils::SImGuiViewportWidget> ViewportWidget = SNew(ImGuiUtils::SImGuiViewportWidget, MainViewportWidgetPtr);
		TSharedPtr<SWindow> ViewportWindow = 
			SNew(SWindow)
			// window size/layout
			.MinWidth(0.f)
			.MinHeight(0.f)
			.LayoutBorder({ 0 })
			.SizingRule(ESizingRule::FixedSize)
			.UserResizeBorder(FMargin(0))
			// window styling
			.HasCloseButton(false)
			.CreateTitleBar(false)
			.IsPopupWindow(true)
			.IsTopmostWindow(false)
			.Type(EWindowType::Menu)
			.UseOSWindowBorder(false)
			// don't activate the window as we would like to keep the focus at MainViewport window
			.FocusWhenFirstShown(false)
			.ActivationPolicy(EWindowActivationPolicy::Never)
			.Content()
			[
				ViewportWidget.ToSharedRef()
			];

		if (TSharedPtr<SWindow> ParentWindow = MainViewportWindowPtr.Pin())
		{
			FSlateApplication::Get().AddWindowAsNativeChild(ViewportWindow.ToSharedRef(), ParentWindow.ToSharedRef(), /*bShowImmediately=*/true);
		}
		else
		{
			FSlateApplication::Get().AddWindow(ViewportWindow.ToSharedRef(), /*bShowImmediately=*/true);
		}

		ImGuiUtils::FImGuiViewportData* ViewportData = IM_NEW(ImGuiUtils::FImGuiViewportData)();
		ViewportData->ViewportWindow = ViewportWindow;
		ViewportData->ViewportWidget = ViewportWidget;
		ViewportData->MainViewportWindow = MainViewportWindowPtr;
		ViewportData->MainViewportWidget = MainViewportWidgetPtr;

		Viewport->PlatformRequestResize = false;
		Viewport->PlatformUserData = ViewportData;
	}

	static void UnrealPlatform_DestroyWindow(ImGuiViewport* Viewport)
	{
		ImGuiUtils::FImGuiViewportData* ViewportData = (ImGuiUtils::FImGuiViewportData*)Viewport->PlatformUserData;
		if (TSharedPtr<SWindow> ViewportWindow = ViewportData->ViewportWindow.Pin())
		{
			ViewportWindow->RequestDestroyWindow();
		}
		ViewportData->ViewportWindow = nullptr;
		ViewportData->ViewportWidget = nullptr;
		ViewportData->MainViewportWindow = nullptr;
		ViewportData->MainViewportWidget = nullptr;
		IM_DELETE(ViewportData);

		Viewport->PlatformUserData = nullptr;
	}

	static void UnrealPlatform_SetWindowPosition(ImGuiViewport* Viewport, ImVec2 NewPosition)
	{
		ImGuiUtils::FImGuiViewportData* ViewportData = (ImGuiUtils::FImGuiViewportData*)Viewport->PlatformUserData;
		if (TSharedPtr<SWindow> ViewportWindow = ViewportData->ViewportWindow.Pin())
		{
			ViewportWindow->MoveWindowTo(FVector2f{ NewPosition.x, NewPosition.y });
		}
	}

	static ImVec2 UnrealPlatform_GetWindowPosition(ImGuiViewport* Viewport)
	{
		ImGuiUtils::FImGuiViewportData* ViewportData = (ImGuiUtils::FImGuiViewportData*)Viewport->PlatformUserData;
		if (ViewportData->ViewportWidget.IsValid())
		{
			FVector2f Position = ViewportData->ViewportWidget->GetCachedGeometry().GetAbsolutePosition();
			return ImVec2{ Position.X, Position.Y };
		}
		else if (TSharedPtr<SImGuiWidgetBase> MainViewportWidget = ViewportData->MainViewportWidget.Pin())
		{
			FVector2f Position = MainViewportWidget->GetCachedGeometry().GetAbsolutePosition();
			return ImVec2{ Position.X, Position.Y };
		}
		return ImVec2{ 0.f, 0.f };
	}

	static void UnrealPlatform_SetWindowSize(ImGuiViewport* Viewport, ImVec2 NewSize)
	{
		ImGuiUtils::FImGuiViewportData* ViewportData = (ImGuiUtils::FImGuiViewportData*)Viewport->PlatformUserData;
		if (TSharedPtr<SWindow> ViewportWindow = ViewportData->ViewportWindow.Pin())
		{
			ViewportWindow->Resize(FVector2f{ NewSize.x, NewSize.y });
		}
	}
	
	static ImVec2 UnrealPlatform_GetWindowSize(ImGuiViewport* Viewport)
	{
		ImGuiUtils::FImGuiViewportData* ViewportData = (ImGuiUtils::FImGuiViewportData*)Viewport->PlatformUserData;
		if (TSharedPtr<SWindow> ViewportWindow = ViewportData->ViewportWindow.Pin())
		{
			const FVector2f WindowSize = ViewportWindow->GetSizeInScreen();
			return ImVec2{ WindowSize.X, WindowSize.Y };
		}
		return ImVec2(0.f, 0.f);
	}

	static void UnrealPlatform_SetWindowTitle(ImGuiViewport* Viewport, const char* Title)
	{
		ImGuiUtils::FImGuiViewportData* ViewportData = (ImGuiUtils::FImGuiViewportData*)Viewport->PlatformUserData;
		if (TSharedPtr<SWindow> ViewportWindow = ViewportData->ViewportWindow.Pin())
		{
			ViewportWindow->SetTitle(FText::FromString(ANSI_TO_TCHAR(Title)));
		}
	}

	static void UnrealPlatform_ShowWindow(ImGuiViewport* Viewport)
	{
		ImGuiUtils::FImGuiViewportData* ViewportData = (ImGuiUtils::FImGuiViewportData*)Viewport->PlatformUserData;
		if (TSharedPtr<SWindow> ViewportWindow = ViewportData->ViewportWindow.Pin())
		{
			ViewportWindow->ShowWindow();
			if (!(Viewport->Flags & ImGuiViewportFlags_NoFocusOnAppearing) && ViewportData->ViewportWindow.IsValid())
			{
				ViewportWindow->GetNativeWindow()->SetWindowFocus();
			}
		}
	}

	static void UnrealPlatform_SetWindowFocus(ImGuiViewport* Viewport)
	{
		ImGuiUtils::FImGuiViewportData* ViewportData = (ImGuiUtils::FImGuiViewportData*)Viewport->PlatformUserData;
		if (TSharedPtr<SWindow> ViewportWindow = ViewportData->ViewportWindow.Pin())
		{
			ViewportWindow->GetNativeWindow()->SetWindowFocus();
		}
	}

	static bool UnrealPlatform_GetWindowFocus(ImGuiViewport* Viewport)
	{
		ImGuiUtils::FImGuiViewportData* ViewportData = (ImGuiUtils::FImGuiViewportData*)Viewport->PlatformUserData;
		if (TSharedPtr<SWindow> ViewportWindow = ViewportData->ViewportWindow.Pin())
		{
			return ViewportWindow->HasAnyUserFocusOrFocusedDescendants();
		}
		return false;
	}

	static bool UnrealPlatform_GetWindowMinimized(ImGuiViewport* Viewport)
	{
		ImGuiUtils::FImGuiViewportData* ViewportData = (ImGuiUtils::FImGuiViewportData*)Viewport->PlatformUserData;
		if (TSharedPtr<SWindow> ViewportWindow = ViewportData->ViewportWindow.Pin())
		{
			return ViewportWindow->IsWindowMinimized();
		}
		return false;
	}

	static void UnrealPlatform_SetWindowAlpha(ImGuiViewport* Viewport, float Alpha)
	{
		ImGuiUtils::FImGuiViewportData* ViewportData = (ImGuiUtils::FImGuiViewportData*)Viewport->PlatformUserData;
		if (TSharedPtr<SWindow> ViewportWindow = ViewportData->ViewportWindow.Pin())
		{
			ViewportWindow->SetOpacity(Alpha);
		}
	}

	static void UnrealPlatform_UpdateWindow(ImGuiViewport* Viewport)
	{
		ImGuiUtils::FImGuiViewportData* ViewportData = (ImGuiUtils::FImGuiViewportData*)Viewport->PlatformUserData;
		if (ViewportData->ViewportWidget.IsValid())
		{
			// window was destroyed by platform, happens when MainViewportWindow is dragged invalidating all child windows
			if (!ViewportData->ViewportWindow.IsValid())
			{
				ImGui::DestroyPlatformWindow((ImGuiViewportP*)Viewport);
			}
		}
	}

	static void UnrealPlatform_RenderWindow(ImGuiViewport* Viewport, void*)
	{
		ImGuiUtils::FImGuiViewportData* ViewportData = (ImGuiUtils::FImGuiViewportData*)Viewport->PlatformUserData;
		if (ViewportData && Viewport->DrawData && ViewportData->ViewportWidget.IsValid())
		{
			ViewportData->ViewportWidget->OnDrawDataGenerated(Viewport->DrawData);
		}
	}
}