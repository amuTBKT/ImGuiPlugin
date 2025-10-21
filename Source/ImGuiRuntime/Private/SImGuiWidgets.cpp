// Copyright 2024-25 Amit Kumar Mehar. All Rights Reserved.

#include "SImGuiWidgets.h"
#include "ImGuiShaders.h"
#include "ImGuiSubsystem.h"
#include "Widgets/SWindow.h"

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

#include "imgui_threaded_rendering.h"
#include "ImGuiViewportUtils.inl"

void SImGuiWidgetBase::Construct(const FArguments& InArgs)
{
	UImGuiSubsystem* ImGuiSubsystem = UImGuiSubsystem::Get();

	m_ImGuiContext = ImGui::CreateContext(ImGuiSubsystem->GetSharedFontAtlas());
	m_DrawDataSnapshot = IM_NEW(ImDrawDataSnapshot)();

	if (InArgs._ConfigFileName && FCStringAnsi::Strlen(InArgs._ConfigFileName) > 2)
	{
		// sanitize filename
		FAnsiString FileName = InArgs._ConfigFileName;
		FileName.RemoveSpacesInline();

		m_ConfigFilePath = FAnsiString::Printf("%s/%s.ini", ImGuiSubsystem->GetIniDirectoryPath(), *FileName);
	}
	else
	{
		m_ConfigFilePath = ImGuiSubsystem->GetIniFilePath();
	}

	ImGuiIO& IO = GetImGuiIO();
	IO.IniFilename = *m_ConfigFilePath;

	IO.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
	IO.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
	IO.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
	IO.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;

	IO.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset;
	IO.BackendFlags |= ImGuiBackendFlags_RendererHasTextures;
	IO.BackendFlags |= ImGuiBackendFlags_RendererHasViewports;
	IO.BackendFlags |= ImGuiBackendFlags_PlatformHasViewports;

	// viewport setup
	if ((IO.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) > 0)
	{
		ImGuiPlatformIO& PlatformIO = ImGui::GetPlatformIO();
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
		MainViewportData->MainViewportWindow = InArgs._MainViewportWindow;
		MainViewportData->MainViewportWidget = StaticCastWeakPtr<SImGuiWidgetBase>(AsShared().ToWeakPtr());

		ImGuiViewport* MainViewport = ImGui::GetMainViewport();
		MainViewport->PlatformUserData = MainViewportData;
	}

	// TODO: setting?
	ImGui::StyleColorsDark();

	// allocate rendering resources
	m_ImGuiRT = NewObject<UTextureRenderTarget2D>();
	m_ImGuiRT->Filter = TextureFilter::TF_Nearest;
	m_ImGuiRT->RenderTargetFormat = ETextureRenderTargetFormat::RTF_RGBA8;
	m_ImGuiRT->ClearColor = FLinearColor(0.f, 0.f, 0.f, 0.f);
	m_ImGuiRT->InitAutoFormat(1, 1);
	m_ImGuiRT->UpdateResourceImmediate(true);

	m_ImGuiSlateBrush.SetResourceObject(m_ImGuiRT);

	// only need to clear the RT when using translucent window, otherwise ImGui fullscreen widget pass should clear the RT.
	m_ClearRenderTargetEveryFrame = (InArgs._bUseOpaqueBackground == false);
}

SImGuiWidgetBase::~SImGuiWidgetBase()
{
	if (m_ImGuiContext)
	{
		ImGuiIO& IO = GetImGuiIO();
		ImGui::DestroyPlatformWindows();

		IM_DELETE(m_DrawDataSnapshot);
		m_DrawDataSnapshot = nullptr;

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
	DECLARE_SCOPE_CYCLE_COUNTER(TEXT("Render Widget [GT]"), STAT_ImGui_RenderWidget_GT, STATGROUP_ImGui);

	UImGuiSubsystem* ImGuiSubsystem = UImGuiSubsystem::Get();

	ImGuiIO& IO = GetImGuiIO();

	ImGui::Render();

	m_DrawDataSnapshot->SnapUsingSwap(ImGui::GetDrawData(), ImGui::GetFrameCount());

	for (ImTextureData* TexData : *m_DrawDataSnapshot->DrawData.Textures)
	{
		ImGuiSubsystem->UpdateTextureData(TexData);
	}

	if (ImGuiUtils::RenderImGuiWidgetToRenderTarget(&m_DrawDataSnapshot->DrawData, m_ImGuiRT.Get(), m_ClearRenderTargetEveryFrame))
	{
		const FSlateRenderTransform WidgetOffsetTransform = FTransform2f(1.f, { 0.f, 0.f });
		const FSlateRect DrawRect = WidgetGeometry.GetRenderBoundingRect();

		const FVector2f V0 = DrawRect.GetTopLeft();
		const FVector2f V1 = DrawRect.GetTopRight();
		const FVector2f V2 = DrawRect.GetBottomRight();
		const FVector2f V3 = DrawRect.GetBottomLeft();

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

	if ((IO.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) > 0)
	{
		ImGuiViewport* MainViewport = ImGui::GetMainViewport();
		ImGuiUtils::FImGuiViewportData* ViewportData = (ImGuiUtils::FImGuiViewportData*)MainViewport->PlatformUserData;
		if (ViewportData && !ViewportData->MainViewportWindow.IsValid())
		{
			ViewportData->MainViewportWindow = FSlateApplication::Get().FindWidgetWindow(AsShared());
		}

		ImGui::UpdatePlatformWindows();
		ImGui::RenderPlatformWindowsDefault();
	}
	
	return LayerId;
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

	if (m_ImGuiTickedByInputProcessing)
	{
		// dummy ImGui render in case slate widget was throttled
		ImGuiIO& IO = GetImGuiIO();
		ImGui::EndFrame();
		ImGui::UpdatePlatformWindows();

		m_ImGuiTickedByInputProcessing = false;
	}

	ImGuiIO& IO = GetImGuiIO();
	FVector2f MousePosition = DragDropEvent.GetScreenSpacePosition();
	if ((IO.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) == 0)
	{
		MousePosition = WidgetGeometry.AbsoluteToLocal(MousePosition);
	}
	IO.AddMousePosEvent(MousePosition.X, MousePosition.Y);

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
		ImGui::EndFrame();
		ImGui::UpdatePlatformWindows();

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
	Super::Construct(
		Super::FArguments()
		.MainViewportWindow(InArgs._MainViewportWindow));
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
	Super::Construct(
		Super::FArguments()
		.MainViewportWindow(InArgs._MainViewportWindow)
		.ConfigFileName(InArgs._ConfigFileName)
		.bUseOpaqueBackground(InArgs._bUseOpaqueBackground));

	m_OnTickDelegate = InArgs._OnTickDelegate;
}

void SImGuiWidget::TickImGuiInternal(FImGuiTickContext* TickContext)
{
	m_OnTickDelegate.ExecuteIfBound(TickContext);
}
