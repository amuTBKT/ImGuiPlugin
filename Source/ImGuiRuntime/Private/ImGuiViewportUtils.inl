// Copyright 2024-25 Amit Kumar Mehar. All Rights Reserved.

namespace ImGuiUtils
{
	static inline uint32 PackF16ToU32(const FVector2f& Value)
	{
		return uint32(FFloat16(Value.X).Encoded) | (uint32(FFloat16(Value.Y).Encoded) << 16);
	}

	class FWidgetDrawer;
	struct FDeferredDeletionQueue
	{
		FDeferredDeletionQueue()
		{
			FCoreDelegates::OnBeginFrame.AddRaw(this, &FDeferredDeletionQueue::ProcessObjects, /*bForceDestroy=*/false);
			FCoreDelegates::OnPreExit.AddRaw(this, &FDeferredDeletionQueue::ProcessObjects, /*bForceDestroy=*/true);
		}

		template <typename ...Args>
		void DeferredDeleteObjects(Args&&... InArgs)
		{
			(ObjectsPendingDelete.Emplace(Forward<Args>(InArgs)), ...);
		}

	private:
		void ProcessObjects(bool bForceDestroy)
		{
			if (ObjectsPendingDelete.IsEmpty())
			{
				return;
			}

			if (bForceDestroy)
			{
				FlushRenderingCommands();
			}

			while (ObjectsPendingDelete.Num())
			{
				FDeferredDeleteObject& Object = ObjectsPendingDelete[0];
				if (!bForceDestroy && (Object.FrameIndex > GFrameCounter))
				{
					break;
				}

				if (Object.Storage.IsType<ImGuiContext*>())
				{
					ImGuiContext* Context = Object.Storage.Get<ImGuiContext*>();
					ImGui::SetCurrentContext(Context);
					{
						ImGui::DestroyPlatformWindows();
						ImGui::DestroyContext(Context);
					}
					ImGui::SetCurrentContext(nullptr);
				}

				ObjectsPendingDelete.RemoveAt(0, EAllowShrinking::No);
			}
		}

		struct FDeferredDeleteObject
		{
			explicit FDeferredDeleteObject(TSharedPtr<FWidgetDrawer> InWidgetDrawer)
				: Storage(TInPlaceType<TSharedPtr<FWidgetDrawer>>(), InWidgetDrawer)
				, FrameIndex(GFrameCounter + 2)
			{
			}
			explicit FDeferredDeleteObject(ImGuiContext* InContext)
				: Storage(TInPlaceType<ImGuiContext*>(), InContext)
				, FrameIndex(GFrameCounter + 2)
			{
			}

			TVariant<TSharedPtr<FWidgetDrawer>, ImGuiContext*> Storage;
			uint64 FrameIndex;
		};
		TArray<FDeferredDeleteObject> ObjectsPendingDelete;
	};
	static FDeferredDeletionQueue DeferredDeletionQueue;

