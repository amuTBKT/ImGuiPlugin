// Copyright 2024-26 Amit Kumar Mehar. All Rights Reserved.

#if WITH_ENGINE

#include "Modules/ModuleManager.h"
#include "Modules/ModuleInterface.h"

#include "Engine/World.h"
#include "TimerManager.h"
#include "Engine/Engine.h"
#include "SImGuiWidgets.h"
#include "ImGuiSubsystem.h"
#include "UObject/Package.h"
#include "InputKeyEventArgs.h"
#include "Algo/BinarySearch.h"
#include "HAL/IConsoleManager.h"
#include "Misc/ConfigCacheIni.h"
#include "Engine/GameViewportClient.h"
#include "Runtime/Launch/Resources/Version.h"
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

#define LOCTEXT_NAMESPACE "ImGuiPlugin"

static TAutoConsoleVariable<bool> CVarAddImGuiWidgetToLevelViewport(
	TEXT("imgui.UseLevelViewport"),
	false,
	TEXT("Prefer LevelViewport over Tabbed window for hosting ImGui editor widget.\n")
	TEXT("This only applies for the Editor context, doesn't affect PIE or Game context."),
	ECVF_ReadOnly);

///////////////////////////////////////////////////////////////////////////////////////////////////////

struct FImGuiMenuContainer
{
	struct FWidgetSlot
	{
		struct FPathIterator
		{
			FPathIterator(FAnsiStringView InPath)
				: Path(InPath)
				, ItrOffset(0)
			{}

			FAnsiStringView operator++()
			{
				int32 DelimitedIndex;
				if (Path.Mid(ItrOffset).FindChar('.', DelimitedIndex))
				{
					ItrOffset += DelimitedIndex + 1;
					return Path.Mid(0, ItrOffset - 1);
				}
				return {};
			}

			operator bool() const
			{
				return ItrOffset < Path.Len();
			}

			FAnsiStringView Path;
			int32 ItrOffset;
		};

		explicit FWidgetSlot(FAnsiString InPath)
			: Path(MoveTemp(InPath))
			, Storage(TInPlaceType<TArray<FWidgetSlot>>(), TArray<FWidgetSlot>{})
			, SlotNameOffset(Path.FindLastChar('.', SlotNameOffset) ? SlotNameOffset + 1 : 0)
		{}

		explicit FWidgetSlot(FAnsiString InPath, FAnsiString InToolTip, const FSlateBrush* InIcon, FOnTickImGuiWidgetDelegate InTickDelegate, EImGuiMainMenuWidgetFlags InWidgetFlags)
			: Path(MoveTemp(InPath))
			, ToolTip(MoveTemp(InToolTip))
			, Icon(InIcon)
			, Storage(TInPlaceType<FOnTickImGuiWidgetDelegate>(), MoveTemp(InTickDelegate))
			, SlotNameOffset(Path.FindLastChar('.', SlotNameOffset) ? SlotNameOffset + 1 : 0)
			, WidgetFlags(InWidgetFlags)
		{}

		const char*							GetName()			const { return *Path + SlotNameOffset; };
		bool								IsMenuItem()		const { return Storage.IsType<FOnTickImGuiWidgetDelegate>(); }
		const FOnTickImGuiWidgetDelegate&	GetTickDelegate()	const { check(IsMenuItem());  return Storage.Get<FOnTickImGuiWidgetDelegate>(); }
		TArray<FWidgetSlot>&				GetChildren()			  { check(!IsMenuItem()); return Storage.Get<TArray<FWidgetSlot>>(); }
		const TArray<FWidgetSlot>&			GetChildren()		const { check(!IsMenuItem()); return Storage.Get<TArray<FWidgetSlot>>(); }

		bool operator==(const FAnsiStringView& Other)	const { return Other.Equals(*Path, ESearchCase::IgnoreCase); }
		bool operator==(const FWidgetSlot& Other)		const { return FCStringAnsi::Stricmp(*Path, *Other.Path) == 0; }
		bool operator<(const FWidgetSlot& Other)		const { return FCStringAnsi::Stricmp(*Path, *Other.Path) < 0; }

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

