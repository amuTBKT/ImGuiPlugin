// Copyright 2024-26 Amit Kumar Mehar. All Rights Reserved.

#ifdef IMGUI_ALLOW_MENUBAR_EXTENSION

#include "Engine/World.h"
#include "Engine/Engine.h"
#include "SImGuiWidgets.h"
#include "ImGuiSubsystem.h"
#include "UObject/Package.h"
#include "InputKeyEventArgs.h"
#include "Algo/BinarySearch.h"
#include "HAL/IConsoleManager.h"
#include "Misc/ConfigCacheIni.h"
#include "Engine/GameViewportClient.h"
#include "Framework/Commands/InputChord.h"
#include "Framework/Application/SlateUser.h"
#include "Runtime/Launch/Resources/Version.h"
#include "Framework/Application/IInputProcessor.h"
#include "Framework/Application/SlateApplication.h"

#if WITH_EDITOR

// new tools menu api starting from 5.6
#define USE_TOOLMENU_API ((ENGINE_MAJOR_VERSION * 100u + ENGINE_MINOR_VERSION) > 505)
#if USE_TOOLMENU_API
#include "ToolMenus.h"
#include "Widgets/SBoxPanel.h"
#include "Styling/StyleColors.h"
#include "Widgets/Images/SImage.h"
#include "LevelEditorMenuContext.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#else
#include "ViewportToolBarContext.h"
#include "Subsystems/PanelExtensionSubsystem.h"
#endif

#include "Widgets/SNullWidget.h"
#include "Styling/ToolBarStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"

#include "SLevelViewport.h"
#include "Widgets/Layout/SConstraintCanvas.h"

#include "EditorStyleSet.h"
#include "WorkspaceMenuStructure.h"
#include "Widgets/Docking/SDockTab.h"
#include "Framework/Docking/TabManager.h"
#include "WorkspaceMenuStructureModule.h"

#endif //#if WITH_EDITOR

static TAutoConsoleVariable<bool> CVarAddImGuiWidgetToLevelViewport(
	TEXT("imgui.UseLevelViewport"),
	false,
	TEXT("Prefer LevelViewport over Tabbed window for hosting ImGui editor widget.\n")
	TEXT("This only applies for the Editor context, doesn't affect PIE or Game context."),
	ECVF_ReadOnly);

static TAutoConsoleVariable<bool> CVarAutoHideMainMenuBar(
	TEXT("imgui.AutoHideMenuBar"),
	true,
	TEXT("Auto hide ImGui main menu bar"));

///////////////////////////////////////////////////////////////////////////////////////////////////////

#define LOCTEXT_NAMESPACE "ImGuiPlugin"

namespace ImGuiFocusHandler
{
	static TWeakPtr<SWidget> LastFocusedWidget;
	static FDelegateHandle FocusChangedEventHandle;

	void OnFocusChanged(const FFocusEvent& Event, const FWeakWidgetPath& OldWidgetPath, const TSharedPtr<SWidget>& OldWidget, const FWidgetPath& NewWidgetPath, const TSharedPtr<SWidget>& NewWidget)
	{
		if (Event.GetUser() == 0 && NewWidget && NewWidget->GetType() != TEXT("SImGuiMainMenuWidget"))
		{
			LastFocusedWidget = NewWidget;
		}
	}
	void RegisterFocusChangedEvents()
	{
		if (FSlateApplication::IsInitialized())
		{
			FocusChangedEventHandle = FSlateApplication::Get().OnFocusChanging().AddStatic(OnFocusChanged);
		}
	}
	void UnregisterFocusChangedEvents()
	{
		if (FSlateApplication::IsInitialized())
		{
			FSlateApplication::Get().OnFocusChanging().Remove(FocusChangedEventHandle);
			FocusChangedEventHandle.Reset();
		}
	}

	// give focus back game
	void ResetFocus()
	{
		if (FSlateApplication::IsInitialized())
		{
			TSharedPtr<SWidget> WidgetToFocus = LastFocusedWidget.Pin();
			if (WidgetToFocus)
			{
				FSlateApplication::Get().SetAllUserFocus(WidgetToFocus, EFocusCause::SetDirectly);
			}
			else
			{
				FSlateApplication::Get().SetAllUserFocusToGameViewport(EFocusCause::SetDirectly);
			}
			LastFocusedWidget.Reset();
		}
	}

	// same logic as Shift+F1
	void SetUIFocus()
	{
		if (FSlateApplication::IsInitialized())
		{
			TSharedPtr<FSlateUser> SlateUser = FSlateApplication::Get().GetUser(0);
			if (SlateUser)
			{
				LastFocusedWidget = SlateUser->GetFocusedWidget();
			}

			ExecuteOnGameThread(TEXT("ImGui_RetainFocus"),
				[]()
				{
					FSlateApplication::Get().ClearKeyboardFocus(EFocusCause::SetDirectly);
					FSlateApplication::Get().ResetToDefaultInputSettings();
				});
		}
	}
}

namespace ImGuiUtils
{
	struct FMenuPathIterator
	{
		FMenuPathIterator(FAnsiStringView InPath)
			: Path(InPath)
			, ItrOffset(0)
		{}

		FAnsiStringView operator++()
		{
			int32 DelimiterIndex;
			if (Path.Mid(ItrOffset).FindChar('.', DelimiterIndex))
			{
				ItrOffset += DelimiterIndex + 1;
				return Path.Mid(0, ItrOffset - 1);
			}
			return {};
		}

		FName GetNextGroup()
		{
			FName GroupName = NAME_None;

			int32 DelimiterIndex;
			if (Path.Mid(ItrOffset).FindChar('.', DelimiterIndex))
			{
				GroupName = FName(Path.Mid(ItrOffset, DelimiterIndex));
				ItrOffset += DelimiterIndex + 1;
			}
			return GroupName;
		}

		operator bool() const
		{
			return ItrOffset < Path.Len();
		}

		FAnsiStringView Path;
		int32 ItrOffset;
	};

	struct FImGuiMenuContainer
	{
		struct FWidgetSlot
		{
			explicit FWidgetSlot(FAnsiString InPath, EImGuiMainMenuWidgetFlags InWidgetFlags = EImGuiMainMenuWidgetFlags::None)
				: Path(MoveTemp(InPath))
				, Storage(TInPlaceType<TArray<FWidgetSlot>>(), TArray<FWidgetSlot>{})
				, WidgetFlags(InWidgetFlags)
			{
				if (Path.FindLastChar('.', SlotNameOffset))
				{
					SlotNameOffset += 1;
				}
				else
				{
					SlotNameOffset = 0;
				}
			}

			explicit FWidgetSlot(FAnsiString InPath, FAnsiString InToolTip, const FSlateBrush* InIcon, FOnTickImGuiWidgetDelegate InTickDelegate, EImGuiMainMenuWidgetFlags InWidgetFlags)
				: Path(MoveTemp(InPath))
				, ToolTip(MoveTemp(InToolTip))
				, Icon(InIcon)
				, Storage(TInPlaceType<FOnTickImGuiWidgetDelegate>(), MoveTemp(InTickDelegate))
				, WidgetFlags(InWidgetFlags)
			{
				if (Path.FindLastChar('.', SlotNameOffset))
				{
					SlotNameOffset += 1;
				}
				else
				{
					SlotNameOffset = 0;
				}
			}

			const char*							GetName()			const { return *Path + SlotNameOffset; };
			bool								IsMenuItem()		const { return Storage.IsType<FOnTickImGuiWidgetDelegate>(); }
			bool								IsRightAligned()	const { return EnumHasAnyFlags(WidgetFlags, EImGuiMainMenuWidgetFlags::RightAligned); }
			const FOnTickImGuiWidgetDelegate&	GetTickDelegate()	const { check(IsMenuItem()); return Storage.Get<FOnTickImGuiWidgetDelegate>(); }
			TArray<FWidgetSlot>&				GetChildren()			  { check(!IsMenuItem()); return Storage.Get<TArray<FWidgetSlot>>(); }
			const TArray<FWidgetSlot>&			GetChildren()		const { check(!IsMenuItem()); return Storage.Get<TArray<FWidgetSlot>>(); }

			bool operator==(const FAnsiStringView& Other)			const { return Other.Equals(*Path, ESearchCase::IgnoreCase); }
			bool operator==(const FWidgetSlot& Other)				const { return FCStringAnsi::Stricmp(*Path, *Other.Path) == 0; }
			bool operator<(const FWidgetSlot& Other)				const { return FCStringAnsi::Stricmp(*Path, *Other.Path) < 0; }

			FAnsiString Path;
			FAnsiString ToolTip;
			const FSlateBrush* Icon = nullptr;
			TVariant<FOnTickImGuiWidgetDelegate, TArray<FWidgetSlot>> Storage;
			int32 SlotNameOffset = 0;
			// is the menu item active and drawing the widget window
			bool bIsActive = false;
			EImGuiMainMenuWidgetFlags WidgetFlags = EImGuiMainMenuWidgetFlags::None;
		};

		struct FQueuedWidgetSlot
		{
			FAnsiString					WidgetPath;
			FAnsiString					WidgetToolTip;
			const FSlateBrush*			WidgetIcon = nullptr;
			FOnTickImGuiWidgetDelegate	TickDelegate;
			EImGuiMainMenuWidgetFlags	WidgetFlags = EImGuiMainMenuWidgetFlags::None;

			bool operator==(const FAnsiStringView& Other) const
			{
				return Other.Equals(*WidgetPath, ESearchCase::IgnoreCase);
			}
			bool operator==(const FQueuedWidgetSlot& Other) const
			{
				return FCStringAnsi::Stricmp(*WidgetPath, *Other.WidgetPath) == 0;
			}
		};