	FORCEINLINE static ImGuiKey UnrealToImGuiKey(FName Keyname)
	{
		static const TMap<FName, ImGuiKey> KeyMap =
		{
			{ EKeys::A.GetFName(), ImGuiKey_A },
			{ EKeys::B.GetFName(), ImGuiKey_B },
			{ EKeys::C.GetFName(), ImGuiKey_C },
			{ EKeys::D.GetFName(), ImGuiKey_D },
			{ EKeys::E.GetFName(), ImGuiKey_E },
			{ EKeys::F.GetFName(), ImGuiKey_F },
			{ EKeys::G.GetFName(), ImGuiKey_G },
			{ EKeys::H.GetFName(), ImGuiKey_H },
			{ EKeys::I.GetFName(), ImGuiKey_I },
			{ EKeys::J.GetFName(), ImGuiKey_J },
			{ EKeys::K.GetFName(), ImGuiKey_K },
			{ EKeys::L.GetFName(), ImGuiKey_L },
			{ EKeys::M.GetFName(), ImGuiKey_M },
			{ EKeys::N.GetFName(), ImGuiKey_N },
			{ EKeys::O.GetFName(), ImGuiKey_O },
			{ EKeys::P.GetFName(), ImGuiKey_P },
			{ EKeys::Q.GetFName(), ImGuiKey_Q },
			{ EKeys::R.GetFName(), ImGuiKey_R },
			{ EKeys::S.GetFName(), ImGuiKey_S },
			{ EKeys::T.GetFName(), ImGuiKey_T },
			{ EKeys::U.GetFName(), ImGuiKey_U },
			{ EKeys::V.GetFName(), ImGuiKey_V },
			{ EKeys::W.GetFName(), ImGuiKey_W },
			{ EKeys::X.GetFName(), ImGuiKey_X },
			{ EKeys::Y.GetFName(), ImGuiKey_Y },
			{ EKeys::Z.GetFName(), ImGuiKey_Z },
			{ EKeys::F1.GetFName(), ImGuiKey_F1 },
			{ EKeys::F2.GetFName(), ImGuiKey_F2 },
			{ EKeys::F3.GetFName(), ImGuiKey_F3 },
			{ EKeys::F4.GetFName(), ImGuiKey_F4 },
			{ EKeys::F5.GetFName(), ImGuiKey_F5 },
			{ EKeys::F6.GetFName(), ImGuiKey_F6 },
			{ EKeys::F7.GetFName(), ImGuiKey_F7 },
			{ EKeys::F8.GetFName(), ImGuiKey_F8 },
			{ EKeys::F9.GetFName(), ImGuiKey_F9 },
			{ EKeys::F10.GetFName(), ImGuiKey_F10 },
			{ EKeys::F11.GetFName(), ImGuiKey_F11 },
			{ EKeys::F12.GetFName(), ImGuiKey_F12 },
			{ EKeys::Enter.GetFName(), ImGuiKey_Enter },
			{ EKeys::Insert.GetFName(), ImGuiKey_Insert },
			{ EKeys::Delete.GetFName(), ImGuiKey_Delete },
			{ EKeys::Escape.GetFName(), ImGuiKey_Escape },
			{ EKeys::Tab.GetFName(), ImGuiKey_Tab },
			{ EKeys::PageUp.GetFName(), ImGuiKey_PageUp },
			{ EKeys::PageDown.GetFName(), ImGuiKey_PageDown },
			{ EKeys::Home.GetFName(), ImGuiKey_Home },
			{ EKeys::End.GetFName(), ImGuiKey_End },
			{ EKeys::NumLock.GetFName(), ImGuiKey_NumLock },
			{ EKeys::ScrollLock.GetFName(), ImGuiKey_ScrollLock },
			{ EKeys::CapsLock.GetFName(), ImGuiKey_CapsLock },
			{ EKeys::RightBracket.GetFName(), ImGuiKey_RightBracket },
			{ EKeys::LeftBracket.GetFName(), ImGuiKey_LeftBracket },
			{ EKeys::Backslash.GetFName(), ImGuiKey_Backslash },
			{ EKeys::Slash.GetFName(), ImGuiKey_Slash },
			{ EKeys::Semicolon.GetFName(), ImGuiKey_Semicolon },
			{ EKeys::Period.GetFName(), ImGuiKey_Period },
			{ EKeys::Comma.GetFName(), ImGuiKey_Comma },
			{ EKeys::Apostrophe.GetFName(), ImGuiKey_Apostrophe },
			{ EKeys::Pause.GetFName(), ImGuiKey_Pause },
			{ EKeys::Zero.GetFName(), ImGuiKey_0 },
			{ EKeys::One.GetFName(), ImGuiKey_1 },
			{ EKeys::Two.GetFName(), ImGuiKey_2 },
			{ EKeys::Three.GetFName(), ImGuiKey_3 },
			{ EKeys::Four.GetFName(), ImGuiKey_4 },
			{ EKeys::Five.GetFName(), ImGuiKey_5 },
			{ EKeys::Six.GetFName(), ImGuiKey_6 },
			{ EKeys::Seven.GetFName(), ImGuiKey_7 },
			{ EKeys::Eight.GetFName(), ImGuiKey_8 },
			{ EKeys::Nine.GetFName(), ImGuiKey_9 },
			{ EKeys::NumPadZero.GetFName(), ImGuiKey_Keypad0 },
			{ EKeys::NumPadOne.GetFName(), ImGuiKey_Keypad1 },
			{ EKeys::NumPadTwo.GetFName(), ImGuiKey_Keypad2 },
			{ EKeys::NumPadThree.GetFName(), ImGuiKey_Keypad3 },
			{ EKeys::NumPadFour.GetFName(), ImGuiKey_Keypad4 },
			{ EKeys::NumPadFive.GetFName(), ImGuiKey_Keypad5 },
			{ EKeys::NumPadSix.GetFName(), ImGuiKey_Keypad6 },
			{ EKeys::NumPadSeven.GetFName(), ImGuiKey_Keypad7 },
			{ EKeys::NumPadEight.GetFName(), ImGuiKey_Keypad8 },
			{ EKeys::NumPadNine.GetFName(), ImGuiKey_Keypad9 },
			{ EKeys::LeftShift.GetFName(), ImGuiKey_LeftShift },
			{ EKeys::LeftControl.GetFName(), ImGuiKey_LeftCtrl },
			{ EKeys::LeftAlt.GetFName(), ImGuiKey_LeftAlt },
			{ EKeys::RightShift.GetFName(), ImGuiKey_RightShift },
			{ EKeys::RightControl.GetFName(), ImGuiKey_RightCtrl },
			{ EKeys::RightAlt.GetFName(), ImGuiKey_RightAlt },
			{ EKeys::SpaceBar.GetFName(), ImGuiKey_Space },
			{ EKeys::BackSpace.GetFName(), ImGuiKey_Backspace },
			{ EKeys::Up.GetFName(), ImGuiKey_UpArrow },
			{ EKeys::Down.GetFName(), ImGuiKey_DownArrow },
			{ EKeys::Left.GetFName(), ImGuiKey_LeftArrow },
			{ EKeys::Right.GetFName(), ImGuiKey_RightArrow },
			{ EKeys::Subtract.GetFName(), ImGuiKey_KeypadSubtract },
			{ EKeys::Add.GetFName(), ImGuiKey_KeypadAdd },
			{ EKeys::Multiply.GetFName(), ImGuiKey_KeypadMultiply },
			{ EKeys::Divide.GetFName(), ImGuiKey_KeypadDivide },
			{ EKeys::Decimal.GetFName(), ImGuiKey_KeypadDecimal },
			{ EKeys::Equals.GetFName(), ImGuiKey_Equal },
		};

		const ImGuiKey* Key = KeyMap.Find(Keyname);
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

		bool SetDrawData(ImDrawData* DrawData, int32 FrameCount, FVector2f DrawRectOffset)
		{
			m_DrawDataSnapshot.SnapUsingSwap(DrawData, FrameCount);
			DrawData = &m_DrawDataSnapshot.DrawData;

			m_DrawRectOffset = DrawRectOffset;

			UImGuiSubsystem* ImGuiSubsystem = UImGuiSubsystem::Get();
			for (ImTextureData* TexData : *DrawData->Textures)
			{
				if (TexData->Status != ImTextureStatus_OK)
				{
					ImGuiSubsystem->UpdateFontAtlasTexture(TexData);
				}
			}
			m_BoundTextureResources.Reset(ImGuiSubsystem->GetOneFrameResources().Num());

			m_bHasDrawCommands = DrawData->TotalVtxCount > 0 &&
								 DrawData->TotalIdxCount > 0 &&
								 (DrawData->DisplaySize.x > KINDA_SMALL_NUMBER) &&
								 (DrawData->DisplaySize.y > KINDA_SMALL_NUMBER);
			if (!m_bHasDrawCommands)
			{
				return false;
			}

			for (const FImGuiTextureResource& TextureResource : ImGuiSubsystem->GetOneFrameResources())
			{
				m_BoundTextureResources.Emplace(TextureResource, TextureResource.GetSlateShaderResource());
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

					m_BoundTextures.Reset(m_BoundTextureResources.Num());
					for (const auto& TextureResourceInfo : m_BoundTextureResources)
					{
						auto& BoundTexture = m_BoundTextures.AddDefaulted_GetRef();

						FSlateShaderResource* ShaderResource = TextureResourceInfo.ExpectedSlateResource;

						// validate resource against handle, this is needed when spamming slate atlas resizes/repacking
						if (TextureResourceInfo.TextureResource.UsesResourceHandle())
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

						if (BoundTexture.TextureRHI == nullptr)
						{
							BoundTexture.TextureRHI = GBlackTexture->TextureRHI;
						}
						if (BoundTexture.SamplerRHI == nullptr)
						{
							BoundTexture.SamplerRHI = TStaticSamplerState<SF_Bilinear, AM_Wrap, AM_Wrap, AM_Wrap>::GetRHI();
						}
					}

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
									else
									{
										DrawCmd.UserCallback(RHICmdList, DrawRect, DrawCmd.UserCallbackData, DrawCmd.UserCallbackDataSize);

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
									const float ScissorRectLeft		= FMath::Clamp(DrawCmd.ClipRect.x - DisplayPos.x + ViewportRect.Min.x, ViewportRect.Min.x, ViewportRect.Max.x);
									const float ScissorRectTop		= FMath::Clamp(DrawCmd.ClipRect.y - DisplayPos.y + ViewportRect.Min.y, ViewportRect.Min.y, ViewportRect.Max.y);
									const float ScissorRectRight	= FMath::Clamp(DrawCmd.ClipRect.z - DisplayPos.x + ViewportRect.Min.x, ViewportRect.Min.x, ViewportRect.Max.x);
									const float ScissorRectBottom	= FMath::Clamp(DrawCmd.ClipRect.w - DisplayPos.y + ViewportRect.Min.y, ViewportRect.Min.y, ViewportRect.Max.y);									
									if (ScissorRectRight <= ScissorRectLeft || ScissorRectBottom <= ScissorRectTop)
									{
										continue;
									}
									
									RHICmdList.SetScissorRect(true, ScissorRectLeft, ScissorRectTop, ScissorRectRight, ScissorRectBottom);

									uint32 TextureIndex = UImGuiSubsystem::ImGuiIDToIndex(DrawCmd.GetTexID());
									if (!(m_BoundTextures.IsValidIndex(TextureIndex)/* && RenderData.BoundTextures[Index].IsValid()*/))
									{
										TextureIndex = UImGuiSubsystem::GetMissingImageTextureIndex();
									}

									SetShaderParametersLegacyVS(
										RHICmdList,
										VertexShader,
										ProjectionMatrixParam,
										m_BoundTextures[TextureIndex].TexCoordOverrideMode);

									SetShaderParametersLegacyPS(
										RHICmdList,
										PixelShader,
										m_BoundTextures[TextureIndex].TextureRHI,
										m_BoundTextures[TextureIndex].SamplerRHI,
										ShaderStateOverrides | (m_BoundTextures[TextureIndex].IsSRGB ? (uint32)EImGuiShaderState::OutputInSRGB : 0));

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
		};
		struct FBoundTexture
		{
			FTextureRHIRef TextureRHI = nullptr;
			FSamplerStateRHIRef SamplerRHI = nullptr;
			bool IsSRGB = false;
			FUintVector2 TexCoordOverrideMode = FUintVector2::ZeroValue;
		};
		TArray<FBoundTexture> m_BoundTextures;
		TArray<FTextureResourceInfo> m_BoundTextureResources;
		FVector2f m_DrawRectOffset = FVector2f::ZeroVector;
		ImDrawDataSnapshot m_DrawDataSnapshot;
		bool m_bHasDrawCommands = false;
		bool m_bCaptureGpuFrame = false;
	};

	struct FImGuiViewportData
	{
		// only initialized for backend viewports
		TWeakPtr<SWindow> ViewportWindow = nullptr;
		TSharedPtr<SImGuiViewportWidget> ViewportWidget = nullptr;

		// For main viewport this window owns the widget
		// For backend viewports this window owns the viewport windows
		TWeakPtr<SWindow> ParentWindow = nullptr;

		// widget owning the ImGui context
		TWeakPtr<SImGuiWidgetBase> MainViewportWidget = nullptr;

		// to request viewport window recreation (for patching slate window references etc.)
		bool bInvalidateManagedViewportWindows = false;
	};

	// Widget handling rendering and inputs (no Tick logic)
	class SImGuiViewportWidget final : public SLeafWidget
	{
		using Super = SLeafWidget;
	public:
		SLATE_BEGIN_ARGS(SImGuiViewportWidget)
		{
		}
		SLATE_END_ARGS()

		void Construct(const FArguments& InArgs, TWeakPtr<SImGuiWidgetBase> InMainViewportWidget, ImGuiViewport* InImGuiViewport)
		{
			m_ImGuiViewport = InImGuiViewport;
			m_MainViewportWidget = InMainViewportWidget;
			m_WidgetDrawers[0] = MakeShared<ImGuiUtils::FWidgetDrawer>();
			m_WidgetDrawers[1] = MakeShared<ImGuiUtils::FWidgetDrawer>();

#if WITH_EDITOR
			FCoreDelegates::OnEndFrame.AddRaw(this, &SImGuiViewportWidget::UpdateWindowVisibility);
#endif
		}
		virtual ~SImGuiViewportWidget()
		{
			// NOTE: This widget is only used along with a standalone slate window
			// The destructor will *always* be called after the window is destroyed which flushes the rendering thread.
			// Lets us clear the widget drawers without worrying about pending render work.
			m_WidgetDrawers[0] = m_WidgetDrawers[1] = nullptr;

#if WITH_EDITOR
			FCoreDelegates::OnEndFrame.RemoveAll(this);
#endif
		}

		virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D::ZeroVector; }

#if WITH_EDITOR
		void UpdateWindowVisibility()
		{
			// slate doesn't update visibility for child windows, so a little workaround for it...
			const FImGuiViewportData* ViewportData = (FImGuiViewportData*)m_ImGuiViewport->PlatformUserData;
			if (ViewportData && ViewportData->ViewportWidget.IsValid())
			{
				TSharedPtr<SWindow> ViewportWindow = ViewportData->ViewportWindow.Pin();
				TSharedPtr<SImGuiWidgetBase> RootWidget = m_MainViewportWidget.Pin();

				const bool bNewVisibility = RootWidget.IsValid() ? (RootWidget->m_LastPaintFrameCounter >= GFrameCounter) : false;
				if (ViewportWindow && (bNewVisibility != ViewportWindow->IsVisible()))
				{
					if (bNewVisibility)
					{
						ViewportWindow->ShowWindow();
					}
					else
					{
						ViewportWindow->HideWindow();
					}
				}
			}
		}
#endif //#if WITH_EDITOR

		virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& WidgetGeometry, const FSlateRect& ClippingRect,
			FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& WidgetStyle, bool bParentEnabled) const override
		{
			DECLARE_SCOPE_CYCLE_COUNTER(TEXT("Render Widget [GT]"), STAT_ImGui_RenderWidget_GT, STATGROUP_ImGui);

			TSharedPtr<ImGuiUtils::FWidgetDrawer> WidgetDrawer = m_WidgetDrawers[ImGui::GetFrameCount() & 0x1];
			if (WidgetDrawer->HasDrawCommands())
			{
				const FSlateRect DrawRect = WidgetGeometry.GetRenderBoundingRect();
				WidgetDrawer->SetDrawRectOffset(DrawRect.GetTopLeft2f());

				OutDrawElements.PushClip(FSlateClippingZone{ ClippingRect });
				FSlateDrawElement::MakeCustom(OutDrawElements, LayerId, WidgetDrawer);
				OutDrawElements.PopClip();
			}
			return LayerId;
		}

		virtual bool SupportsKeyboardFocus() const override { return true; }

		void OnDrawDataGenerated(ImDrawData* DrawData)
		{
			m_WidgetDrawers[ImGui::GetFrameCount() & 0x1]->SetDrawData(DrawData, ImGui::GetFrameCount(), FVector2f::ZeroVector);
		}

		virtual FReply OnKeyChar(const FGeometry& WidgetGeometry, const FCharacterEvent& CharacterEvent) override
		{
			TSharedPtr<SImGuiWidgetBase> RootWidget = m_MainViewportWidget.Pin();
			return RootWidget ? RootWidget->OnKeyChar(RootWidget->GetCachedGeometry(), CharacterEvent) : FReply::Unhandled();
		}

		virtual FReply OnKeyDown(const FGeometry& WidgetGeometry, const FKeyEvent& KeyEvent) override
		{
			TSharedPtr<SImGuiWidgetBase> RootWidget = m_MainViewportWidget.Pin();
			return RootWidget ? RootWidget->OnKeyDown(m_MainViewportWidget.Pin()->GetCachedGeometry(), KeyEvent) : FReply::Unhandled();
		}

		virtual FReply OnKeyUp(const FGeometry& WidgetGeometry, const FKeyEvent& KeyEvent) override
		{
			TSharedPtr<SImGuiWidgetBase> RootWidget = m_MainViewportWidget.Pin();
			return RootWidget ? RootWidget->OnKeyUp(RootWidget->GetCachedGeometry(), KeyEvent) : FReply::Unhandled();
		}

		virtual void OnMouseLeave(const FPointerEvent& MouseEvent) override
		{
			// NOTE: ImGui windows often lag when dragged,
			// so don't call OnMouseLeave if we could potentially be moving a viewport window (check for HasMouseCapture())
			if (!HasMouseCapture())
			{
				TSharedPtr<SImGuiWidgetBase> RootWidget = m_MainViewportWidget.Pin();
				if (RootWidget)
				{
					RootWidget->OnMouseLeave(MouseEvent);
				}
			}
		}

		virtual FReply OnMouseButtonDown(const FGeometry& WidgetGeometry, const FPointerEvent& MouseEvent) override
		{
			TSharedPtr<SImGuiWidgetBase> RootWidget = m_MainViewportWidget.Pin();
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
			TSharedPtr<SImGuiWidgetBase> RootWidget = m_MainViewportWidget.Pin();
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
			TSharedPtr<SImGuiWidgetBase> RootWidget = m_MainViewportWidget.Pin();
			return RootWidget ? RootWidget->OnMouseButtonDoubleClick(RootWidget->GetCachedGeometry(), MouseEvent) : FReply::Unhandled();
		}

		virtual FReply OnMouseWheel(const FGeometry& WidgetGeometry, const FPointerEvent& MouseEvent) override
		{
			TSharedPtr<SImGuiWidgetBase> RootWidget = m_MainViewportWidget.Pin();
			return RootWidget ? RootWidget->OnMouseWheel(RootWidget->GetCachedGeometry(), MouseEvent) : FReply::Unhandled();
		}

		virtual FReply OnMouseMove(const FGeometry& WidgetGeometry, const FPointerEvent& MouseEvent) override
		{
			TSharedPtr<SImGuiWidgetBase> RootWidget = m_MainViewportWidget.Pin();
			return RootWidget ? RootWidget->OnMouseMove(RootWidget->GetCachedGeometry(), MouseEvent) : FReply::Unhandled();
		}

		virtual FCursorReply OnCursorQuery(const FGeometry& WidgetGeometry, const FPointerEvent& CursorEvent) const override
		{
			TSharedPtr<SImGuiWidgetBase> RootWidget = m_MainViewportWidget.Pin();
			return RootWidget ? RootWidget->OnCursorQuery(RootWidget->GetCachedGeometry(), CursorEvent) : FCursorReply::Unhandled();
		}

		virtual void OnDragLeave(const FDragDropEvent& DragDropEvent) override
		{
			TSharedPtr<SImGuiWidgetBase> RootWidget = m_MainViewportWidget.Pin();
			if (RootWidget)
			{
				RootWidget->OnDragLeave(DragDropEvent);
			}
		}

		virtual FReply OnDragOver(const FGeometry& WidgetGeometry, const FDragDropEvent& DragDropEvent) override
		{
			TSharedPtr<SImGuiWidgetBase> RootWidget = m_MainViewportWidget.Pin();
			return RootWidget ? RootWidget->OnDragOver(RootWidget->GetCachedGeometry(), DragDropEvent) : FReply::Unhandled();
		}

		virtual FReply OnDrop(const FGeometry& WidgetGeometry, const FDragDropEvent& DragDropEvent) override
		{
			TSharedPtr<SImGuiWidgetBase> RootWidget = m_MainViewportWidget.Pin();
			return RootWidget ? RootWidget->OnDrop(RootWidget->GetCachedGeometry(), DragDropEvent) : FReply::Unhandled();
		}

	private:
		const ImGuiViewport* m_ImGuiViewport = nullptr;
		TSharedPtr<ImGuiUtils::FWidgetDrawer> m_WidgetDrawers[2];
		TWeakPtr<SImGuiWidgetBase> m_MainViewportWidget = nullptr;
	};