	TPair<EFindSlotResult, FWidgetSlot*> FindOrCreateSlot_Internal(FAnsiStringView Path, bool bCreateHierarchy)
	{
		FWidgetSlot::FPathIterator PathItr{ Path };

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
				ParentSlot = AddSlotSorted(WidgetSlots, FWidgetSlot(FAnsiString(SubPath)));
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
		return FindOrCreateSlot_Internal(Path, /*bCreateHierarchy=*/false).Value;
	}
	TPair<EFindSlotResult, FWidgetSlot*> FindOrCreateSlot(FAnsiStringView Path)
	{
		return FindOrCreateSlot_Internal(Path, /*bCreateHierarchy=*/true);
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

		auto FoundSlot = FindOrCreateSlot(WidgetPath);
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
	void				 EndIterationOverMainMenuWidgetSlots()	 { QueueWidgetSlotChanges = false; ProcessQueuedMainWidgetSlots(); }

#if WITH_EDITOR
	void SetWorld(const UWorld* World)
	{
		OwningWorld = World;
		if (World)
		{
			int32 PIEInstanceId = World->GetOutermost()->GetPIEInstanceID();
			if (PIEInstanceId != INDEX_NONE)
			{
				SaveDataSectionName = FString::Printf(TEXT("MainMenuBar_%i"), PIEInstanceId);
			}
		}
	}
	const UWorld* GetWorld() const { return OwningWorld.Get(); }
	TWeakObjectPtr<const UWorld> OwningWorld;
#endif

#if WITH_EDITOR
	FString						 SaveDataSectionName = TEXT("MainMenuBar_Editor");
#else
	FString						 SaveDataSectionName = TEXT("MainMenuBar_Game");
#endif
	TArray<FQueuedWidgetSlot>	 WidgetSlotsToAdd;
	TArray<FQueuedWidgetSlot>	 WidgetSlotsToRemove;
	TArray<FWidgetSlot>			 WidgetSlots;
	bool						 QueueWidgetSlotChanges = false;
};

FImGuiMenuContainer& GetMenuContainerForWorld(const UWorld* World);

void RegisterMainMenuWidgetForWorld(
	const UWorld* World, const char* WidgetPath, const char* WidgetToolTip, const FSlateBrush* WidgetIcon,
	FOnTickImGuiWidgetDelegate TickDelegate, EImGuiMainMenuWidgetFlags WidgetFlags)
{
	FImGuiMenuContainer& MenuContainer = GetMenuContainerForWorld(World);
	MenuContainer.RegisterWidget(WidgetPath, WidgetToolTip, WidgetIcon, TickDelegate, WidgetFlags);
}

void UnregisterMainMenuWidgetForWorld(const UWorld* World, const char* WidgetPath)
{
	FImGuiMenuContainer& MenuContainer = GetMenuContainerForWorld(World);
	MenuContainer.UnregisterWidget(WidgetPath);
}

bool* GetMainMenuWidgetActiveStateForWorld(const UWorld* World, const char* WidgetPath)
{
	FImGuiMenuContainer& MenuContainer = GetMenuContainerForWorld(World);
	FImGuiMenuContainer::FWidgetSlot* ParentSlot = MenuContainer.FindSlot(WidgetPath);
	FImGuiMenuContainer::FWidgetSlot* Slot = nullptr;
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

namespace ImGuiFocusHandler
{
	// give focus back to game viewport
	void ResetFocus()
	{
		FSlateApplication::Get().SetAllUserFocusToGameViewport(EFocusCause::SetDirectly);
	}

	// same logic as Shift+F1
	void SetUIFocus()
	{
		ExecuteOnGameThread(TEXT("ImGui_RetainFocus"),
			[]()
			{
				FSlateApplication::Get().ClearKeyboardFocus(EFocusCause::SetDirectly);
				FSlateApplication::Get().ResetToDefaultInputSettings();
			});
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////////

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

	if (UImGuiSubsystem* ImGuiSubsystem = UImGuiSubsystem::Get())
	{
		RegisterParams.InitFunction();
		GetMenuContainerForWorld(nullptr).RegisterWidget(
			RegisterParams.WidgetPath, RegisterParams.WidgetDescription, RegisterParams.WidgetIcon.GetOptionalIcon(),
			FOnTickImGuiWidgetDelegate::CreateStatic(RegisterParams.TickFunction), WidgetFlags);
	}
	else
	{
		UImGuiSubsystem::OnSubsystemInitialized.AddLambda(
			[RegisterParams, WidgetFlags](UImGuiSubsystem* ImGuiSubsystem)
			{
				RegisterParams.InitFunction();
				GetMenuContainerForWorld(nullptr).RegisterWidget(
					RegisterParams.WidgetPath, RegisterParams.WidgetDescription, RegisterParams.WidgetIcon.GetOptionalIcon(),
					FOnTickImGuiWidgetDelegate::CreateStatic(RegisterParams.TickFunction), WidgetFlags);
			});
	}
}

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

FAutoRegisterStandaloneWidget::FAutoRegisterStandaloneWidget(FImGuiWidgetRegisterParams RegisterParams)
{
	if (!GIsEditor)
	{
		// when running with '-game' argument push everything to the main menu (same behaviour as packaged builds)
		FAutoRegisterMainMenuWidget RegisterMainMenuWidget{ MoveTemp(RegisterParams) };
		return;
	}

	extern TSharedRef<FWorkspaceItem> GetImGuiTabGroup();

	if (!ensureAlways(RegisterParams.IsValid()))
	{
		return;
	}
	if (!ensureAlwaysMsgf(!RegisterParams.bTickInMenuBar, TEXT("Standalone widgets should not be using `bTickInMenuBar` option")))
	{
		return;
	}

	if (UImGuiSubsystem* ImGuiSubsystem = UImGuiSubsystem::Get())
	{
		RegisterParams.InitFunction();
		FGlobalTabmanager::Get()->RegisterNomadTabSpawner(FName(RegisterParams.GetWidetName()), FOnSpawnTab::CreateStatic(&SpawnWidgetTab, RegisterParams))
			.SetGroup(GetImGuiTabGroup())
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
				FGlobalTabmanager::Get()->RegisterNomadTabSpawner(FName(RegisterParams.GetWidetName()), FOnSpawnTab::CreateStatic(&SpawnWidgetTab, RegisterParams))
					.SetGroup(GetImGuiTabGroup())
					.SetDisplayName(FText::FromString(UTF8_TO_TCHAR(RegisterParams.GetWidetName())))
					.SetTooltipText(FText::FromString(UTF8_TO_TCHAR(RegisterParams.WidgetDescription)))
					.SetIcon(RegisterParams.WidgetIcon);
			});
	}
}
#endif //#if WITH_EDITOR

///////////////////////////////////////////////////////////////////////////////////////////////////////

/* main menu widget, only one instance per world */
class SImGuiMainMenuWidget : public SImGuiWidgetBase
{
	using Super = SImGuiWidgetBase;
public:
	SLATE_BEGIN_ARGS(SImGuiMainMenuWidget)
		: _MainViewportWindow(nullptr)
		, _OwningWorld(nullptr)
		{
		}
		SLATE_ARGUMENT(TSharedPtr<SWindow>, MainViewportWindow);
		SLATE_ARGUMENT(const UWorld*, OwningWorld);
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs)
	{
		FAnsiString ConfigFileName = "ImGui";
		if (InArgs._OwningWorld)
		{
			int32 PIEInstanceId = InArgs._OwningWorld->GetOutermost()->GetPIEInstanceID();
			if (PIEInstanceId != INDEX_NONE)
			{
				ConfigFileName = FAnsiString::Printf("ImGui_%i", PIEInstanceId);
			}
		}

		Super::Construct(
			Super::FArguments()
			.MainViewportWindow(InArgs._MainViewportWindow)
			.ConfigFileName(*ConfigFileName));

		m_OwningWorld = InArgs._OwningWorld;

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

private:
	void BeginFrame()
	{
		if (!GetVisibility().IsVisible())
		{
			return;
		}

		FImGuiTickScope Scope{ GetTickContext() };

		BeginImGuiFrame(GetCachedGeometry());

		SetupDockNode();
	}

	void EndFrame()
	{
		FImGuiTickScope Scope{ GetTickContext() };

		EndImGuiFrame();
		m_bIsDockNodeValid = false;
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
				ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport(), bIsEditorContext ? ImGuiDockNodeFlags_None : ImGuiDockNodeFlags_PassthruCentralNode);
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
				Slot.GetTickDelegate().ExecuteIfBound(TickContext);
			}
			else
			{
				FImGuiImageBindingParams Icon = Slot.Icon ? m_ImGuiSubsystem->RegisterOneFrameResource(Slot.Icon, ImGui::GetTextLineHeight()) : FImGuiImageBindingParams{};
				if (FImGui::MenuItem(Slot.GetName(), Slot.bIsActive, Icon))
				{
					Slot.bIsActive = !Slot.bIsActive;
				}
				ImGui::SetItemTooltip(*Slot.ToolTip);
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

		FImGuiTickScope Scope{ TickContext };

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

		auto RunMainMenuTickLogic = [&]()
			{
				TickContext->bIsTickingMainMenuBar = true;
				for (FImGuiMenuContainer::FWidgetSlot& Slot : Slots)
				{
					if (Slot.IsMenuItem())
					{
						Slot.GetTickDelegate().ExecuteIfBound(TickContext);
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
				TickContext->bIsTickingMainMenuBar = false;
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

						RunMainMenuTickLogic();

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
			if (ImGui::BeginMainMenuBar())
			{
				RunMainMenuTickLogic();
				ImGui::EndMainMenuBar();
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
				+SHorizontalBox::Slot()
				.FillWidth(1)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("ImGuiMainTabTitle", "ImGui"))
				]
				+SHorizontalBox::Slot()
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

	void OnPressed();
	FReply OnClicked();

	FButtonStyle ButtonStyle;

	// container for ImGui main menu widget
	TWeakPtr<SWidget> OverlayWidget;

	// parent level viewport
	TWeakPtr<SLevelViewport> LevelViewport;

	// should we open the menu on click?
	bool bRequestMenuOnClick = false;
};
#endif //#if WITH_EDITOR

static FDelayedAutoRegisterHelper ImGuiSubsystem_DelayedAutoRegister(EDelayedRegisterRunPhase::EndOfEngineInit,
	[]()
	{
		UImGuiSubsystem::InitializeSubsystem();
	});

class FImGuiRuntimeModule : public IModuleInterface
{
private:
	virtual void StartupModule() override
	{
		if (!UImGuiSubsystem::ShouldEnableImGui())
		{
			return;
		}

		IMGUI_CHECKVERSION();
		IMGUI_SETUP_DEFAULT_ALLOCATOR();

#if WITH_EDITOR
		m_ImGuiTabGroup = WorkspaceMenu::GetMenuStructure().GetToolsCategory()->AddGroup(
			LOCTEXT("ImGuiGroupName", "ImGui"),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Layout"), /*bSortChildren=*/true);

		if (CVarAddImGuiWidgetToLevelViewport.GetValueOnGameThread() == false && GIsEditor)
		{
			FGlobalTabmanager::Get()->RegisterNomadTabSpawner(IMGUI_FNAME("ImGuiTab"), FOnSpawnTab::CreateRaw(this, &FImGuiRuntimeModule::SpawnImGuiTab))
				.SetGroup(m_ImGuiTabGroup.ToSharedRef())
				.SetDisplayName(LOCTEXT("ImGuiMainTabTitle", "ImGui"))
				.SetTooltipText(LOCTEXT("ImGuiMainTabTooltip", "Window hosting static ImGui widgets"))
				.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Layout"));
		}

		FWorldDelegates::OnWorldBeginTearDown.AddRaw(this, &FImGuiRuntimeModule::OnWorldBeginTearDown);

		m_OpenImGuiMenuCommand = MakeUnique<FAutoConsoleCommandWithWorld>(
			TEXT("imgui.ToggleMenu"),
			TEXT("Toggles ImGui menu."),
			FConsoleCommandWithWorldDelegate::CreateRaw(this, &FImGuiRuntimeModule::TogglePIEImGuiContext));

#else
		UGameViewportClient::OnViewportCreated().AddRaw(this, &FImGuiRuntimeModule::OnViewportCreated);

		m_OpenImGuiMenuCommand = MakeUnique<FAutoConsoleCommandWithWorld>(
			TEXT("imgui.ToggleMenu"),
			TEXT("Toggles ImGui menu."),
			FConsoleCommandWithWorldDelegate::CreateRaw(this, &FImGuiRuntimeModule::TogglePrimaryImGuiContext));
#endif
	}

	virtual void ShutdownModule() override
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
		}

		if (FSlateApplication::IsInitialized())
		{
			FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(IMGUI_FNAME("ImGuiTab"));
		}
		m_PIEContextWidgets.Reset();
#endif
		m_PrimaryContextWidget.Reset();

		UGameViewportClient::OnViewportCreated().RemoveAll(this);

		m_OpenImGuiMenuCommand.Reset();
	}

#if WITH_EDITOR
	TSharedRef<SDockTab> SpawnImGuiTab(const FSpawnTabArgs& SpawnTabArgs)
	{
		check(CVarAddImGuiWidgetToLevelViewport.GetValueOnGameThread() == false);

		TSharedPtr<SImGuiMainMenuWidget> MainMenuWidget = SNew(SImGuiMainMenuWidget)
			.MainViewportWindow(SpawnTabArgs.GetOwnerWindow());
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

		for (auto Itr = m_MenuContainers.CreateIterator(); Itr; ++Itr)
		{
			if (Itr->GetWorld() == World)
			{
				Itr.RemoveCurrent();
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

		if (!World->IsGameWorld())
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
			return;
		}

		if (MainMenuWidget)
		{
			if (MainMenuWidget->GetVisibility() == EVisibility::Visible)
			{
				MainMenuWidget->SetVisibility(EVisibility::Hidden);
				ImGuiFocusHandler::ResetFocus();
			}
			else
			{
				MainMenuWidget->SetVisibility(EVisibility::Visible);
				ImGuiFocusHandler::SetUIFocus();
			}
		}
	}
#else
	void OnViewportCreated()
	{
		UGameViewportClient* GameViewport = GEngine->GameViewport;
		if (IsValid(GameViewport))
		{
			GameViewport->OnInputKey().AddRaw(this, &FImGuiRuntimeModule::HandleViewportInputKeyEvent);
		}
	}

	void HandleViewportInputKeyEvent(const FInputKeyEventArgs& EventArgs)
	{
		if (EventArgs.Event != EInputEvent::IE_Pressed)
		{
			return;
		}

		FModifierKeysState KeyState = FSlateApplication::Get().GetModifierKeys();
		// take focus from viewport client and show the mouse cursor (similar to pressing Shift+F1 during PIE)
		if (EventArgs.Key == EKeys::F1 && KeyState.IsShiftDown())
		{
			ImGuiFocusHandler::SetUIFocus();
		}
	}
#endif //#if WITH_EDITOR

	void TogglePrimaryImGuiContext(UWorld* World)
	{
		TSharedPtr<SImGuiMainMenuWidget> MainMenuWidget = m_PrimaryContextWidget.Pin();
		if (!MainMenuWidget)
		{
			if (UGameViewportClient* GameViewport = World->GetGameViewport())
			{
				MainMenuWidget = SNew(SImGuiMainMenuWidget)
					.MainViewportWindow(GameViewport->GetWindow());
				GameViewport->AddViewportWidgetContent(MainMenuWidget.ToSharedRef(), TNumericLimits<int32>::Max());
				m_PrimaryContextWidget = MainMenuWidget;

				ImGuiFocusHandler::SetUIFocus();
			}
			return;
		}

		if (MainMenuWidget)
		{
			if (MainMenuWidget->GetVisibility() == EVisibility::Visible)
			{
				MainMenuWidget->SetVisibility(EVisibility::Hidden);
				ImGuiFocusHandler::ResetFocus();
			}
			else
			{
				MainMenuWidget->SetVisibility(EVisibility::Visible);
				ImGuiFocusHandler::SetUIFocus();
			}
		}
	}

public:
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

	FImGuiMenuContainer& GetMenuContainer(const UWorld* World)
	{
#if WITH_EDITOR
		for (auto Itr = m_MenuContainers.CreateIterator(); Itr; ++Itr)
		{
			if (Itr->GetWorld() == World)
			{
				return *Itr;
			}
		}
		FImGuiMenuContainer NewContainer = {};
		NewContainer.SetWorld(World);
		return m_MenuContainers.Add_GetRef(MoveTemp(NewContainer));
#else
		return m_MenuContainer;
#endif
	}

public:
#if WITH_EDITOR
	static const FName ImGuiTabName;
	TSharedPtr<FWorkspaceItem> m_ImGuiTabGroup;

	TArray<FImGuiMenuContainer> m_MenuContainers;
	TArray<TWeakPtr<SImGuiMainMenuWidget>> m_PIEContextWidgets;
#else
	FImGuiMenuContainer m_MenuContainer;
#endif
	// Editor/Game context widget
	TWeakPtr<SImGuiMainMenuWidget> m_PrimaryContextWidget;

	TUniquePtr<FAutoConsoleCommandWithWorld> m_OpenImGuiMenuCommand = nullptr;
};

#if WITH_EDITOR
static FDelayedAutoRegisterHelper ImGuiViewportToolbar_DelayedAutoRegister(EDelayedRegisterRunPhase::EndOfEngineInit,
	[]()
	{
		if (CVarAddImGuiWidgetToLevelViewport.GetValueOnGameThread() == false || !GEditor)
		{
			return;
		}

#if USE_TOOLMENU_API
		static FImGuiRuntimeModule& ImGuiModule = FModuleManager::GetModuleChecked<FImGuiRuntimeModule>("ImGuiRuntime");
		FToolMenuOwnerScoped ScopedOwner(&ImGuiModule);

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

TSharedRef<FWorkspaceItem> GetImGuiTabGroup()
{
	static FImGuiRuntimeModule& ImGuiModule = FModuleManager::GetModuleChecked<FImGuiRuntimeModule>("ImGuiRuntime");
	return ImGuiModule.m_ImGuiTabGroup.ToSharedRef();
}

void SImGuiViewportToolBarButton::OnPressed()
{
	static FImGuiRuntimeModule& ImGuiModule = FModuleManager::GetModuleChecked<FImGuiRuntimeModule>("ImGuiRuntime");

	TSharedPtr<SImGuiMainMenuWidget> MainMenuWidget = ImGuiModule.m_PrimaryContextWidget.Pin();
	if (MainMenuWidget)
	{
		bRequestMenuOnClick = !MainMenuWidget->IsMenuOpen();
	}
	else
	{
		bRequestMenuOnClick = true;
	}
}

FReply SImGuiViewportToolBarButton::OnClicked()
{
	static FImGuiRuntimeModule& ImGuiModule = FModuleManager::GetModuleChecked<FImGuiRuntimeModule>("ImGuiRuntime");

	TSharedPtr<SLevelViewport> Viewport = LevelViewport.Pin();
	if (Viewport)
	{
		TSharedPtr<SWindow> ViewportWindow = FSlateApplication::Get().FindWidgetWindow(Viewport.ToSharedRef());

		TSharedPtr<SImGuiMainMenuWidget> MainMenuWidget = ImGuiModule.m_PrimaryContextWidget.Pin();
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
			ImGuiModule.m_PrimaryContextWidget = MainMenuWidget;
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
#endif //#if WITH_EDITOR

FImGuiMenuContainer& GetMenuContainerForWorld(const UWorld* World)
{
	static FImGuiRuntimeModule& ImGuiModule = FModuleManager::GetModuleChecked<FImGuiRuntimeModule>("ImGuiRuntime");
	return ImGuiModule.GetMenuContainer(World);
}

FImGuiTickContext* GetWidgetTickContextForWorld(const UWorld* World)
{
	static FImGuiRuntimeModule& ImGuiModule = FModuleManager::GetModuleChecked<FImGuiRuntimeModule>("ImGuiRuntime");
	return ImGuiModule.GetWidgetTickContext(World);
}

IMPLEMENT_MODULE(FImGuiRuntimeModule, ImGuiRuntime)

#undef LOCTEXT_NAMESPACE

#endif //#if WITH_ENGINE