		enum class EFindSlotResult
		{
			Found,			// menu item available/created on demand
			NotFound,		// menu item not found
			ConflictingID	// menu item with same ID already registered
		};

		static FWidgetSlot* AddSlotSorted(TArray<FWidgetSlot>& Container, FWidgetSlot&& Slot)
		{
			const int32 InsertIndex = Algo::LowerBound(Container, Slot);
			Container.Insert(MoveTemp(Slot), InsertIndex);
			return &Container[InsertIndex];
		}

		TPair<EFindSlotResult, FWidgetSlot*> FindOrCreateSlot_Internal(FAnsiStringView Path, bool bCreateHierarchy, TOptional<EImGuiMainMenuWidgetFlags> Flags)
		{
			FMenuPathIterator PathItr{ Path };

			FAnsiStringView SubPath = ++PathItr;
			if (SubPath.IsEmpty())
			{
				return { EFindSlotResult::NotFound, nullptr };
			}

			FWidgetSlot* ParentSlot = WidgetSlots.FindByKey(SubPath);
			if (!ParentSlot)
			{
				if (bCreateHierarchy)
				{
					ParentSlot = AddSlotSorted(WidgetSlots, FWidgetSlot(FAnsiString(SubPath), Flags.Get(EImGuiMainMenuWidgetFlags::None)));
				}
				else
				{
					return { EFindSlotResult::NotFound, nullptr };
				}
			}
			else if (ParentSlot->IsMenuItem())
			{
				ensureAlwaysMsgf(false, TEXT("Path(%hs) points to an active menu item (%hs)"), Path.GetData(), *ParentSlot->Path);
				return { EFindSlotResult::ConflictingID, nullptr };
			}
			else if (Flags.IsSet() && ParentSlot->IsRightAligned() != (EnumHasAnyFlags(Flags.GetValue(), EImGuiMainMenuWidgetFlags::RightAligned)))
			{
				ensureAlwaysMsgf(false, TEXT("ParentSlot(%hs) was previously added using a different alignment, widget(%hs) will render incorrectly"), *ParentSlot->Path, Path.GetData());
			}

			while (PathItr)
			{
				SubPath = ++PathItr;
				if (SubPath.IsEmpty())
				{
					break;
				}

				FWidgetSlot* NextSlot = ParentSlot->GetChildren().FindByKey(SubPath);
				if (!NextSlot)
				{
					if (bCreateHierarchy)
					{
						NextSlot = AddSlotSorted(ParentSlot->GetChildren(), FWidgetSlot(FAnsiString(SubPath)));
					}
					else
					{
						break;
					}
				}
				else if (NextSlot->IsMenuItem())
				{
					ensureAlwaysMsgf(false, TEXT("Path(%hs) points to an active menu item (%hs)"), *FAnsiString(SubPath), *NextSlot->Path);
					return { EFindSlotResult::ConflictingID, nullptr };
				}
				ParentSlot = NextSlot;
			}

			return { EFindSlotResult::Found, ParentSlot };
		}
		FWidgetSlot* FindSlot(FAnsiStringView Path)
		{
			return FindOrCreateSlot_Internal(Path, /*bCreateHierarchy=*/false, TOptional<EImGuiMainMenuWidgetFlags>{}).Value;
		}
		TPair<EFindSlotResult, FWidgetSlot*> FindOrCreateSlot(FAnsiStringView Path, EImGuiMainMenuWidgetFlags Flags)
		{
			return FindOrCreateSlot_Internal(Path, /*bCreateHierarchy=*/true, Flags);
		}

		void ProcessQueuedMainWidgetSlots()
		{
			check(!QueueWidgetSlotChanges);

			for (const auto& Item : WidgetSlotsToAdd)
			{
				RegisterWidget(*Item.WidgetPath, *Item.WidgetToolTip, Item.WidgetIcon, Item.TickDelegate, Item.WidgetFlags);
			}
			WidgetSlotsToAdd.Reset();

			for (const auto& Item : WidgetSlotsToRemove)
			{
				UnregisterWidget(*Item.WidgetPath);
			}
			WidgetSlotsToRemove.Reset();
		}

		void RegisterWidget(
			const char* WidgetPath, const char* WidgetToolTip, const FSlateBrush* WidgetIcon,
			FOnTickImGuiWidgetDelegate TickDelegate, EImGuiMainMenuWidgetFlags WidgetFlags)
		{
			if (!ensureAlways(WidgetPath && FCStringAnsi::Strlen(WidgetPath) > 0))
			{
				return;
			}

			if (QueueWidgetSlotChanges)
			{
				WidgetSlotsToAdd.Emplace(WidgetPath, WidgetToolTip, WidgetIcon, TickDelegate, WidgetFlags);
				WidgetSlotsToRemove.Remove(WidgetSlotsToAdd.Last());
				return;
			}

			auto FoundSlot = FindOrCreateSlot(WidgetPath, WidgetFlags);
			if (FWidgetSlot* ParentSlot = FoundSlot.Value)
			{
				if (ParentSlot->GetChildren().Contains(WidgetPath))
				{
					ensureAlwaysMsgf(false, TEXT("Widget slot (Path=%hs) already registered"), WidgetPath);
				}
				else
				{
					FWidgetSlot NewSlot = FWidgetSlot{ FAnsiString(WidgetPath), FAnsiString(WidgetToolTip), WidgetIcon, TickDelegate, WidgetFlags };
					LoadSlotState(NewSlot);

					AddSlotSorted(ParentSlot->GetChildren(), MoveTemp(NewSlot));
				}
			}
			else if ((FoundSlot.Key == EFindSlotResult::NotFound) && ensureAlways(EnumHasAnyFlags(WidgetFlags, EImGuiMainMenuWidgetFlags::TickInMenuBar)))
			{
				// widget will create its own menu
				if (WidgetSlots.Contains(WidgetPath))
				{
					ensureAlwaysMsgf(false, TEXT("Widget slot (Path=%hs) already registered"), WidgetPath);
				}
				else
				{
					AddSlotSorted(WidgetSlots, FWidgetSlot{ FAnsiString(WidgetPath), FAnsiString(WidgetToolTip), WidgetIcon, TickDelegate, WidgetFlags });
				}
			}
		}
		void UnregisterWidget(const char* WidgetPath)
		{
			if (!ensureAlways(WidgetPath && FCStringAnsi::Strlen(WidgetPath) > 0))
			{
				return;
			}

			if (QueueWidgetSlotChanges)
			{
				WidgetSlotsToRemove.Emplace(WidgetPath);
				WidgetSlotsToAdd.Remove(WidgetSlotsToRemove.Last());
				return;
			}

			FWidgetSlot* ParentSlot = FindSlot(WidgetPath);
			if (ParentSlot)
			{
				ParentSlot->GetChildren().RemoveAll([&](const auto& Entry) { return Entry == WidgetPath; });
			}
			else
			{
				WidgetSlots.RemoveAll([&](const auto& Entry) { return Entry == WidgetPath; });
			}
		}

		void LoadSlotState(FWidgetSlot& Slot)
		{
			if (FConfigFile* WidgetSettings = UImGuiSubsystem::Get()->GetSaveDataConfigFile())
			{
				WidgetSettings->GetBool(*SaveDataSectionName, UTF8_TO_TCHAR(*Slot.Path), Slot.bIsActive);
			}
		}
		void SaveSlotState(const FWidgetSlot& Slot)
		{
			if (FConfigFile* WidgetSettings = UImGuiSubsystem::Get()->GetSaveDataConfigFile())
			{
				WidgetSettings->SetBool(*SaveDataSectionName, UTF8_TO_TCHAR(*Slot.Path), Slot.bIsActive);
				UImGuiSubsystem::Get()->SaveConfigToDisk();
			}
		}

		TArray<FWidgetSlot>& StartIterationOverMainMenuWidgetSlots() { QueueWidgetSlotChanges = true; return WidgetSlots; }
		void				 EndIterationOverMainMenuWidgetSlots() { QueueWidgetSlotChanges = false; ProcessQueuedMainWidgetSlots(); }

#if WITH_EDITOR
		void SetWorld(const UWorld* World)
		{
			OwningWorld = World;
			if (World)
			{
				if (World->GetNetMode() == NM_DedicatedServer)
				{
					SaveDataSectionName = "MainMenuBar_Server";
				}
				else
				{
					int32 PIEInstanceId = World->GetOutermost()->GetPIEInstanceID();
					if (PIEInstanceId != INDEX_NONE)
					{
						SaveDataSectionName = FString::Printf(TEXT("MainMenuBar_%i"), PIEInstanceId);
					}
				}
			}
		}
		const UWorld* GetWorld() const { return OwningWorld.Get(); }
		TWeakObjectPtr<const UWorld> OwningWorld;
#endif

#if WITH_EDITOR
		FString						 SaveDataSectionName = TEXT("MainMenuBar_Editor");
#elif UE_SERVER
		FString						 SaveDataSectionName = TEXT("MainMenuBar_Server");
#else
		FString						 SaveDataSectionName = TEXT("MainMenuBar_Game");
#endif
		TArray<FQueuedWidgetSlot>	 WidgetSlotsToAdd;
		TArray<FQueuedWidgetSlot>	 WidgetSlotsToRemove;
		TArray<FWidgetSlot>			 WidgetSlots;
		bool						 QueueWidgetSlotChanges = false;
	};

	FImGuiMenuContainer& GetMenuContainerForWorld(const UWorld* World);