	static void UnrealPlatform_CreateWindow(ImGuiViewport* Viewport)
	{
		TWeakPtr<SWindow> ParentWindowPtr = nullptr;
		TWeakPtr<SImGuiWidgetBase> MainViewportWidgetPtr = nullptr;
		{
			ImGuiViewport* ParentViewport = ImGui::FindViewportByID(Viewport->ParentViewportId);
			if (!ParentViewport)
			{
				ParentViewport = ImGui::GetMainViewport();
			}
			if (ParentViewport)
			{
				FImGuiViewportData* ParentViewportData = (FImGuiViewportData*)ParentViewport->PlatformUserData;
				MainViewportWidgetPtr = ParentViewportData->MainViewportWidget;
				
				// prefer viewport window if available (otherwise we fallback to the main viewport widget window)
				if (ParentViewportData->ViewportWindow.IsValid())
				{
					ParentWindowPtr = ParentViewportData->ViewportWindow;
				}
				else
				{
					ParentWindowPtr = ParentViewportData->ParentWindow;
				}
			}
		}
		if (!ensure(ParentWindowPtr.IsValid() && MainViewportWidgetPtr.IsValid()))
		{
			return;
		}

		const bool bTooltipWindow = (Viewport->Flags & ImGuiViewportFlags_TopMost);
		const bool bPopupWindow = (Viewport->Flags & ImGuiViewportFlags_NoTaskBarIcon);
		// don't activate the window as we would like to keep the focus at MainViewport window
		const bool bFocusOnAppearing = false;// ((Viewport->Flags & ImGuiViewportFlags_NoFocusOnAppearing) == 0);

		TSharedPtr<ImGuiUtils::SImGuiViewportWidget> ViewportWidget = SNew(ImGuiUtils::SImGuiViewportWidget, MainViewportWidgetPtr, Viewport);
		TSharedPtr<SWindow> ViewportWindow = 
			SNew(SWindow)
			.MinWidth(0.f)
			.MinHeight(0.f)
			.LayoutBorder({ 0 })
			.SizingRule(ESizingRule::FixedSize)
			.UserResizeBorder(FMargin(0))
			.HasCloseButton(false)
			.CreateTitleBar(false)
			.IsPopupWindow(bPopupWindow)
			.IsTopmostWindow(bTooltipWindow)
			.Type(bTooltipWindow ? EWindowType::ToolTip : (bPopupWindow ? EWindowType::Menu : EWindowType::Normal))
			.UseOSWindowBorder(false)
			.FocusWhenFirstShown(bFocusOnAppearing)
			.ActivationPolicy(bFocusOnAppearing ? EWindowActivationPolicy::Always : EWindowActivationPolicy::Never)
			.Content()
			[
				ViewportWidget.ToSharedRef()
			];

		if (TSharedPtr<SWindow> ParentWindow = ParentWindowPtr.Pin())
		{
			FSlateApplication::Get().AddWindowAsNativeChild(ViewportWindow.ToSharedRef(), ParentWindow.ToSharedRef(), /*bShowImmediately=*/true);
		}
		else
		{
			FSlateApplication::Get().AddWindow(ViewportWindow.ToSharedRef(), /*bShowImmediately=*/true);
		}

		FImGuiViewportData* ViewportData = IM_NEW(FImGuiViewportData)();
		ViewportData->ViewportWindow = ViewportWindow;
		ViewportData->ViewportWidget = ViewportWidget;
		ViewportData->ParentWindow = ParentWindowPtr;
		ViewportData->MainViewportWidget = MainViewportWidgetPtr;

		Viewport->PlatformRequestResize = false;
		Viewport->PlatformUserData = ViewportData;
	}

