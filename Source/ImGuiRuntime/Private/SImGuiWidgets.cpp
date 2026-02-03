// Copyright 2024-25 Amit Kumar Mehar. All Rights Reserved.

#include "SImGuiWidgets.h"

#include "RHI.h"
#include "RHIStaticStates.h"
#include "RenderGraphUtils.h"
#include "GlobalRenderResources.h"
#include "CommonRenderResources.h"
#include "SlateUTextureResource.h"
#include "RenderCaptureInterface.h"
#include "Rendering/RenderingCommon.h"

#include "Input/Events.h"
#include "InputCoreTypes.h"
#include "Widgets/SWindow.h"
#include "HAL/PlatformApplicationMisc.h"
#include "Application/ThrottleManager.h"
#include "Runtime/Launch/Resources/Version.h"
#include "Framework/Application/SlateApplication.h"

#include "ImGuiShaders.h"
#include "ImGuiSubsystem.h"
#include "imgui_threaded_rendering.h"
#include "ImGuiViewportUtils.inl"

void SImGuiWidgetBase::Construct(const FArguments& InArgs)
{
	UImGuiSubsystem* ImGuiSubsystem = UImGuiSubsystem::Get();

	m_ImGuiContext = ImGui::CreateContext(ImGuiSubsystem->GetSharedFontAtlas());
	m_WidgetDrawers[0] = MakeShared<ImGuiUtils::FWidgetDrawer>();
	m_WidgetDrawers[1] = MakeShared<ImGuiUtils::FWidgetDrawer>();

	m_TickContext = MakeUnique<FImGuiTickContext>();
	m_TickContext->ImguiContext = m_ImGuiContext;

	if (InArgs._ConfigFileName && FCStringAnsi::Strlen(InArgs._ConfigFileName) > 2)
	{
		// sanitize filename
		FAnsiString FileName = InArgs._ConfigFileName;
		FileName.RemoveSpacesInline();

		m_ConfigFilePath = FAnsiString::Printf("%s/%s.ini", ImGuiSubsystem->GetIniDirectoryPath(), *FileName);
	}

	ImGuiIO& IO = GetImGuiIO();
	IO.UserData = m_TickContext.Get();
	IO.IniFilename = m_ConfigFilePath.IsEmpty() ? nullptr : *m_ConfigFilePath;

	IO.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
	IO.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
	IO.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;

	IO.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset;
	IO.BackendFlags |= ImGuiBackendFlags_RendererHasTextures;

	if (InArgs._bEnableViewports)
	{
		IO.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
		IO.BackendFlags |= ImGuiBackendFlags_RendererHasViewports;
		IO.BackendFlags |= ImGuiBackendFlags_PlatformHasViewports;
	}

	ImGuiPlatformIO& PlatformIO = ImGui::GetPlatformIO();

#ifdef IMGUI_DISABLE_WIN32_DEFAULT_CLIPBOARD_FUNCTIONS
	PlatformIO.Platform_ClipboardUserData = &ClipboardText;
	PlatformIO.Platform_GetClipboardTextFn = ImGuiUtils::UnrealPlatform_GetClipboardText;
	PlatformIO.Platform_SetClipboardTextFn = ImGuiUtils::UnrealPlatform_SetClipboardText;
#endif

#ifdef IMGUI_DISABLE_DEFAULT_SHELL_FUNCTIONS
	PlatformIO.Platform_OpenInShellFn = ImGuiUtils::UnrealPlatform_OpenInShell;
#endif

	// viewport setup
	if ((IO.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) > 0)
	{
		PlatformIO.Platform_CreateWindow		= ImGuiUtils::UnrealPlatform_CreateWindow;
		PlatformIO.Platform_DestroyWindow		= ImGuiUtils::UnrealPlatform_DestroyWindow;
		PlatformIO.Platform_SetWindowPos		= ImGuiUtils::UnrealPlatform_SetWindowPosition;
		PlatformIO.Platform_GetWindowPos		= ImGuiUtils::UnrealPlatform_GetWindowPosition;
		PlatformIO.Platform_SetWindowSize		= ImGuiUtils::UnrealPlatform_SetWindowSize;
		PlatformIO.Platform_GetWindowSize		= ImGuiUtils::UnrealPlatform_GetWindowSize;
		PlatformIO.Platform_SetWindowTitle		= ImGuiUtils::UnrealPlatform_SetWindowTitle;
		PlatformIO.Platform_ShowWindow			= ImGuiUtils::UnrealPlatform_ShowWindow;
		PlatformIO.Platform_SetWindowFocus		= ImGuiUtils::UnrealPlatform_SetWindowFocus;
		PlatformIO.Platform_GetWindowFocus		= ImGuiUtils::UnrealPlatform_GetWindowFocus;
		PlatformIO.Platform_GetWindowMinimized	= ImGuiUtils::UnrealPlatform_GetWindowMinimized;
		PlatformIO.Platform_SetWindowAlpha		= ImGuiUtils::UnrealPlatform_SetWindowAlpha;
		PlatformIO.Platform_UpdateWindow		= ImGuiUtils::UnrealPlatform_UpdateWindow;
		PlatformIO.Platform_RenderWindow		= ImGuiUtils::UnrealPlatform_RenderWindow;
		PlatformIO.Platform_OnChangedViewport	= nullptr;

		FDisplayMetrics DisplayMetrics;
		FDisplayMetrics::RebuildDisplayMetrics(DisplayMetrics);
		for (const FMonitorInfo& MonitorInfo : DisplayMetrics.MonitorInfo)
		{
			ImGuiPlatformMonitor ImguiMonitor;
			ImguiMonitor.MainPos = ImVec2((float)MonitorInfo.DisplayRect.Left, (float)MonitorInfo.DisplayRect.Top);
			ImguiMonitor.MainSize = ImVec2((float)(MonitorInfo.DisplayRect.Right - MonitorInfo.DisplayRect.Left),
				(float)(MonitorInfo.DisplayRect.Bottom - MonitorInfo.DisplayRect.Top));
			ImguiMonitor.WorkPos = ImVec2((float)MonitorInfo.WorkArea.Left, (float)MonitorInfo.WorkArea.Top);
			ImguiMonitor.WorkSize = ImVec2((float)(MonitorInfo.WorkArea.Right - MonitorInfo.WorkArea.Left),
				(float)(MonitorInfo.WorkArea.Bottom - MonitorInfo.WorkArea.Top));
			ImguiMonitor.DpiScale = MonitorInfo.DPI / 96.f;
			ImguiMonitor.PlatformHandle = nullptr;

			if (MonitorInfo.bIsPrimary)
			{
				PlatformIO.Monitors.push_front(ImguiMonitor);
			}
			else
			{
				PlatformIO.Monitors.push_back(ImguiMonitor);
			}
		}

		ImGuiUtils::FImGuiViewportData* MainViewportData = IM_NEW(ImGuiUtils::FImGuiViewportData)();
		MainViewportData->ViewportWindow = nullptr;
		MainViewportData->ViewportWidget = nullptr;
		MainViewportData->ParentWindow = InArgs._MainViewportWindow;
		MainViewportData->MainViewportWidget = StaticCastWeakPtr<SImGuiWidgetBase>(AsShared().ToWeakPtr());

		ImGuiViewport* MainViewport = ImGui::GetMainViewport();
		MainViewport->PlatformUserData = MainViewportData;
	}

	// TODO: setting?
	ImGui::StyleColorsDark();
}