	/* main menu widget, only one instance per world */
	class SImGuiMainMenuWidget : public SImGuiWidgetBase
	{
		using Super = SImGuiWidgetBase;
	public:
		SLATE_BEGIN_ARGS(SImGuiMainMenuWidget)
			: _MainViewportWindow(nullptr)
			, _OwningWorld(nullptr)
			, _AutoHideMenuBar(true)
			{}
			SLATE_ARGUMENT(TSharedPtr<SWindow>, MainViewportWindow);
			SLATE_ARGUMENT(const UWorld*, OwningWorld);
			SLATE_ARGUMENT(bool, AutoHideMenuBar);
		SLATE_END_ARGS()

		void Construct(const FArguments& InArgs)
		{
			const UWorld* World = InArgs._OwningWorld;

			FAnsiString ConfigFileName = "ImGui";
			if (World)
			{
				if (World->GetNetMode() == NM_DedicatedServer)
				{
					ConfigFileName = "ImGui_Server";
				}
				else
				{
					int32 PIEInstanceId = World->GetOutermost()->GetPIEInstanceID();
					if (PIEInstanceId != INDEX_NONE)
					{
						ConfigFileName = FAnsiString::Printf("ImGui_%i", PIEInstanceId);
					}
				}
			}
			else if (IsRunningDedicatedServer())
			{
				ConfigFileName = "ImGui_Server";
			}

			Super::Construct(
				Super::FArguments()
				.MainViewportWindow(InArgs._MainViewportWindow)
				.ConfigFileName(*ConfigFileName));

			m_OwningWorld = World;
			m_AutoHideMenuBar = InArgs._AutoHideMenuBar;
			UImGuiSubsystem::OnBeginImGuiFrame.AddRaw(this, &SImGuiMainMenuWidget::BeginFrame);
			UImGuiSubsystem::OnEndImGuiFrame.AddRaw(this, &SImGuiMainMenuWidget::EndFrame);
		}

		~SImGuiMainMenuWidget()
		{
			UImGuiSubsystem::OnBeginImGuiFrame.RemoveAll(this);
			UImGuiSubsystem::OnEndImGuiFrame.RemoveAll(this);
		}

		virtual FVector2D ComputeDesiredSize(float) const override
		{
#if WITH_EDITOR
			// very small non zero size to disable windows from getting merged into the main viewport (inputs are disabled on level editor widget!)
			return m_bIsAddedToLevelViewport ? FVector2D(8.f, 8.f) : FVector2D::ZeroVector;
#else
			return FVector2D::ZeroVector;
#endif
		}

		const UWorld* GetWorld() const { return m_OwningWorld.Get(); }

#if WITH_EDITOR
		bool IsMenuOpen() const { return m_bIsMenuOpen; }
		void OpenMenu() { m_bOpenMenu = true; m_MenuRequestedFrameIndex = GetImGuiContext()->FrameCount; }
		void SetMenuOffset(ImVec2 Offset) { m_PendingMenuOffset = Offset; }
		TSharedPtr<SLevelViewport> GetLevelViewport() const { return m_LevelViewport.Pin(); }
		TSharedPtr<SWidget> GetLevelViewportOverlayWidget() const { return m_LevelViewportOverlayWidget.Pin(); }
		void SetLevelViewport(TSharedPtr<SLevelViewport> InLevelViewport, TSharedPtr<SWidget> InOverlayWidget)
		{
			m_bIsAddedToLevelViewport = InOverlayWidget.IsValid();
			m_LevelViewportOverlayWidget = InOverlayWidget;
			m_LevelViewport = InLevelViewport;
		}
#endif

		void HideWidget()
		{
			m_PendingVisibilityState = EVisibility::Hidden;
		}
		void ShowWidget()
		{
			bFocusRequested = true;
			m_PendingVisibilityState = EVisibility::Visible;
		}

	private:
		void BeginFrame()
		{
			if (GetVisibility().IsVisible())
			{
				FImGuiTickScope TickScope{ GetTickContext() };

				BeginImGuiFrame(GetCachedGeometry());

				SetupDockNode();

				if (bFocusRequested && FSlateApplication::IsInitialized())
				{
					bFocusRequested = false;
					FSlateApplication::Get().SetAllUserFocus(AsShared(), EFocusCause::SetDirectly);
				}
			}

			// update visiibility after BeginFrame to ensure viewport windows get destroyed
			// setting this before would mean nothing gets ticked and viewport windows will stay visible in unresponsive state
			if (m_PendingVisibilityState.IsSet())
			{
				EVisibility NewVisibility = m_PendingVisibilityState.GetValue();
				if (NewVisibility != EVisibility::Hidden)
				{
					m_MenuBarAlpha = MenuBarVisibilityDuration;
				}

				SetVisibility(NewVisibility);
				m_PendingVisibilityState.Reset();
			}
		}

		void EndFrame()
		{
			FImGuiTickScope TickScope{ GetTickContext() };

			EndImGuiFrame();
			m_bIsDockNodeValid = false;

			// TODO: maybe move this to SImGuiWidgetBase? not sure this seems a bit hacky and not needed in general atm.
			EVisibility CurrentVisibility = GetVisibility();
			if (GetTickContext()->bIsDrawingRemotely)
			{
				if (CurrentVisibility != EVisibility::HitTestInvisible)
				{
					SetVisibility(EVisibility::HitTestInvisible);
				}
				if (HasAnyUserFocus())
				{
					ImGuiFocusHandler::ResetFocus();
				}
			}
			else if (CurrentVisibility != EVisibility::Hidden && FSlateApplication::IsInitialized())
			{
				// give a few frames before disabling inputs (otherwise it just keeps flipping b/w the two states)
				HitTestInvisibilityCounter += GetImGuiContext()->IO.WantCaptureMouse ? 4 : -1;
				HitTestInvisibilityCounter = FMath::Clamp(HitTestInvisibilityCounter, -4, 4);
				if (HitTestInvisibilityCounter == 4)
				{
					SetVisibility(EVisibility::Visible);
				}
				else if (HitTestInvisibilityCounter == -4)
				{
					SetVisibility(EVisibility::HitTestInvisible);
				}

				// we don't receive mouse position events when set to HitTestInvisible
				if (CurrentVisibility == EVisibility::HitTestInvisible)
				{
					FVector2f MousePosition = FSlateApplication::Get().GetCursorPos();
					if ((GetImGuiContext()->IO.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) == 0)
					{
						MousePosition = GetCachedGeometry().AbsoluteToLocal(MousePosition);
					}
					if (!LastMousePosition.Equals(MousePosition, 1.f))
					{
						LastMousePosition = MousePosition;
						GetImGuiContext()->IO.AddMousePosEvent(MousePosition.X, MousePosition.Y);
					}
				}
			}
		}

		void SetupDockNode()
		{
			if (!m_bIsDockNodeValid)
			{
				m_bIsDockNodeValid = true;

#if WITH_EDITOR
				if (m_bIsAddedToLevelViewport)
				{
					// can't support docking :(
				}
				else
#endif
				{
					const bool bIsEditorContext = GIsEditor && !GetWorld();
					MainViewportDockSpaceId = ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport(), bIsEditorContext ? ImGuiDockNodeFlags_None : ImGuiDockNodeFlags_PassthruCentralNode);
				}
			}
		}

	private:
		TWeakObjectPtr<const UWorld> m_OwningWorld;

		// when added to level viewport
#if WITH_EDITOR
		bool m_bOpenMenu = true;
		bool m_bIsMenuOpen = false;
		int32 m_MenuRequestedFrameIndex = 0;
		bool m_bIsAddedToLevelViewport = false;
		ImVec2 m_MenuOffset = ImVec2(0.f, 0.f);
		TOptional<ImVec2> m_PendingMenuOffset;
		TWeakPtr<SLevelViewport> m_LevelViewport;
		TWeakPtr<SWidget> m_LevelViewportOverlayWidget;
#endif
		// TODO: hacky auto hiding menu bar (probably should use a curve or somethig? needs cleaning up at some point)
		static constexpr float MenuBarVisibilityDuration = 4.f;
		bool m_AutoHideMenuBar = true;
		float m_MenuBarAlpha = MenuBarVisibilityDuration;

		ImGuiID MainViewportDockSpaceId = 0;
		int32 HitTestInvisibilityCounter = 0;
		FVector2f LastMousePosition = FVector2f::ZeroVector;
		bool bFocusRequested = true;
		TOptional<EVisibility> m_PendingVisibilityState;

		// cached during tick for easier access
		bool m_bIsDockNodeValid = false;
		UImGuiSubsystem* m_ImGuiSubsystem = nullptr;
		FImGuiImageBindingParams m_ExpandedMenuIcon{};
		FImGuiImageBindingParams m_CollapsedMenuIcon{};