	static void UnrealPlatform_DestroyWindow(ImGuiViewport* Viewport)
	{
		if (FImGuiViewportData* ViewportData = (FImGuiViewportData*)Viewport->PlatformUserData)
		{
			if (TSharedPtr<SWindow> ViewportWindow = ViewportData->ViewportWindow.Pin())
			{
				ViewportWindow->RequestDestroyWindow();
			}
			ViewportData->ViewportWindow = nullptr;
			ViewportData->ViewportWidget = nullptr;
			ViewportData->ParentWindow = nullptr;
			ViewportData->MainViewportWidget = nullptr;
			IM_DELETE(ViewportData);

			Viewport->PlatformUserData = nullptr;
		}
	}

	static void UnrealPlatform_SetWindowPosition(ImGuiViewport* Viewport, ImVec2 NewPosition)
	{
		if (FImGuiViewportData* ViewportData = (FImGuiViewportData*)Viewport->PlatformUserData)
		{
			if (TSharedPtr<SWindow> ViewportWindow = ViewportData->ViewportWindow.Pin())
			{
				ViewportWindow->MoveWindowTo(FVector2f{ NewPosition.x, NewPosition.y });
			}
		}
	}

	static ImVec2 UnrealPlatform_GetWindowPosition(ImGuiViewport* Viewport)
	{
		if (FImGuiViewportData* ViewportData = (FImGuiViewportData*)Viewport->PlatformUserData)
		{
			if (ViewportData->ViewportWidget.IsValid())
			{
				FVector2f Position = ViewportData->ViewportWidget->GetCachedGeometry().GetAbsolutePosition();
				return ImVec2{ Position.X, Position.Y };
			}
			else if (TSharedPtr<SImGuiWidgetBase> MainViewportWidget = ViewportData->MainViewportWidget.Pin())
			{
				// NOTE: Rounding is required to fix 1px-offset issues when drawing the widget
				FVector2f Position = MainViewportWidget->GetCachedGeometry().GetAbsolutePosition();
				return ImVec2{ FMath::RoundToFloat(Position.X), FMath::RoundToFloat(Position.Y) };
			}
		}
		return ImVec2{ 0.f, 0.f };
	}

