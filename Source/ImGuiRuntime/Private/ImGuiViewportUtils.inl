// Copyright 2024-25 Amit Kumar Mehar. All Rights Reserved.

namespace ImGuiUtils
{
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
			m_BoundRenderResources.Reset();
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
			m_BoundRenderResources.Reset(ImGuiSubsystem->GetOneFrameResources().Num());

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
				bool bAdded = false;
				ON_SCOPE_EXIT
				{
					if (!bAdded)
					{
						m_BoundRenderResources.Emplace(TInPlaceType<FTextureResource*>(), nullptr);
					}
				};

				FSlateShaderResource* ShaderResource = TextureResource.GetSlateShaderResource();
				if (ShaderResource)
				{
					ESlateShaderResource::Type ResourceType = ShaderResource->GetType();

					if (ResourceType == ESlateShaderResource::Type::TextureObject)
					{
						FSlateBaseUTextureResource* TextureObjectResource = static_cast<FSlateBaseUTextureResource*>(ShaderResource);
						if (FTextureResource* Resource = TextureObjectResource->GetTextureObject()->GetResource())
						{
							bAdded = true;
							m_BoundRenderResources.Emplace(TInPlaceType<FTextureResource*>(), Resource);
						}
					}
					else if (ResourceType == ESlateShaderResource::Type::NativeTexture)
					{
						bAdded = true;
						m_BoundRenderResources.Emplace(TInPlaceType<FSlateShaderResource*>(), ShaderResource);
					}
				}
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

		virtual void Draw_RenderThread(FRDGBuilder& GraphBuilder, const FDrawPassInputs& Inputs) override
		{
			DECLARE_SCOPE_CYCLE_COUNTER(TEXT("Render Widget [RT]"), STAT_ImGui_RenderWidget_RT, STATGROUP_ImGui);

			FRenderParameters* PassParameters = GraphBuilder.AllocParameters<FRenderParameters>();
			PassParameters->RenderTargets[0] = FRenderTargetBinding(Inputs.OutputTexture, ERenderTargetLoadAction::ELoad);

			GraphBuilder.AddPass(RDG_EVENT_NAME("RenderImGui"), PassParameters, ERDGPassFlags::Raster | ERDGPassFlags::NeverCull,
				[this](FRHICommandListImmediate& RHICmdList)
				{
					const ImDrawData* DrawData = &m_DrawDataSnapshot.DrawData;

					const ImRect DrawRect = ImRect(
						ImVec2(m_DrawRectOffset.X, m_DrawRectOffset.Y),
						ImVec2(m_DrawRectOffset.X, m_DrawRectOffset.Y) + DrawData->DisplaySize);
					const ImRect ViewportRect = ImRect(
						FMath::RoundToFloat(DrawRect.Min.x), FMath::RoundToFloat(DrawRect.Min.y),
						FMath::RoundToFloat(DrawRect.Max.x), FMath::RoundToFloat(DrawRect.Max.y));

					const ImVec2 DisplayPos = ImVec2(FMath::RoundToFloat(DrawData->DisplayPos.x), FMath::RoundToFloat(DrawData->DisplayPos.y));
					const ImVec2 DisplaySize = ViewportRect.GetSize();

					m_BoundTextures.Reset(m_BoundRenderResources.Num());
					for (const auto& RenderResource : m_BoundRenderResources)
					{
						auto& TextureInfo = m_BoundTextures.AddDefaulted_GetRef();
						if (RenderResource.IsType<FTextureResource*>())
						{
							if (const FTextureResource* TextureResource = RenderResource.Get<FTextureResource*>())
							{
								TextureInfo.TextureRHI = TextureResource->TextureRHI;
								TextureInfo.SamplerRHI = TextureResource->SamplerStateRHI;
								TextureInfo.IsSRGB = TextureResource->bSRGB;
							}
						}
						else if (RenderResource.IsType<FSlateShaderResource*>())
						{
							const FSlateShaderResource* ShaderResource = RenderResource.Get<FSlateShaderResource*>();
							if (FRHITexture* NativeTextureRHI = ((TSlateTexture<FTextureRHIRef>*)ShaderResource)->GetTypedResource())
							{
								TextureInfo.TextureRHI = NativeTextureRHI;
								TextureInfo.IsSRGB = EnumHasAnyFlags(NativeTextureRHI->GetFlags(), ETextureCreateFlags::SRGB);
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

						auto UpdateVertexShaderParameters = [&]()
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

								SetShaderParametersLegacyVS(
									RHICmdList,
									VertexShader,
									ProjectionMatrix);
							};

						uint32_t ShaderStateOverrides = 0;

						RHICmdList.SetViewport(ViewportRect.Min.x, ViewportRect.Min.y, 0.f, ViewportRect.Max.x, ViewportRect.Max.y, 1.f);
						RHICmdList.SetScissorRect(false, 0.f, 0.f, 0.f, 0.f);

						UpdateVertexShaderParameters();

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

										UpdateVertexShaderParameters();
									}
									else if (DrawCmd.UserCallback == ImDrawCallback_SetShaderState)
									{
										ShaderStateOverrides = static_cast<uint32>(reinterpret_cast<uintptr_t>(DrawCmd.UserCallbackData));

										//UpdateVertexShaderParameters(); No VS state exposed atm.
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

											UpdateVertexShaderParameters();
										}
									}
								}
								else
								{
									const ImVec2& ClipRectOffset = DisplayPos;
									RHICmdList.SetScissorRect(
										true,
										FMath::Max(0.f, DrawCmd.ClipRect.x - ClipRectOffset.x) + ViewportRect.Min.x,
										FMath::Max(0.f, DrawCmd.ClipRect.y - ClipRectOffset.y) + ViewportRect.Min.y,
										FMath::Min(ViewportRect.Max.x, DrawCmd.ClipRect.z - ClipRectOffset.x + ViewportRect.Min.x),
										FMath::Min(ViewportRect.Max.y, DrawCmd.ClipRect.w - ClipRectOffset.y + ViewportRect.Min.y));

									uint32 TextureIndex = UImGuiSubsystem::ImGuiIDToIndex(DrawCmd.GetTexID());
									if (!(m_BoundTextures.IsValidIndex(TextureIndex)/* && RenderData.BoundTextures[Index].IsValid()*/))
									{
										TextureIndex = UImGuiSubsystem::GetMissingImageTextureIndex();
									}

									SetShaderParametersLegacyPS(
										RHICmdList,
										PixelShader,
										m_BoundTextures[TextureIndex].TextureRHI,
										m_BoundTextures[TextureIndex].SamplerRHI,
										m_BoundTextures[TextureIndex].IsSRGB,
										ShaderStateOverrides);

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
		struct FTextureInfo
		{
			FTextureRHIRef TextureRHI = nullptr;
			FSamplerStateRHIRef SamplerRHI = nullptr;
			bool IsSRGB = false;
		};
		TArray<FTextureInfo> m_BoundTextures;
		TArray<TVariant<FTextureResource*, FSlateShaderResource*>> m_BoundRenderResources;
		FVector2f m_DrawRectOffset = FVector2f::ZeroVector;
		ImDrawDataSnapshot m_DrawDataSnapshot;
		bool m_bHasDrawCommands = false;
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
}