		void TickMainMenuBar(FImGuiMenuContainer& MenuContainer, FImGuiMenuContainer::FWidgetSlot& Slot, FImGuiTickContext* TickContext)
		{
			if (Slot.IsMenuItem())
			{
				const bool bWasSlotActive = Slot.bIsActive;

				if (EnumHasAnyFlags(Slot.WidgetFlags, EImGuiMainMenuWidgetFlags::TickInMenuBar))
				{
					TickContext->MainMenuBar_CurrentItemEnabledState = &Slot.bIsActive;
					Slot.GetTickDelegate().ExecuteIfBound(TickContext);
					TickContext->MainMenuBar_CurrentItemEnabledState = nullptr;
				}
				else
				{
					FImGuiImageBindingParams Icon = Slot.Icon ? m_ImGuiSubsystem->RegisterOneFrameResource(Slot.Icon, ImGui::GetTextLineHeight()) : FImGuiImageBindingParams{};
					if (FImGui::MenuItem(Slot.GetName(), Slot.bIsActive, Icon))
					{
						Slot.bIsActive = !Slot.bIsActive;
					}
					if (Slot.ToolTip.Len() > 0)
					{
						ImGui::SetItemTooltip("%s", *Slot.ToolTip);
					}
				}

				if (bWasSlotActive != Slot.bIsActive)
				{
					MenuContainer.SaveSlotState(Slot);
				}
			}
			else if (!Slot.GetChildren().IsEmpty())
			{
				FImGui::SubMenu(Slot.GetName(),
					[&]()
					{
						for (auto& ChildSlot : Slot.GetChildren())
						{
							TickMainMenuBar(MenuContainer, ChildSlot, TickContext);
						}
					}, m_ExpandedMenuIcon, m_CollapsedMenuIcon);
			}
		}
		void TickMainMenuWidgets(FImGuiMenuContainer& MenuContainer, FImGuiMenuContainer::FWidgetSlot& Slot, FImGuiTickContext* TickContext)
		{
			if (Slot.IsMenuItem())
			{
				if (Slot.bIsActive)
				{
					if (EnumHasAnyFlags(Slot.WidgetFlags, EImGuiMainMenuWidgetFlags::SkipWindowCreation))
					{
						Slot.GetTickDelegate().ExecuteIfBound(TickContext);
					}
					else
					{
						const bool bWasActive = Slot.bIsActive;
						ImGui::SetNextWindowSize(ImVec2(512.f, 512.f), ImGuiCond_FirstUseEver);
						if (ImGui::Begin(Slot.GetName(), &Slot.bIsActive))
						{
							Slot.GetTickDelegate().ExecuteIfBound(TickContext);
						}
						ImGui::End();

						if (bWasActive != Slot.bIsActive)
						{
							MenuContainer.SaveSlotState(Slot);
						}
					}
				}
			}
			else
			{
				for (auto& Child : Slot.GetChildren())
				{
					TickMainMenuWidgets(MenuContainer, Child, TickContext);
				}
			}
		}