	static void UnrealPlatform_SetWindowSize(ImGuiViewport* Viewport, ImVec2 NewSize)
	{
		if (FImGuiViewportData* ViewportData = (FImGuiViewportData*)Viewport->PlatformUserData)
		{
			if (TSharedPtr<SWindow> ViewportWindow = ViewportData->ViewportWindow.Pin())
			{
				ViewportWindow->Resize(FVector2f{ NewSize.x, NewSize.y });
			}
		}
	}
	
	static ImVec2 UnrealPlatform_GetWindowSize(ImGuiViewport* Viewport)
	{
		if (FImGuiViewportData* ViewportData = (FImGuiViewportData*)Viewport->PlatformUserData)
		{
			if (TSharedPtr<SWindow> ViewportWindow = ViewportData->ViewportWindow.Pin())
			{
				const FVector2f WindowSize = ViewportWindow->GetSizeInScreen();
				return ImVec2{ WindowSize.X, WindowSize.Y };
			}
		}
		return ImVec2{ 0.f, 0.f };
	}

	static void UnrealPlatform_SetWindowTitle(ImGuiViewport* Viewport, const char* Title)
	{
		if (FImGuiViewportData* ViewportData = (FImGuiViewportData*)Viewport->PlatformUserData)
		{
			if (TSharedPtr<SWindow> ViewportWindow = ViewportData->ViewportWindow.Pin())
			{
				ViewportWindow->SetTitle(FText::FromString(ANSI_TO_TCHAR(Title)));
			}
		}
	}