SImGuiWidgetBase::~SImGuiWidgetBase()
{
	// cleanup references to this widget
	{
		ImGuiIO& IO = GetImGuiIO();
		IO.UserData = nullptr;

		// duplicate of context shutdown operation as we clear the IniFilename here
		if (m_ImGuiContext->SettingsLoaded && IO.IniFilename)
		{
			ImGui::SaveIniSettingsToDisk(IO.IniFilename);
		}
		IO.IniFilename = nullptr;

		ImGuiPlatformIO& PlatformIO = ImGui::GetPlatformIO();
		PlatformIO.Platform_ClipboardUserData = nullptr;
	}

	// NOTE: widget drawers should be queued before context
	ImGuiUtils::DeferredDeletionQueue.DeferredDeleteObjects(MoveTemp(m_WidgetDrawers[0]), MoveTemp(m_WidgetDrawers[1]), m_ImGuiContext);
	m_ImGuiContext = nullptr;
}

ImGuiIO& SImGuiWidgetBase::GetImGuiIO() const
{
	checkf(m_ImGuiContext, TEXT("ImGuiContext is invalid!"));

	ImGui::SetCurrentContext(m_ImGuiContext);
	return ImGui::GetIO();
}

void SImGuiWidgetBase::BeginImGuiFrame(const FGeometry& WidgetGeometry, float DeltaTime)
{
	if (m_ImGuiContext->WithinFrameScope)
	{
		return;
	}

	TSharedPtr<FDragDropOperation> CurrentDragDropOperation = FSlateApplication::Get().GetDragDroppingContent();
	if (LastDragDropOperation.IsValid() && !CurrentDragDropOperation.IsValid())
	{
		m_TickContext->bDragDropOperationReleasedThisFrame = true;
		m_TickContext->DragDropOperation = LastDragDropOperation;
	}
	else
	{
		m_TickContext->bDragDropOperationReleasedThisFrame = false;
		m_TickContext->DragDropOperation = MoveTemp(CurrentDragDropOperation);
	}
	LastDragDropOperation.Reset();

	{
		ImGuiIO& IO = GetImGuiIO();

		FVector2f WidgetSize = WidgetGeometry.GetAbsoluteSize();
		IO.DisplaySize = ImVec2(WidgetSize.X, WidgetSize.Y);
		IO.DeltaTime = DeltaTime;

		ImGui::NewFrame();

		if (m_TickContext->DragDropOperation.IsValid() && m_IsDragOverActive)
		{
			// disable widgets from reacting to mouse events (hover/tooltips etc)
			ImGui::SetActiveID(-1, nullptr);
		}
	}
}