		static bool HasAnyDockedWindow(ImGuiDockNode* Node)
		{
			if (!Node) return false;
			if (Node->Windows.Size > 0) return true;
			return HasAnyDockedWindow(Node->ChildNodes[0]) || HasAnyDockedWindow(Node->ChildNodes[1]);
		}
		virtual void TickImGuiInternal(FImGuiTickContext* TickContext) override
		{
			m_ImGuiSubsystem = UImGuiSubsystem::Get();
			FImGuiMenuContainer& MenuContainer = GetMenuContainerForWorld(GetWorld());
			TArray<FImGuiMenuContainer::FWidgetSlot>& Slots = MenuContainer.StartIterationOverMainMenuWidgetSlots();
			ON_SCOPE_EXIT
			{
				MenuContainer.EndIterationOverMainMenuWidgetSlots();
				m_ImGuiSubsystem = nullptr;
			};

			FImGuiTickScope TickScope{ TickContext };

			if (Slots.IsEmpty())
			{
				const ImGuiWindowFlags WindowFlags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoMove;
				const ImVec2 WindowPos = ImGui::GetMainViewport()->Pos + ImGui::GetStyle().WindowPadding * ImVec2(1.f, -1.f) + ImVec2(0.f, ImGui::GetMainViewport()->Size.y);
				ImGui::SetNextWindowPos(WindowPos, ImGuiCond_Always, ImVec2(0.f, 1.f));
				ImGui::SetNextWindowBgAlpha(0.5f);
				ImGui::Begin("EmptyWindow", nullptr, WindowFlags);
				ImGui::TextUnformatted("No ImGui menu items registered!");
				ImGui::End();
				return;
			}

			m_ExpandedMenuIcon = m_ImGuiSubsystem->RegisterOneFrameResource(IMGUI_STYLE_ICON_BRUSH("CoreStyle", "Icons.FolderOpen"), ImGui::GetTextLineHeight());
			m_CollapsedMenuIcon = m_ImGuiSubsystem->RegisterOneFrameResource(IMGUI_STYLE_ICON_BRUSH("CoreStyle", "Icons.FolderClosed"), ImGui::GetTextLineHeight());

			SetupDockNode();

			auto RunMainMenuTickLogic = [&](bool bDrawRightAlignedItems)
				{
					TickContext->MainMenuBar_bIsTicking = true;
					TickContext->MainMenuBar_Height = ImGui::GetFrameHeight();

					for (FImGuiMenuContainer::FWidgetSlot& Slot : Slots)
					{
						if (Slot.IsRightAligned())
							continue;

						if (Slot.IsMenuItem())
						{
							TickContext->MainMenuBar_CurrentItemEnabledState = &Slot.bIsActive;
							Slot.GetTickDelegate().ExecuteIfBound(TickContext);
							TickContext->MainMenuBar_CurrentItemEnabledState = nullptr;
						}
						else if (!Slot.GetChildren().IsEmpty() && ImGui::BeginMenu(Slot.GetName()))
						{
							for (auto& ChildSlot : Slot.GetChildren())
							{
								TickMainMenuBar(MenuContainer, ChildSlot, TickContext);
							}
							ImGui::EndMenu();
						}
					}

					// draw right aligned menu items
					if (bDrawRightAlignedItems)
					{
						TickContext->MainMenuBar_RightDirOffsetX = ImGui::GetCursorPosX();
						TickContext->MainMenuBar_RightDirOffsetY = ImGui::GetCursorPosY();
						TickContext->MainMenuBar_RightDirCursorPosX = ImGui::GetMainViewport()->WorkSize.x - ImGui::GetCurrentWindow()->DC.MenuBarOffset.x;

						if (TickContext->AllocateSpaceForRightAlignedMenuItem("Search"))
						{
							ImGui::MenuItem("Search");
						}

						for (FImGuiMenuContainer::FWidgetSlot& Slot : Slots)
						{
							if (!Slot.IsRightAligned())
								continue;

							if (Slot.IsMenuItem())
							{
								Slot.GetTickDelegate().ExecuteIfBound(TickContext);
							}
							else if (!Slot.GetChildren().IsEmpty())
							{
								if (!TickContext->AllocateSpaceForRightAlignedMenuItem(Slot.GetName()))
								{
									// won't be able to add any more items
									break;
								}

								if (ImGui::BeginMenu(Slot.GetName()))
								{
									for (auto& ChildSlot : Slot.GetChildren())
									{
										TickMainMenuBar(MenuContainer, ChildSlot, TickContext);
									}
									ImGui::EndMenu();
								}
							}
						}
					}

					TickContext->MainMenuBar_bIsTicking = false;
				};

#if WITH_EDITOR
			if (m_bIsAddedToLevelViewport)
			{
				TSharedPtr<SLevelViewport> LevelViewport = GetLevelViewport();
#if USE_TOOLMENU_API
				const bool bIsViewportToolbarHidden = LevelViewport && !LevelViewport->IsViewportToolbarVisible();
#else
				const bool bIsViewportToolbarHidden = GLevelEditorModeTools().IsViewportUIHidden();
#endif

				// NOTE: 2 frames delay to make sure the widget has ticked atleast once to adjust viewport offset
				// plus to make sure windows opening on the same frame don't dismiss the menu bar popup.
				if (ImGui::GetFrameCount() > (m_MenuRequestedFrameIndex + 2))
				{
					// HACK: this should match the value in `UnrealPlatform_CreateWindow` for auto focus to work properly.
					// the logic here manually calls ImGui::Begin to make sure the popup window name matches this value.
					static const char* LevelEditorPopupMenuName = "Menu##LevelEditor";

					if (m_bOpenMenu)
					{
						ImGui::OpenPopup(LevelEditorPopupMenuName, ImGuiPopupFlags_NoReopen);
					}
					if (m_PendingMenuOffset)
					{
						m_MenuOffset = m_PendingMenuOffset.GetValue() - ImGui::GetMainViewport()->Pos + ImVec2(0.f, 2.f);
						m_PendingMenuOffset.Reset();
					}

					m_bIsMenuOpen = ImGui::IsPopupOpen(LevelEditorPopupMenuName, ImGuiPopupFlags_None);
					if (m_bIsMenuOpen)
					{
						ImGui::SetNextWindowPos(ImGui::GetMainViewport()->Pos + m_MenuOffset, ImGuiCond_Always);
						if (ImGui::Begin(LevelEditorPopupMenuName, nullptr, ImGuiWindowFlags_Popup | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove))
						{
							if (m_bOpenMenu && ImGui::IsWindowAppearing())
							{
								m_bOpenMenu = false;
							}

							RunMainMenuTickLogic(false);

							if (bIsViewportToolbarHidden)
							{
								ImGui::CloseCurrentPopup();
							}
						}
						ImGui::EndPopup();
					}
				}
			}
			else
#endif
			{
				bool bKeepMenuBarVisible = (!FSlateApplication::IsInitialized() || TickContext->bIsDrawingRemotely) || !m_AutoHideMenuBar || (CVarAutoHideMainMenuBar.GetValueOnGameThread() == false);
				if (ImGuiDockNode* DockNode = bKeepMenuBarVisible ? nullptr : ImGui::DockBuilderGetNode(MainViewportDockSpaceId))
				{
					if (HasAnyDockedWindow(DockNode))
					{
						bKeepMenuBarVisible = true;
					}
				}
				if (!bKeepMenuBarVisible)
				{
					ImGuiViewport* MainViewport = ImGui::GetMainViewport();
					ImVec2 MenuBarMin = MainViewport->Pos;
					ImVec2 MenuBarMax = MenuBarMin + ImVec2(MainViewport->Size.x, ImGui::GetFrameHeight() * 0.5f);

					if (ImGui::IsMouseHoveringRect(MenuBarMin, MenuBarMax, /*clip=*/false))
					{
						bKeepMenuBarVisible = true;
					}
					// make the menu visible when using Ctrl+Tab
					if (GetImGuiContext()->NavWindowingTarget && FCStringAnsi::Strcmp(GetImGuiContext()->NavWindowingTarget->Name, "##MainMenuBar") == 0)
					{
						bKeepMenuBarVisible = true;
					}
				}

				ImGui::PushStyleVar(ImGuiStyleVar_Alpha, FMath::Min(1.f, m_MenuBarAlpha));
				if (ImGui::BeginMainMenuBar())
				{
					RunMainMenuTickLogic(true);

					auto IsAnyMenuItemActive = [&]()
						{
							bool bActive = false;
							ImGuiWindow* CurrentWindow = ImGui::GetCurrentWindow();
							ImGuiWindow* HoveredWindow = GetImGuiContext()->HoveredWindow;
							ImGuiWindow* NavWindow = GetImGuiContext()->NavWindow;
							ImGuiWindow* ActiveIdWindow = GetImGuiContext()->ActiveIdWindow;

							// NOTE: this function will mostly likely be called when hovering the menus so check hovered windows first
							ImGuiWindow* Window = HoveredWindow;
							while (Window)
							{
								if (Window == CurrentWindow)
								{
									bActive = true;
									break;
								}
								Window = Window->ParentWindow;
							}

							// check for nav window next (needed when using Ctrl+Tab)
							if (!bActive && (HoveredWindow != NavWindow) && HasAnyUserFocus())
							{
								Window = NavWindow;
								while (Window)
								{
									if (Window == CurrentWindow)
									{
										bActive = true;
										break;
									}
									Window = Window->ParentWindow;
								}
							}

							// check for active window next (needed to handle widgets like InputText)
							if (!bActive)
							{
								Window = ActiveIdWindow;
								while (Window)
								{
									if (Window == CurrentWindow)
									{
										bActive = true;
										break;
									}
									Window = Window->ParentWindow;
								}
							}
							return bActive;
						};

					if (!bKeepMenuBarVisible && IsAnyMenuItemActive())
					{
						bKeepMenuBarVisible = true;
					}

					ImGui::EndMainMenuBar();
				}
				ImGui::PopStyleVar();

				if (bKeepMenuBarVisible)
				{
					m_MenuBarAlpha = FMath::Min(MenuBarVisibilityDuration, m_MenuBarAlpha + ImGui::GetIO().DeltaTime * 16.f);
				}
				else
				{
					m_MenuBarAlpha = FMath::Max(0.f, m_MenuBarAlpha - ImGui::GetIO().DeltaTime * 4.f);
				}
			}

			for (FImGuiMenuContainer::FWidgetSlot& Slot : Slots)
			{
				TickMainMenuWidgets(MenuContainer, Slot, TickContext);
			}

			// we are done with the dock node
			// if we enter `TickImGuiInternal` again that would mean the event is coming from window resizing
			// so will have to add the dock node again!
			m_bIsDockNodeValid = false;
		}
	};

	struct FImGuiMenuExtension : public IInputProcessor
	{
		FImGuiMenuExtension()
		{
#if WITH_EDITOR
			if (GIsEditor)
			{
				m_ImGuiTabGroup = WorkspaceMenu::GetMenuStructure().GetToolsCategory()->AddGroup(
					IMGUI_FNAME("ImGui"), LOCTEXT("ImGuiGroupName", "ImGui"),
					FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Layout"), /*bSortChildren=*/true);

				if (CVarAddImGuiWidgetToLevelViewport.GetValueOnGameThread() == false)
				{
					FGlobalTabmanager::Get()->RegisterNomadTabSpawner(IMGUI_FNAME("ImGuiTab"), FOnSpawnTab::CreateRaw(this, &FImGuiMenuExtension::SpawnImGuiTab))
						.SetGroup(m_ImGuiTabGroup.ToSharedRef())
						.SetDisplayName(LOCTEXT("ImGuiMainTabTitle", "ImGui"))
						.SetTooltipText(LOCTEXT("ImGuiMainTabTooltip", "Window hosting static ImGui widgets"))
						.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Layout"));
				}

				FEditorDelegates::EndPIE.AddLambda(
					[this](bool bSimulating)
					{
						// make sure server window is closed (in case world teardown somehow misses it)
						TSharedPtr<SWindow> Window = m_PIEDedicatedServerWindow.Pin();
						if (Window)
						{
							Window->RequestDestroyWindow();
						}
					});
			}

			FWorldDelegates::OnWorldBeginTearDown.AddRaw(this, &FImGuiMenuExtension::OnWorldBeginTearDown);

			m_OpenImGuiMenuCommand = MakeUnique<FAutoConsoleCommandWithWorld>(
				TEXT("imgui.ToggleMenu"),
				TEXT("Toggles ImGui menu."),
				FConsoleCommandWithWorldDelegate::CreateRaw(this, &FImGuiMenuExtension::TogglePIEImGuiContext));

#else
			m_OpenImGuiMenuCommand = MakeUnique<FAutoConsoleCommandWithWorld>(
				TEXT("imgui.ToggleMenu"),
				TEXT("Toggles ImGui menu."),
				FConsoleCommandWithWorldDelegate::CreateRaw(this, &FImGuiMenuExtension::TogglePrimaryImGuiContext));
#endif

			FString RawChordString;
			if (GConfig && GConfig->GetString(TEXT("ImGuiPlugin"), TEXT("ToggleMenuKeyChord"), RawChordString, GInputIni))
			{
				UScriptStruct* StructReflection = FInputChord::StaticStruct();
				StructReflection->ImportText(*RawChordString, &m_ToggleMenuKeyChord, nullptr, EPropertyPortFlags::PPF_None, nullptr, FInputChord::StaticStruct()->GetName(), true);
			}
			if (GConfig && GConfig->GetString(TEXT("ImGuiPlugin"), TEXT("SetUIFocusKeyChord"), RawChordString, GInputIni))
			{
				UScriptStruct* StructReflection = FInputChord::StaticStruct();
				StructReflection->ImportText(*RawChordString, &m_SetUIFocusKeyChord, nullptr, EPropertyPortFlags::PPF_None, nullptr, FInputChord::StaticStruct()->GetName(), true);
			}
		}
		~FImGuiMenuExtension()
		{
#if WITH_EDITOR
			if (GEditor)
			{
#if USE_TOOLMENU_API
				UToolMenus::UnregisterOwner(this);
#else
				if (UPanelExtensionSubsystem* PanelExtensionSubsystem = GEditor->GetEditorSubsystem<UPanelExtensionSubsystem>())
				{
					PanelExtensionSubsystem->UnregisterPanelFactory(IMGUI_FNAME("LevelViewportToolBar.LeftExtension"), IMGUI_FNAME("ImGuiPlugin_Menu"));
				}
#endif

				FEditorDelegates::EndPIE.RemoveAll(this);
			}

			if (FSlateApplication::IsInitialized())
			{
				FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(IMGUI_FNAME("ImGuiTab"));
			}
			m_PIEContextWidgets.Reset();
#endif
			m_PrimaryContextWidget.Reset();

			m_OpenImGuiMenuCommand.Reset();
		}

		virtual void Tick(const float DeltaTime, FSlateApplication& SlateApp, TSharedRef<ICursor> Cursor) override
		{

		}
		virtual bool HandleKeyDownEvent(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent) override
		{
			if (!GEngine)
			{
				return false;
			}
			if (InKeyEvent.IsRepeat())
			{
				return false;
			}

			auto IsEventBoundToKey = [](const FKeyEvent& KeyEvent, const FInputChord& InputChord) -> bool
				{
					if (KeyEvent.GetKey() != InputChord.Key)
						return false;

					const uint8 KeyEventModifierState =
						((uint8)KeyEvent.IsControlDown() << 0) | ((uint8)KeyEvent.IsShiftDown() << 1) | ((uint8)KeyEvent.IsAltDown() << 2) | ((uint8)KeyEvent.IsCommandDown() << 3);
					const uint8 InputChordModifierState =
						((uint8)InputChord.NeedsControl() << 0) | ((uint8)InputChord.NeedsShift() << 1) | ((uint8)InputChord.NeedsAlt() << 2) | ((uint8)InputChord.NeedsCommand() << 3);

					return KeyEventModifierState == InputChordModifierState;
				};

			if (IsEventBoundToKey(InKeyEvent, m_SetUIFocusKeyChord))
			{
				ImGuiFocusHandler::SetUIFocus();
			}

			if (IsEventBoundToKey(InKeyEvent, m_ToggleMenuKeyChord))
			{
#if WITH_EDITOR
				TSharedPtr<SWidget> FocusedWidget = SlateApp.GetUserFocusedWidget(InKeyEvent.GetUserIndex());
				if (FocusedWidget.IsValid())
				{
					TSharedPtr<SWindow> WidgetWindow = SlateApp.FindWidgetWindow(FocusedWidget.ToSharedRef());
					// find the PIE world associated with the widget window
					for (const FWorldContext& Context : GEngine->GetWorldContexts())
					{
						UGameViewportClient* ViewportClient = Context.GameViewport;
						if (IsValid(ViewportClient) && ViewportClient->GetWindow() == WidgetWindow)
						{
							TogglePIEImGuiContext(ViewportClient->GetWorld());
							return true;
						}
					}
				}
#else
				UGameViewportClient* GameViewport = GEngine->GameViewport;
				if (IsValid(GameViewport))
				{
					TogglePrimaryImGuiContext(GameViewport->GetWorld());
					return true;
				}
#endif
			}

			return false;
		}
		virtual const TCHAR* GetDebugName() const { return TEXT("ImGuiMenuExtension"); }

#if WITH_EDITOR
		TSharedRef<FWorkspaceItem> GetImGuiTabGroup()
		{
			return m_ImGuiTabGroup.ToSharedRef();
		}

		TSharedRef<SDockTab> SpawnImGuiTab(const FSpawnTabArgs& SpawnTabArgs)
		{
			check(CVarAddImGuiWidgetToLevelViewport.GetValueOnGameThread() == false);

			TSharedPtr<SImGuiMainMenuWidget> MainMenuWidget = SNew(SImGuiMainMenuWidget)
				.MainViewportWindow(SpawnTabArgs.GetOwnerWindow())
				.AutoHideMenuBar(false);
			m_PrimaryContextWidget = MainMenuWidget;

			return SNew(SDockTab)
				.TabRole(ETabRole::NomadTab)
				[
					MainMenuWidget.ToSharedRef()
				];
		}

		void OnWorldBeginTearDown(UWorld* World)
		{
			for (auto Itr = m_PIEContextWidgets.CreateIterator(); Itr; ++Itr)
			{
				TSharedPtr<SImGuiMainMenuWidget> Widget = Itr->Pin();
				const UWorld* WidgetWorld = Widget ? Widget->GetWorld() : nullptr;
				if (!WidgetWorld || WidgetWorld == World)
				{
					Itr.RemoveCurrent();
				}
			}

			for (auto Itr = m_PIEMenuContainers.CreateIterator(); Itr; ++Itr)
			{
				const UWorld* MenuContainerWorld = Itr->GetWorld();
				if (!MenuContainerWorld || MenuContainerWorld == World)
				{
					Itr.RemoveCurrent();
				}
			}

			if (World->GetNetMode() == NM_DedicatedServer)
			{
				TSharedPtr<SWindow> Window = m_PIEDedicatedServerWindow.Pin();
				if (Window)
				{
					Window->RequestDestroyWindow();
				}
			}
		}

		void TogglePIEImGuiContext(UWorld* World)
		{
			if (!GIsEditor)
			{
				TogglePrimaryImGuiContext(World);
				return;
			}

			if (!World || !World->IsGameWorld())
			{
				return;
			}

			TSharedPtr<SImGuiMainMenuWidget> MainMenuWidget;
			for (int32 WidgetIndex = 0; WidgetIndex < m_PIEContextWidgets.Num(); ++WidgetIndex)
			{
				MainMenuWidget = m_PIEContextWidgets[WidgetIndex].Pin();
				if (MainMenuWidget && MainMenuWidget->GetWorld() == World)
				{
					break;
				}
				MainMenuWidget.Reset();
			}

			if (!MainMenuWidget)
			{
				UGameViewportClient* GameViewport = World->bIsTearingDown ? nullptr : World->GetGameViewport();
				if (GameViewport)
				{
					MainMenuWidget = SNew(SImGuiMainMenuWidget)
						.MainViewportWindow(GameViewport->GetWindow())
						.OwningWorld(World);
					GameViewport->AddViewportWidgetContent(MainMenuWidget.ToSharedRef(), TNumericLimits<int32>::Max());
					m_PIEContextWidgets.Add(MainMenuWidget);

					ImGuiFocusHandler::SetUIFocus();
				}
				else if (World->GetNetMode() == NM_DedicatedServer)
				{
					TSharedPtr<SWindow> Window = m_PIEDedicatedServerWindow.Pin();
					if (!Window)
					{
						Window = SNew(SWindow)
							.Title(FText::FromString("Server"))
							.ClientSize(FVector2f(512.f, 512.f))
							.AutoCenter(EAutoCenter::PrimaryWorkArea)
							.SupportsMaximize(true)
							.SupportsMinimize(true)
							.SizingRule(ESizingRule::UserSized);
						FSlateApplication::Get().AddWindow(Window.ToSharedRef());
						m_PIEDedicatedServerWindow = Window;
					}
					MainMenuWidget = SNew(SImGuiMainMenuWidget)
						.MainViewportWindow(Window)
						.OwningWorld(World)
						.AutoHideMenuBar(false);
					Window->SetContent(MainMenuWidget.ToSharedRef());
					m_PIEContextWidgets.Add(MainMenuWidget);

					ImGuiFocusHandler::SetUIFocus();
				}
				return;
			}

			if (World->GetNetMode() == NM_DedicatedServer)
			{
				TSharedPtr<SWindow> Window = m_PIEDedicatedServerWindow.Pin();
				if (Window)
				{
					Window->RequestDestroyWindow();
				}
			}
			else if (MainMenuWidget)
			{
				if (MainMenuWidget->GetVisibility() == EVisibility::Hidden)
				{
					MainMenuWidget->ShowWidget();
					ImGuiFocusHandler::SetUIFocus();
				}
				else
				{
					MainMenuWidget->HideWidget();
					ImGuiFocusHandler::ResetFocus();
				}
			}
		}
#endif //#if WITH_EDITOR

		void TogglePrimaryImGuiContext(UWorld* World)
		{
			TSharedPtr<SImGuiMainMenuWidget> MainMenuWidget = m_PrimaryContextWidget.Pin();
			if (!MainMenuWidget)
			{
				UGameViewportClient* GameViewport = World ? World->GetGameViewport() : nullptr;
				if (GameViewport)
				{
					MainMenuWidget = SNew(SImGuiMainMenuWidget)
						.MainViewportWindow(GameViewport->GetWindow());
					GameViewport->AddViewportWidgetContent(MainMenuWidget.ToSharedRef(), TNumericLimits<int32>::Max());
					m_PrimaryContextWidget = MainMenuWidget;

					ImGuiFocusHandler::SetUIFocus();
				}
				else if (!FSlateApplication::IsInitialized()) //running headles without slate
				{
					MainMenuWidget = SNew(SImGuiMainMenuWidget);
					m_PrimaryContextWidget = MainMenuWidget;
					m_PinnedPrimaryContextWidget = MainMenuWidget;
				}
				return;
			}

			if (MainMenuWidget)
			{
				if (MainMenuWidget->GetVisibility() == EVisibility::Hidden)
				{
					MainMenuWidget->ShowWidget();
					ImGuiFocusHandler::SetUIFocus();
				}
				else
				{
					MainMenuWidget->HideWidget();
					ImGuiFocusHandler::ResetFocus();
				}
			}
		}

		FImGuiMenuContainer& GetMenuContainer(const UWorld* World)
		{
#if WITH_EDITOR
			if (!World)
			{
				return m_PrimaryContextMenuContainer;
			}

			for (auto Itr = m_PIEMenuContainers.CreateIterator(); Itr; ++Itr)
			{
				if (Itr->GetWorld() == World)
				{
					return *Itr;
				}
			}
			FImGuiMenuContainer NewContainer = {};
			NewContainer.SetWorld(World);
			return m_PIEMenuContainers.Add_GetRef(MoveTemp(NewContainer));
#else
			return m_PrimaryContextMenuContainer;
#endif
		}

		FImGuiTickContext* GetWidgetTickContext(const UWorld* World) const
		{
			auto GetTickContextFromWidget = [](SImGuiMainMenuWidget* Widget) -> FImGuiTickContext*
				{
#if WITH_EDITOR
					if ((GFrameCounter - Widget->GetLastPaintFrameCounter()) > 1)
					{
						return nullptr;
					}
#endif
					ImGuiContext* ImguiContext = Widget->GetImGuiContext();
					// NOTE: additional `WithinFrameScope` check as the context will most likely be used for drawing widget
					// No point returning valid context if we cannot tick widgets
					return (ImguiContext && ImguiContext->WithinFrameScope) ? FImGuiTickContext::GetTickContextFromImGuiContext(ImguiContext) : nullptr;
				};

#if WITH_EDITOR
			if (!World)
			{
				TSharedPtr<SImGuiMainMenuWidget> MainMenuWidget = m_PrimaryContextWidget.Pin();
				if (MainMenuWidget)
				{
					return GetTickContextFromWidget(MainMenuWidget.Get());
				}
			}
			else
			{
				for (int32 WidgetIndex = 0; WidgetIndex < m_PIEContextWidgets.Num(); ++WidgetIndex)
				{
					TSharedPtr<SImGuiMainMenuWidget> MainMenuWidget = m_PIEContextWidgets[WidgetIndex].Pin();
					if (MainMenuWidget && MainMenuWidget->GetWorld() == World)
					{
						return GetTickContextFromWidget(MainMenuWidget.Get());
					}
				}
			}
#else
			TSharedPtr<SImGuiMainMenuWidget> MainMenuWidget = m_PrimaryContextWidget.Pin();
			if (MainMenuWidget)
			{
				return GetTickContextFromWidget(MainMenuWidget.Get());
			}
#endif
			return nullptr;
		}

#if WITH_EDITOR
		TSharedPtr<FWorkspaceItem> m_ImGuiTabGroup;
		TArray<FImGuiMenuContainer> m_PIEMenuContainers;
		TWeakPtr<SWindow> m_PIEDedicatedServerWindow;
		TArray<TWeakPtr<SImGuiMainMenuWidget>> m_PIEContextWidgets;
#endif

		// Editor/Game context widget
		TWeakPtr<SImGuiMainMenuWidget> m_PrimaryContextWidget;
		FImGuiMenuContainer m_PrimaryContextMenuContainer;

		// Keep the widget pinned when running headless (no windows around to keep the widget alive)
		TSharedPtr<SImGuiMainMenuWidget> m_PinnedPrimaryContextWidget;

		TUniquePtr<FAutoConsoleCommandWithWorld> m_OpenImGuiMenuCommand = nullptr;

		FInputChord m_ToggleMenuKeyChord;
		FInputChord m_SetUIFocusKeyChord;
	};
	static TSharedPtr<FImGuiMenuExtension> MenuExtensionHandle = nullptr;

	FImGuiMenuContainer& GetMenuContainerForWorld(const UWorld* World)
	{
		return MenuExtensionHandle->GetMenuContainer(World);
	}

	///////////////////////////////////////////////////////////////////////////////////////////////////////

#if WITH_EDITOR
	class SImGuiViewportToolBarButton : public SCompoundWidget
	{
	public:
		SLATE_BEGIN_ARGS(SImGuiViewportToolBarButton) {}
			SLATE_ARGUMENT(TWeakPtr<SLevelViewport>, LevelViewport);
		SLATE_END_ARGS()

		void Construct(const FArguments& InArgs)
		{
			LevelViewport = InArgs._LevelViewport;

#if USE_TOOLMENU_API
			ButtonStyle = FButtonStyle()
			.SetNormal(FSlateRoundedBoxBrush(FStyleColors::Dropdown, 4.f))
			.SetHovered(FSlateRoundedBoxBrush(FStyleColors::Hover, 4.f))
			.SetPressed(FSlateRoundedBoxBrush(FStyleColors::Header, 4.f))
			.SetDisabled(FSlateNoResource())
			.SetNormalForeground(FStyleColors::Foreground)
			.SetHoveredForeground(FStyleColors::ForegroundHover)
			.SetPressedForeground(FStyleColors::ForegroundHover)
			.SetDisabledForeground(FStyleColors::Foreground);
			ChildSlot
			[
				SNew(SButton)
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				.ContentPadding(FMargin(4.f, 0.f))
				.PressMethod(EButtonPressMethod::ButtonPress)
				.OnPressed(this, &SImGuiViewportToolBarButton::OnPressed)
				.OnClicked(this, &SImGuiViewportToolBarButton::OnClicked)
				.ButtonStyle(&ButtonStyle)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.FillWidth(1)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("ImGuiMainTabTitle", "ImGui"))
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.HAlign(HAlign_Right)
					.VAlign(VAlign_Center)
					.Padding(2.f, 0.f, 0.f, 0.f)
					[
						SNew(SImage)
						.Image(&FAppStyle::Get().GetWidgetStyle<FComboBoxStyle>("SimpleComboBox").ComboButtonStyle.DownArrowImage)
						.DesiredSizeOverride(FVector2D(10., 12.))
						.ColorAndOpacity(FSlateColor::UseForeground())
					]
				]
			];
#else
			ChildSlot
			[
				SNew(SButton)
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				.ContentPadding(FMargin(4.f, 0.f))
				.PressMethod(EButtonPressMethod::ButtonPress)
				.OnPressed(this, &SImGuiViewportToolBarButton::OnPressed)
				.OnClicked(this, &SImGuiViewportToolBarButton::OnClicked)
				.ButtonStyle(&FAppStyle::Get().GetWidgetStyle<FToolBarStyle>("EditorViewportToolBar").ButtonStyle)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("ImGuiMainTabTitle", "ImGui"))
				]
			];