	static void UnrealPlatform_ShowWindow(ImGuiViewport* Viewport)
	{
		if (FImGuiViewportData* ViewportData = (FImGuiViewportData*)Viewport->PlatformUserData)
		{
			if (TSharedPtr<SWindow> ViewportWindow = ViewportData->ViewportWindow.Pin())
			{
				ViewportWindow->ShowWindow();
			}
		}
	}

	static void UnrealPlatform_SetWindowFocus(ImGuiViewport* Viewport)
	{
		if (FImGuiViewportData* ViewportData = (FImGuiViewportData*)Viewport->PlatformUserData)
		{
			TSharedPtr<SWindow> ViewportWindow = ViewportData->ViewportWindow.Pin();
			TSharedPtr<FGenericWindow> NativeWindow = ViewportWindow ? ViewportWindow->GetNativeWindow() : nullptr;
			if (NativeWindow)
			{
				NativeWindow->SetWindowFocus();
			}
		}
	}

	static bool UnrealPlatform_GetWindowFocus(ImGuiViewport* Viewport)
	{
		if (FImGuiViewportData* ViewportData = (FImGuiViewportData*)Viewport->PlatformUserData)
		{
			// Special handling for the main viewport
			if ((Viewport->Flags & ImGuiViewportFlags_OwnedByApp) > 0)
			{
				TSharedPtr<SWindow> ViewportWindow = ViewportData->ParentWindow.Pin();
				TSharedPtr<FGenericWindow> NativeWindow = ViewportWindow ? ViewportWindow->GetNativeWindow() : nullptr;
				if (NativeWindow)
				{
					return NativeWindow->IsForegroundWindow();
				}
			}
			else
			{
				TSharedPtr<SWindow> ViewportWindow = ViewportData->ViewportWindow.Pin();
				TSharedPtr<FGenericWindow> NativeWindow = ViewportWindow ? ViewportWindow->GetNativeWindow() : nullptr;
				if (NativeWindow)
				{
					return NativeWindow->IsForegroundWindow();
				}
			}
		}
		return false;
	}