void SImGuiWidgetBase::EndImGuiFrame()
{
	if (!m_ImGuiContext->WithinFrameScope)
	{
		return;
	}

	// the widget was not rendered this frame
	ImGui::SetCurrentContext(m_ImGuiContext);
	ImGui::EndFrame();
	ImGui::UpdatePlatformWindows();
}

void SImGuiWidgetBase::Tick(const FGeometry& WidgetGeometry, const double CurrentTime, const float DeltaTime)
{
	DECLARE_SCOPE_CYCLE_COUNTER(TEXT("Tick Widget"), STAT_ImGui_TickWidget, STATGROUP_ImGui);

	Super::Tick(WidgetGeometry, CurrentTime, DeltaTime);

	BeginImGuiFrame(WidgetGeometry, DeltaTime);

	TickImGuiInternal(m_TickContext.Get());
}

int32 SImGuiWidgetBase::OnPaint(const FPaintArgs& Args, const FGeometry& WidgetGeometry, const FSlateRect& ClippingRect,
	FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& WidgetStyle, bool bParentEnabled) const
{
	DECLARE_SCOPE_CYCLE_COUNTER(TEXT("Render Widget [GT]"), STAT_ImGui_RenderWidget_GT, STATGROUP_ImGui);

	if (!m_ImGuiContext->WithinFrameScope)
	{
		return LayerId;
	}

	ImGuiIO& IO = GetImGuiIO();
	ImGui::Render();

	const FSlateRect DrawRect = WidgetGeometry.GetRenderBoundingRect();

	TSharedPtr<ImGuiUtils::FWidgetDrawer> WidgetDrawer = m_WidgetDrawers[ImGui::GetFrameCount() & 0x1];
	if (WidgetDrawer->SetDrawData(ImGui::GetDrawData(), ImGui::GetFrameCount(), DrawRect.GetTopLeft2f()))
	{
		OutDrawElements.PushClip(FSlateClippingZone{ ClippingRect });
		FSlateDrawElement::MakeCustom(OutDrawElements, LayerId, WidgetDrawer);
		OutDrawElements.PopClip();
	}

	if ((IO.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) > 0)
	{
		ImGuiViewport* MainViewport = ImGui::GetMainViewport();
		ImGuiUtils::FImGuiViewportData* ViewportData = (ImGuiUtils::FImGuiViewportData*)MainViewport->PlatformUserData;

		// TODO: maybe find a better way to detect window docking operations? This is not expensive, just a bit ugly!
		TSharedPtr<SWindow> PreviousParentWindow = ViewportData->ParentWindow.Pin();
		TSharedPtr<SWindow> CurrentParentWindow = FSlateApplication::Get().FindWidgetWindow(AsShared());
		if (ViewportData && (!PreviousParentWindow || (PreviousParentWindow != CurrentParentWindow)))
		{
			ViewportData->ParentWindow = CurrentParentWindow;
			ViewportData->bInvalidateManagedViewportWindows = true;
		}

		ImGui::UpdatePlatformWindows();
		ImGui::RenderPlatformWindowsDefault();

		if (ViewportData)
		{
			ViewportData->bInvalidateManagedViewportWindows = false;
		}
	}

#if WITH_EDITOR
	m_LastPaintFrameCounter = GFrameCounter;
#endif
	
	return LayerId;
}