#endif
		}

		void OnPressed()
		{
			TSharedPtr<SImGuiMainMenuWidget> MainMenuWidget = MenuExtensionHandle->m_PrimaryContextWidget.Pin();
			if (MainMenuWidget)
			{
				bRequestMenuOnClick = !MainMenuWidget->IsMenuOpen();
			}
			else
			{
				bRequestMenuOnClick = true;
			}
		}

		FReply OnClicked()
		{
			TSharedPtr<SLevelViewport> Viewport = LevelViewport.Pin();
			if (Viewport)
			{
				TSharedPtr<SWindow> ViewportWindow = FSlateApplication::Get().FindWidgetWindow(Viewport.ToSharedRef());

				TSharedPtr<SImGuiMainMenuWidget> MainMenuWidget = MenuExtensionHandle->m_PrimaryContextWidget.Pin();
				if (MainMenuWidget)
				{
					// handle split viewport switching!
					TSharedPtr<SWidget> PreviousOverlayWidget = MainMenuWidget->GetLevelViewportOverlayWidget();
					TSharedPtr<SLevelViewport> PreviousLevelViewport = MainMenuWidget->GetLevelViewport();
					if (PreviousLevelViewport != Viewport)
					{
						if (PreviousLevelViewport && PreviousOverlayWidget)
						{
							PreviousLevelViewport->RemoveOverlayWidget(PreviousOverlayWidget.ToSharedRef());
						}
						MainMenuWidget.Reset();
					}
				}

				if (!MainMenuWidget)
				{
					// here we create a 8x8 widget with inputs disabled on the main ImGui widget
					// the setup works by forcing separate platform windows for each ImGui window with input handling
					// otherwise it interferes with PIE context and a lot of other input/focus related issues

					MainMenuWidget = SNew(SImGuiMainMenuWidget).MainViewportWindow(ViewportWindow);
					MainMenuWidget->SetVisibility(EVisibility::HitTestInvisible);

					TSharedRef<SConstraintCanvas> Canvas = SNew(SConstraintCanvas);
					Canvas->AddSlot()
						.AutoSize(true)
						.Anchors(FAnchors(0.5f))
						.Offset(FMargin(0.f))
						.Alignment(FVector2D(0.5, 0.5))
						[
							MainMenuWidget.ToSharedRef()
						];
					Viewport->AddOverlayWidget(Canvas);

					OverlayWidget = Canvas;
					MainMenuWidget->SetLevelViewport(Viewport, Canvas);
					MenuExtensionHandle->m_PrimaryContextWidget = MainMenuWidget;
				}
				else
				{
					if (FSlateApplication::Get().GetModifierKeys().IsShiftDown())
					{
						MainMenuWidget->SetVisibility(EVisibility::Hidden);
					}
					else
					{
						MainMenuWidget->SetVisibility(EVisibility::HitTestInvisible);
						if (bRequestMenuOnClick)
						{
							MainMenuWidget->OpenMenu();
						}
					}
				}

				// adjust offset to match the viewport toolbar button
				if (MainMenuWidget)
				{
					FVector2f MenuOffset = GetTickSpaceGeometry().GetRenderBoundingRect().GetBottomLeft2f();
					MainMenuWidget->SetMenuOffset(ImVec2(MenuOffset.X, MenuOffset.Y));
				}
			}
			return FReply::Handled();
		}

		FButtonStyle ButtonStyle;

		// container for ImGui main menu widget
		TWeakPtr<SWidget> OverlayWidget;

		// parent level viewport
		TWeakPtr<SLevelViewport> LevelViewport;

		// should we open the menu on click?
		bool bRequestMenuOnClick = false;
	};

	static FDelayedAutoRegisterHelper ImGuiViewportToolbar_DelayedAutoRegister(EDelayedRegisterRunPhase::EndOfEngineInit,
		[]()
		{
			if (CVarAddImGuiWidgetToLevelViewport.GetValueOnGameThread() == false || !GEditor)
			{
				return;
			}

#if USE_TOOLMENU_API
			FToolMenuOwnerScoped ScopedOwner(MenuExtensionHandle.Get());

			// copy of `FToolMenuEntry::InitWidget` as the widget creation requires access to `ULevelViewportContext`
			FToolMenuEntry Entry(UToolMenus::Get()->CurrentOwner(), "ImGuiTab", EMultiBlockType::Widget);
			Entry.Label = LOCTEXT("ImGuiMainTabTitle", "ImGui");
			Entry.ToolTip = LOCTEXT("ImGuiMainTabTitle", "ImGui");
			Entry.MakeCustomWidget.BindLambda(
				[](const FToolMenuContext& Context, const FToolMenuCustomWidgetContext&) -> TSharedRef<SWidget>
				{
					const ULevelViewportContext* LevelViewportContext = Context.FindContext<ULevelViewportContext>();
					if (ensureAlways(LevelViewportContext))
					{
						return SNew(SImGuiViewportToolBarButton).LevelViewport(LevelViewportContext->LevelViewport);
					}
					return SNullWidget::NullWidget;
				});
			Entry.InsertPosition.Position = EToolMenuInsertType::First;

			UToolMenu* Menu = UToolMenus::Get()->ExtendMenu("LevelEditor.ViewportToolbar");
			FToolMenuSection& RightSection = Menu->FindOrAddSection("Right");
			RightSection.AddEntry(MoveTemp(Entry));
#else
			if (UPanelExtensionSubsystem* PanelExtensionSubsystem = GEditor->GetEditorSubsystem<UPanelExtensionSubsystem>())
			{
				FPanelExtensionFactory MenuWidget;
				MenuWidget.CreateExtensionWidget = FPanelExtensionFactory::FCreateExtensionWidget::CreateLambda(
					[](FWeakObjectPtr ExtensionContext) -> TSharedRef<SWidget>
					{
						if (const UViewportToolBarContext* ExtensionContextObject = Cast<UViewportToolBarContext>(ExtensionContext.Get()); ensure(ExtensionContextObject))
						{
							return SNew(SImGuiViewportToolBarButton).LevelViewport(ExtensionContextObject->Viewport);
						}
						return SNullWidget::NullWidget;
					});
				MenuWidget.Identifier = IMGUI_FNAME("ImGuiPlugin_Menu");
				PanelExtensionSubsystem->RegisterPanelFactory(IMGUI_FNAME("LevelViewportToolBar.LeftExtension"), MenuWidget);
			}
#endif
		});