	static bool UnrealPlatform_GetWindowMinimized(ImGuiViewport* Viewport)
	{
		if (FImGuiViewportData* ViewportData = (FImGuiViewportData*)Viewport->PlatformUserData)
		{
			if (TSharedPtr<SWindow> ViewportWindow = ViewportData->ViewportWindow.Pin())
			{
				return ViewportWindow->IsWindowMinimized();
			}
		}
		return false;
	}

	static void UnrealPlatform_SetWindowAlpha(ImGuiViewport* Viewport, float Alpha)
	{
		if (FImGuiViewportData* ViewportData = (FImGuiViewportData*)Viewport->PlatformUserData)
		{
			if (TSharedPtr<SWindow> ViewportWindow = ViewportData->ViewportWindow.Pin())
			{
				ViewportWindow->SetOpacity(Alpha);
			}
		}
	}

	static void UnrealPlatform_UpdateWindow(ImGuiViewport* Viewport)
	{
		if (FImGuiViewportData* ViewportData = (FImGuiViewportData*)Viewport->PlatformUserData)
		{
			if (ViewportData->ViewportWidget.IsValid())
			{
				// window was destroyed by platform, happens when MainViewportWindow is dragged invalidating all child windows
				bool bInvalidateWindow = !ViewportData->ViewportWindow.IsValid();

#if WITH_EDITOR
				// recreate window if parent viewport requested it, needed for patching parent slate window reference after docking/undocking tabs
				if (ImGuiViewport* ParentViewport = ImGui::FindViewportByID(Viewport->ParentViewportId))
				{
					FImGuiViewportData* ParentViewportData = (FImGuiViewportData*)ParentViewport->PlatformUserData;
					if (ParentViewportData && ParentViewportData->bInvalidateManagedViewportWindows)
					{
						bInvalidateWindow = true;
					}
				}
#endif

				if (bInvalidateWindow)
				{
					// TODO: maybe not ideal to access viewport as 'ImGuiViewportP', but there doesn't seem to be a way to request window recreation
					ImGui::DestroyPlatformWindow((ImGuiViewportP*)Viewport);
				}
			}
		}
	}

