// Copyright 2024-26 Amit Kumar Mehar. All Rights Reserved.

namespace ImGuiUtils
{
	class FWidgetDrawer;
	struct FDeferredDeletionQueue
	{
		FDeferredDeletionQueue()
		{
			UImGuiSubsystem::OnBeginImGuiFrame.AddRaw(this, &FDeferredDeletionQueue::ProcessObjects, /*bForceDestroy=*/false);
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

	class SImGuiViewportWidget;
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

		// flag to set focus on widget next time viewport updates
		// kept it separate from widget logic as viewport widgets don't tick
		bool bFocusRequested = false;

		// handle to window closed event
		// to cleanup ImGui viewport when Slate window is manually closed
		FDelegateHandle OnWindowClosedHandle{};
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
			const FImGuiViewportData* ViewportData = m_ImGuiViewport ? (FImGuiViewportData*)m_ImGuiViewport->PlatformUserData : nullptr;
			if (ViewportData && ViewportData->ViewportWindow.IsValid())
			{
				TSharedPtr<SWindow> ViewportWindow = ViewportData->ViewportWindow.Pin();
				TSharedPtr<SImGuiWidgetBase> RootWidget = m_MainViewportWidget.Pin();

				const bool bNewVisibility = RootWidget ? (RootWidget->GetLastPaintFrameCounter() >= GFrameCounter) : false;
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

			TSharedPtr<ImGuiUtils::FWidgetDrawer> WidgetDrawer = m_WidgetDrawers[WidgetDrawerToRenderThisFrame];
			if (WidgetDrawer->HasDrawCommands())
			{
				const FSlateRect DrawRect = WidgetGeometry.GetRenderBoundingRect();
				WidgetDrawer->SetDrawRectOffset(DrawRect.GetTopLeft2f());

				OutDrawElements.PushClip(FSlateClippingZone{ ClippingRect });
#if WITH_ENGINE
				FSlateDrawElement::MakeCustom(OutDrawElements, LayerId, WidgetDrawer);
#else
				WidgetDrawer->DrawSlateWidget(WidgetGeometry, OutDrawElements, LayerId);
#endif
				OutDrawElements.PopClip();
			}
			return LayerId;
		}

		virtual bool SupportsKeyboardFocus() const override { return true; }

		void OnDrawDataGenerated(ImDrawData* DrawData)
		{
			// it's unsafe to make ImGui calls during OnPaint() so cache the drawer index here
			WidgetDrawerToRenderThisFrame = ImGui::GetFrameCount() & 0x1;
			m_WidgetDrawers[WidgetDrawerToRenderThisFrame]->SetDrawData(DrawData, ImGui::GetTime(), FVector2f::ZeroVector);
		}

		virtual FReply OnFocusReceived(const FGeometry& MyGeometry, const FFocusEvent& InFocusEvent) override
		{
			TSharedPtr<SImGuiWidgetBase> RootWidget = m_MainViewportWidget.Pin();
			return RootWidget ? RootWidget->OnFocusReceived(RootWidget->GetCachedGeometry(), InFocusEvent) : FReply::Unhandled();
		}

		virtual void OnFocusLost(const FFocusEvent& InFocusEvent) override
		{
			TSharedPtr<SImGuiWidgetBase> RootWidget = m_MainViewportWidget.Pin();
			if (RootWidget)
			{
				RootWidget->OnFocusLost(InFocusEvent);
			}
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

			ImGuiIO& IO = RootWidget->GetImGuiContext()->IO;
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

			ImGuiIO& IO = RootWidget->GetImGuiContext()->IO;
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

		void ResetImGuiViewportData()
		{
			m_ImGuiViewport = nullptr;
		}

	private:
		const ImGuiViewport* m_ImGuiViewport = nullptr;
		TSharedPtr<ImGuiUtils::FWidgetDrawer> m_WidgetDrawers[2];
		TWeakPtr<SImGuiWidgetBase> m_MainViewportWidget = nullptr;
		int32 WidgetDrawerToRenderThisFrame = 0;
	};
}