#endif //#if WITH_EDITOR

	///////////////////////////////////////////////////////////////////////////////////////////////////////

#if WITH_EDITOR
	static TSharedRef<SDockTab> SpawnWidgetTab(const FSpawnTabArgs& SpawnTabArgs, FImGuiWidgetRegisterParams RegisterParams)
	{
		return SNew(SDockTab)
			.TabRole(ETabRole::NomadTab)
			[
				SNew(SImGuiWidget)
				.MainViewportWindow(SpawnTabArgs.GetOwnerWindow())
				.OnTickDelegate(FOnTickImGuiWidgetDelegate::CreateStatic(RegisterParams.TickFunction))
				.ConfigFileName(RegisterParams.GetWidetName())
				.bEnableViewports(RegisterParams.bEnableViewports)
				.bTickDelegateCreatesWindow(RegisterParams.bSkipWindowCreation)
			];
	}
	TSharedRef<FWorkspaceItem> GetMenuRoot(FName MenuName)
	{
		// "Window" section
		if (MenuName == IMGUI_FNAME("Menu"))	return WorkspaceMenu::GetMenuStructure().GetStructureRoot();
		if (MenuName == IMGUI_FNAME("Window"))	return WorkspaceMenu::GetMenuStructure().GetStructureRoot();

		// "Tools" section
		if (MenuName == IMGUI_FNAME("Tools"))	return WorkspaceMenu::GetMenuStructure().GetToolsStructureRoot();

		// fallback to "Tools.ImGui" section
		return MenuExtensionHandle->GetImGuiTabGroup();
	}
	static TSharedRef<FWorkspaceItem> GetMenuGroup(const char* MenuPath)
	{
		FAnsiString Path(MenuPath);
		// for convenience inject "Instrumentation" section manually
		if (Path.StartsWith("Tools.Debug", ESearchCase::IgnoreCase)		||
			Path.StartsWith("Tools.Profile", ESearchCase::IgnoreCase)	||
			Path.StartsWith("Tools.Audit", ESearchCase::IgnoreCase)		||
			Path.StartsWith("Tools.Platforms", ESearchCase::IgnoreCase))
		{
			Path.InsertAt(5, ".Instrumentation");
		}

		FMenuPathIterator PathItr{ *Path };
		TSharedRef<FWorkspaceItem> MenuGroup = GetMenuRoot(PathItr.GetNextGroup());
		while (PathItr)
		{
			FName NextGroupName = PathItr.GetNextGroup();
			if (NextGroupName.IsNone())
			{
				break;
			}

			const TSharedRef<FWorkspaceItem>* Menu = MenuGroup->GetChildItems().FindByPredicate([&](const auto& Entry) { return Entry->GetFName() == NextGroupName; });
			if (!Menu)
			{
				MenuGroup = MenuGroup->AddGroup(NextGroupName, FText::FromString(*NextGroupName.ToString()), FSlateIcon(), true);
			}
			else
			{
				MenuGroup = *Menu;
			}
		}

		return MenuGroup;
	}