	static void UnrealPlatform_RenderWindow(ImGuiViewport* Viewport, void*)
	{
		if (FImGuiViewportData* ViewportData = (FImGuiViewportData*)Viewport->PlatformUserData)
		{
			if (Viewport->DrawData && ViewportData->ViewportWidget.IsValid())
			{
				ViewportData->ViewportWidget->OnDrawDataGenerated(Viewport->DrawData);
			}
		}
	}

	static const char* UnrealPlatform_GetClipboardText(ImGuiContext* Context)
	{
		FAnsiString* ClipboardText = static_cast<FAnsiString*>(Context->PlatformIO.Platform_ClipboardUserData);
		if (ClipboardText)
		{
			FString Content;
			FPlatformApplicationMisc::ClipboardPaste(Content);

			*ClipboardText = TCHAR_TO_UTF8(*Content);

			return ClipboardText->GetCharArray().GetData();
		}

		return nullptr;
	}

	static void UnrealPlatform_SetClipboardText(ImGuiContext* Context, const char* ClipboardText)
	{
		FPlatformApplicationMisc::ClipboardCopy(UTF8_TO_TCHAR(ClipboardText));
	}

	static bool UnrealPlatform_OpenInShell(ImGuiContext* Context, const char* Path)
	{
		return FPlatformProcess::LaunchFileInDefaultExternalApplication(UTF8_TO_TCHAR(Path));
	}
}
