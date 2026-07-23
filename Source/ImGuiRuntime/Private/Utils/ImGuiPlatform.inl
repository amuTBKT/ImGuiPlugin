// Copyright 2024-26 Amit Kumar Mehar. All Rights Reserved.

#include "Containers/StringConv.h"
#include "HAL/PlatformFileManager.h"
#include "HAL/PlatformApplicationMisc.h"
#include "GenericPlatform/GenericPlatformFile.h"

namespace ImGuiUtils
{
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

		bool bTooltipWindow = (Viewport->Flags & ImGuiViewportFlags_TopMost);
		bool bPopupWindow = (Viewport->Flags & ImGuiViewportFlags_NoTaskBarIcon);
		// don't activate the window as we would like to keep the focus at MainViewport window
		bool bFocusWindowOnAppearing = ((Viewport->Flags & ImGuiViewportFlags_NoFocusOnAppearing) == 0);
		bool bFocusWidgetOnAppearing = ((Viewport->Flags & ImGuiViewportFlags_NoFocusOnAppearing) == 0);

#if WITH_EDITOR
		// HACK: popups don't support auto focus so a little hack for level editor menu
		if (FCStringAnsi::Stricmp(((ImGuiViewportP*)Viewport)->Window->Name, "Menu##LevelEditor") == 0)
		{
			bFocusWindowOnAppearing = true;
			bFocusWidgetOnAppearing = true;
		}

		// HACK: disable taskbar icon for menu bar, figure out a better way to handle this
		if (FCStringAnsi::Stricmp(((ImGuiViewportP*)Viewport)->Window->Name, "##MainMenuBar") == 0)
		{
			bPopupWindow = true;
		}
#endif

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
			.FocusWhenFirstShown(bFocusWindowOnAppearing)
			.ActivationPolicy(bFocusWindowOnAppearing ? EWindowActivationPolicy::Always : EWindowActivationPolicy::Never)
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
		ViewportData->bFocusRequested = bFocusWidgetOnAppearing;
		ViewportData->OnWindowClosedHandle = ViewportWindow->GetOnWindowClosedEvent().AddLambda(
			[Viewport](TSharedPtr<SWindow> Window)
			{
				// NOTE: Viewport raw ptr accessed here, but should be fine as we remove the delegate if ImGui viewport gets destroyed first
				Viewport->PlatformRequestClose = true;
			});

		Viewport->PlatformRequestResize = false;
		Viewport->PlatformUserData = ViewportData;
	}

	static void UnrealPlatform_DestroyWindow(ImGuiViewport* Viewport)
	{
		if (FImGuiViewportData* ViewportData = (FImGuiViewportData*)Viewport->PlatformUserData)
		{
			if (TSharedPtr<SWindow> ViewportWindow = ViewportData->ViewportWindow.Pin())
			{
				ViewportWindow->GetOnWindowClosedEvent().Remove(ViewportData->OnWindowClosedHandle);
				ViewportWindow->RequestDestroyWindow();
			}
			if (ViewportData->ViewportWidget)
			{
				// shared ptr can live around for a few frames, so make sure references to ImGui data is released
				ViewportData->ViewportWidget->ResetImGuiViewportData();
			}
			ViewportData->ViewportWindow = nullptr;
			ViewportData->ViewportWidget = nullptr;
			ViewportData->ParentWindow = nullptr;
			ViewportData->MainViewportWidget = nullptr;
			ViewportData->OnWindowClosedHandle.Reset();
			IM_DELETE(ViewportData);

			Viewport->PlatformUserData = nullptr;

#if !WITH_EDITOR
			// try to keep UI focus
			ExecuteOnGameThread(TEXT("ImGui_RetainFocus"),
				[]()
				{
					FSlateApplication::Get().ClearKeyboardFocus(EFocusCause::SetDirectly);
					FSlateApplication::Get().ResetToDefaultInputSettings();
				});
#endif
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
			if (ViewportData->ViewportWidget)
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
			if (ViewportData->ViewportWindow.IsValid())
			{
#if WITH_ENGINE
				TSharedPtr<SWindow> Window = ViewportData->ViewportWindow.Pin();
				if (Window)
				{
					Window->Resize(FVector2f{ NewSize.x, NewSize.y });
				}
#else
				// when running without Engine window resizes trigger paint event causing some asserts in D3D buffer locking
				// this has a downside that when resizing parent window viewport windows disappear till resizing is finished
				ExecuteOnGameThread(TEXT("ImGui_ResizeWindow"),
					[WindowWeakPtr=ViewportData->ViewportWindow, NewSize]()
					{
						TSharedPtr<SWindow> Window = WindowWeakPtr.Pin();
						if (Window)
						{
							Window->Resize(FVector2f{ NewSize.x, NewSize.y });
						}
					});
#endif
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
				ViewportWindow->SetTitle(FText::FromString(UTF8_TO_TCHAR(Title)));
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
			ViewportData->bFocusRequested = true;
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
				if (ViewportData->ViewportWidget)
				{
					return ViewportData->ViewportWidget->HasAnyUserFocus().IsSet();
				}

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
			if (ViewportData->ViewportWidget)
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
				else if (ViewportData->bFocusRequested)
				{
					// TODO: this could potentially spam if a lot of windows are created on the same frame.
					ViewportData->bFocusRequested = false;
					FSlateApplication::Get().SetAllUserFocus(ViewportData->ViewportWidget, EFocusCause::SetDirectly);
				}
			}
		}
	}

	static void UnrealPlatform_RenderWindow(ImGuiViewport* Viewport, void*)
	{
		if (FImGuiViewportData* ViewportData = (FImGuiViewportData*)Viewport->PlatformUserData)
		{
			if (Viewport->DrawData && ViewportData->ViewportWidget)
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

#ifdef IMGUI_DISABLE_DEFAULT_FILE_FUNCTIONS
ImFileHandle ImFileOpen(const char* FileName, const char* Mode)
{
	bool bRead = false;
	bool bWrite = false;
	bool bAppend = false;
	bool bExtended = false;

	for (; *Mode; ++Mode)
	{
		if (*Mode == 'r')
		{
			bRead = true;
		}
		else if (*Mode == 'w')
		{
			bWrite = true;
		}
		else if (*Mode == 'a')
		{
			bAppend = true;
		}
		else if (*Mode == '+')
		{
			bExtended = true;
		}
	}

	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();

	if (bWrite || bAppend || bExtended)
	{
		return PlatformFile.OpenWrite(UTF8_TO_TCHAR(FileName), bAppend, bExtended);
	}

	if (bRead)
	{
		return PlatformFile.OpenRead(UTF8_TO_TCHAR(FileName), true);
	}

	return nullptr;
}

bool ImFileClose(ImFileHandle File)
{
	if (File)
	{
		delete File;
		return true;
	}
	return false;
}

uint64 ImFileGetSize(ImFileHandle File)
{
	return File ? File->Size() : MAX_uint64;
}

uint64 ImFileRead(void* Data, uint64 Size, uint64 Count, ImFileHandle File)
{
	if (!File)
	{
		return 0;
	}

	const int64 StartPos = File->Tell();
	File->Read(static_cast<uint8*>(Data), Size * Count);

	const uint64 ReadSize = File->Tell() - StartPos;
	return ReadSize;
}

uint64 ImFileWrite(const void* Data, uint64 Size, uint64 Count, ImFileHandle File)
{
	if (!File)
	{
		return 0;
	}

	const int64 StartPos = File->Tell();
	File->Write(static_cast<const uint8*>(Data), Size * Count);

	const uint64 WriteSize = File->Tell() - StartPos;
	return WriteSize;
}
#endif