#endif //#if WITH_EDITOR

	///////////////////////////////////////////////////////////////////////////////////////////////////////

	void RegisterMenuExtensions()
	{
		MenuExtensionHandle = MakeShared<FImGuiMenuExtension>();
		if (FSlateApplication::IsInitialized() &&
			(MenuExtensionHandle->m_ToggleMenuKeyChord.IsValidChord() || MenuExtensionHandle->m_SetUIFocusKeyChord.IsValidChord()))
		{
			FSlateApplication::Get().RegisterInputPreProcessor(MenuExtensionHandle);
		}

		ImGuiFocusHandler::RegisterFocusChangedEvents();
	}

	void UnregisterMenuExtensions()
	{
		ImGuiFocusHandler::UnregisterFocusChangedEvents();

		MenuExtensionHandle = nullptr;
	}

} //namespace ImGuiUtils

FImGuiTickContext* GetMainMenuWidgetTickContextForWorld(const UWorld* World)
{
	return ImGuiUtils::MenuExtensionHandle ? ImGuiUtils::MenuExtensionHandle->GetWidgetTickContext(World) : nullptr;
}

void RegisterMainMenuWidgetForWorld(
	const UWorld* World, const char* WidgetPath, const char* WidgetToolTip, const FSlateBrush* WidgetIcon,
	FOnTickImGuiWidgetDelegate TickDelegate, EImGuiMainMenuWidgetFlags WidgetFlags)
{
	if (!ensureAlways(ImGuiUtils::MenuExtensionHandle))
	{
		return;
	}

	auto& MenuContainer = ImGuiUtils::GetMenuContainerForWorld(World);
	MenuContainer.RegisterWidget(WidgetPath, WidgetToolTip, WidgetIcon, TickDelegate, WidgetFlags);
}

void UnregisterMainMenuWidgetForWorld(const UWorld* World, const char* WidgetPath)
{
	if (!ensureAlways(ImGuiUtils::MenuExtensionHandle))
	{
		return;
	}

	auto& MenuContainer = ImGuiUtils::GetMenuContainerForWorld(World);
	MenuContainer.UnregisterWidget(WidgetPath);
}

bool* GetMainMenuWidgetActiveStateForWorld(const UWorld* World, const char* WidgetPath)
{
	if (!ensureAlways(ImGuiUtils::MenuExtensionHandle))
	{
		return nullptr;
	}

	auto& MenuContainer = ImGuiUtils::GetMenuContainerForWorld(World);
	ImGuiUtils::FImGuiMenuContainer::FWidgetSlot* ParentSlot = MenuContainer.FindSlot(WidgetPath);
	ImGuiUtils::FImGuiMenuContainer::FWidgetSlot* Slot = nullptr;
	if (ParentSlot)
	{
		Slot = ParentSlot->GetChildren().FindByKey(WidgetPath);
	}
	else
	{
		Slot = MenuContainer.WidgetSlots.FindByKey(WidgetPath);
	}
	return Slot ? &Slot->bIsActive : nullptr;
}

FAutoRegisterMainMenuWidget::FAutoRegisterMainMenuWidget(FImGuiWidgetRegisterParams RegisterParams)
{
	if (!ensureAlways(RegisterParams.IsValid()))
	{
		return;
	}

	EImGuiMainMenuWidgetFlags WidgetFlags = EImGuiMainMenuWidgetFlags::None;
	if (RegisterParams.bTickInMenuBar)
	{
		WidgetFlags |= EImGuiMainMenuWidgetFlags::TickInMenuBar;
	}
	if (RegisterParams.bSkipWindowCreation)
	{
		WidgetFlags |= EImGuiMainMenuWidgetFlags::SkipWindowCreation;
	}
	if (RegisterParams.bRightAligned)
	{
		WidgetFlags |= EImGuiMainMenuWidgetFlags::RightAligned;
	}

	if (UImGuiSubsystem* ImGuiSubsystem = UImGuiSubsystem::Get())
	{
		RegisterParams.InitFunction();
		ImGuiUtils::GetMenuContainerForWorld(nullptr).RegisterWidget(
			RegisterParams.WidgetPath, RegisterParams.WidgetDescription, RegisterParams.WidgetIcon.GetOptionalIcon(),
			FOnTickImGuiWidgetDelegate::CreateStatic(RegisterParams.TickFunction), WidgetFlags);
	}
	else
	{
		UImGuiSubsystem::OnSubsystemInitialized.AddLambda(
			[RegisterParams, WidgetFlags](UImGuiSubsystem* ImGuiSubsystem)
			{
				RegisterParams.InitFunction();
				ImGuiUtils::GetMenuContainerForWorld(nullptr).RegisterWidget(
					RegisterParams.WidgetPath, RegisterParams.WidgetDescription, RegisterParams.WidgetIcon.GetOptionalIcon(),
					FOnTickImGuiWidgetDelegate::CreateStatic(RegisterParams.TickFunction), WidgetFlags);
			});
	}
}

#if WITH_EDITOR
FAutoRegisterStandaloneWidget::FAutoRegisterStandaloneWidget(FImGuiWidgetRegisterParams RegisterParams)
{
	if (!GIsEditor)
	{
		// when running with '-game' argument push everything to the main menu (same behaviour as packaged builds)
		FAutoRegisterMainMenuWidget RegisterMainMenuWidget{ MoveTemp(RegisterParams) };
		return;
	}

	if (!ensureAlways(RegisterParams.IsValid()))
	{
		return;
	}
	if (!ensureAlwaysMsgf(!RegisterParams.bTickInMenuBar, TEXT("Standalone widgets should not be using `bTickInMenuBar` option")))
	{
		return;
	}

	ImGuiUtils::FMenuPathIterator PathItr{ RegisterParams.WidgetPath };

	if (UImGuiSubsystem* ImGuiSubsystem = UImGuiSubsystem::Get())
	{
		RegisterParams.InitFunction();
		FGlobalTabmanager::Get()->RegisterNomadTabSpawner(FName(RegisterParams.GetWidetName()), FOnSpawnTab::CreateStatic(&ImGuiUtils::SpawnWidgetTab, RegisterParams))
			.SetGroup(ImGuiUtils::GetMenuGroup(RegisterParams.WidgetPath))
			.SetDisplayName(FText::FromString(UTF8_TO_TCHAR(RegisterParams.GetWidetName())))
			.SetTooltipText(FText::FromString(UTF8_TO_TCHAR(RegisterParams.WidgetDescription)))
			.SetIcon(RegisterParams.WidgetIcon);
	}
	else
	{
		UImGuiSubsystem::OnSubsystemInitialized.AddLambda(
			[RegisterParams](UImGuiSubsystem* ImGuiSubsystem)
			{
				RegisterParams.InitFunction();
				FGlobalTabmanager::Get()->RegisterNomadTabSpawner(FName(RegisterParams.GetWidetName()), FOnSpawnTab::CreateStatic(&ImGuiUtils::SpawnWidgetTab, RegisterParams))
					.SetGroup(ImGuiUtils::GetMenuGroup(RegisterParams.WidgetPath))
					.SetDisplayName(FText::FromString(UTF8_TO_TCHAR(RegisterParams.GetWidetName())))
					.SetTooltipText(FText::FromString(UTF8_TO_TCHAR(RegisterParams.WidgetDescription)))
					.SetIcon(RegisterParams.WidgetIcon);
			});
	}
}
#endif //#if WITH_EDITOR

#undef LOCTEXT_NAMESPACE

#endif //#ifdef IMGUI_ALLOW_MENUBAR_EXTENSION