#pragma region SLATE_INPUT
void SImGuiWidgetBase::AddKeyEvent(ImGuiIO& IO, FKeyEvent KeyEvent, bool IsDown)
{
	const ImGuiKey ImGuiKey = ImGuiUtils::ImGuiToUnrealKey(KeyEvent.GetKey().GetFName());
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

FReply SImGuiWidgetBase::OnMouseButtonUp(const FGeometry& WidgetGeometry, const FPointerEvent& MouseEvent)
{
	ImGuiIO& IO = GetImGuiIO();
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

FReply SImGuiWidgetBase::OnMouseButtonDoubleClick(const FGeometry& WidgetGeometry, const FPointerEvent& MouseEvent)
{
	ImGuiIO& IO = GetImGuiIO();
	IO.AddMouseButtonEvent(ImGuiUtils::UnrealToImGuiMouseButton(MouseEvent.GetEffectingButton()), /*down=*/true);

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
	ImGuiIO& IO = GetImGuiIO();	
	FVector2f MousePosition = MouseEvent.GetScreenSpacePosition();
	if ((IO.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) == 0)
	{
		MousePosition = WidgetGeometry.AbsoluteToLocal(MousePosition);
	}
	IO.AddMousePosEvent(MousePosition.X, MousePosition.Y);

	return FReply::Handled();
}

FCursorReply SImGuiWidgetBase::OnCursorQuery(const FGeometry& WidgetGeometry, const FPointerEvent& CursorEvent) const
{
	ImGuiIO& IO = GetImGuiIO();
	const ImGuiMouseCursor MouseCursor = ImGui::GetMouseCursor();
	return (MouseCursor == ImGuiMouseCursor_None) ? FCursorReply::Unhandled() : FCursorReply::Cursor(ImGuiUtils::ImGuiToUnrealCursor(MouseCursor));
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

	ImGuiIO& IO = GetImGuiIO();
	FVector2f MousePosition = DragDropEvent.GetScreenSpacePosition();
	if ((IO.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) == 0)
	{
		MousePosition = WidgetGeometry.AbsoluteToLocal(MousePosition);
	}
	IO.AddMousePosEvent(MousePosition.X, MousePosition.Y);

	// NOTE: this is incorrect but needed to receive the OnDrop event :(
	return FReply::Handled();
}

FReply SImGuiWidgetBase::OnDrop(const FGeometry& WidgetGeometry, const FDragDropEvent& DragDropEvent)
{
	m_IsDragOverActive = false;
	LastDragDropOperation = DragDropEvent.GetOperation();

	// NOTE: this is incorrect but better than having drag drop events pass through to the viewpport.
	// probably only an issue when the widget is placed directly on the viewport (have to fix input events for that at some point...)
	return FReply::Handled();
}
#pragma endregion SLATE_INPUT

void SImGuiWidget::Construct(const FArguments& InArgs)
{
	Super::Construct(
		Super::FArguments()
		.MainViewportWindow(InArgs._MainViewportWindow)
		.ConfigFileName(InArgs._ConfigFileName)
		.bEnableViewports(InArgs._bEnableViewports));

	m_OnTickDelegate = InArgs._OnTickDelegate;
}

void SImGuiWidget::TickImGuiInternal(FImGuiTickContext* TickContext)
{
	m_OnTickDelegate.ExecuteIfBound(TickContext);
}
