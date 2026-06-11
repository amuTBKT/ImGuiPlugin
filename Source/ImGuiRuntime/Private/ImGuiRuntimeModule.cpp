// Copyright 2024-26 Amit Kumar Mehar. All Rights Reserved.

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
#include "HAL/PlatformFileManager.h"
#include "Engine/GameViewportClient.h"
#include "Framework/Application/SlateApplication.h"

#if WITH_EDITOR
#include "EditorStyleSet.h"
#include "Widgets/Docking/SDockTab.h"
#include "Framework/Docking/TabManager.h"
#include "Editor/WorkspaceMenuStructure/Public/WorkspaceMenuStructure.h"
#include "Editor/WorkspaceMenuStructure/Public/WorkspaceMenuStructureModule.h"
#endif

#define LOCTEXT_NAMESPACE "ImGuiPlugin"

///////////////////////////////////////////////////////////////////////////////////////////////////////

#ifdef WITH_IMGUI_STATIC_LIB
void ImGuiAssertHook(bool bCondition, const char* Expression, const char* File, uint32 Line)
{
	if (bCondition)
	{
		return;
	}

	static TSet<uint32> EncounteredAsserts;
	uint32 Key = HashCombine(PointerHash(File), GetTypeHash(Line));
	if (!EncounteredAsserts.Contains(Key)) // fire once, similar to `ensure(expr)`
	{
		EncounteredAsserts.Add(Key);

		FPlatformMisc::LowLevelOutputDebugString(*FString::Printf(TEXT("Ensure condition failed: %hs [File: %hs] [Line: %u]\n"), Expression, File, Line));
		if (FPlatformMisc::IsDebuggerPresent())
		{
			PLATFORM_BREAK();
		}
	}
}
#else
#include "ImGuiLib.cpp"
#endif

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

	static FWidgetSlot* AddSlotSorted(TArray<FWidgetSlot>& Container, FWidgetSlot&& Slot)
	{
		const int32 InsertIndex = Algo::LowerBound(Container, Slot);
		Container.Insert(MoveTemp(Slot), InsertIndex);
		return &Container[InsertIndex];
	}

	FWidgetSlot* FindSlot(FAnsiStringView Path)
	{
		FWidgetSlot::FPathIterator PathItr{ Path };

		FAnsiStringView SubPath = ++PathItr;
		FWidgetSlot* ParentSlot = SubPath.IsEmpty() ? nullptr : WidgetSlots.FindByKey(SubPath);
		if (!ParentSlot)
		{
			return nullptr;
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
				break;
			}
			else if (NextSlot->IsMenuItem())
			{
				ensureAlwaysMsgf(false, TEXT("Path points to an active menu item!"));
				ParentSlot = nullptr;
				break;
			}
			ParentSlot = NextSlot;
		}

		return ParentSlot;
	}
	FWidgetSlot* InitializeSlotHierarchy(FAnsiStringView Path)
	{
		FWidgetSlot::FPathIterator PathItr{ Path };

		FAnsiStringView SubPath = ++PathItr;
		FWidgetSlot* ParentSlot = SubPath.IsEmpty() ? nullptr : WidgetSlots.FindByKey(SubPath);
		if (!ParentSlot)
		{
			ParentSlot = AddSlotSorted(WidgetSlots, FWidgetSlot(FAnsiString(SubPath)));
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
				NextSlot = AddSlotSorted(ParentSlot->GetChildren(), FWidgetSlot(FAnsiString(SubPath)));
			}
			else if (NextSlot->IsMenuItem())
			{
				ensureAlwaysMsgf(false, TEXT("Path points to an active menu item!"));
				ParentSlot = nullptr;
				break;
			}
			ParentSlot = NextSlot;
		}

		return ParentSlot;
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
		if (QueueWidgetSlotChanges)
		{
			WidgetSlotsToAdd.Emplace(WidgetPath, WidgetToolTip, WidgetIcon, TickDelegate, WidgetFlags);
			WidgetSlotsToRemove.Remove(WidgetSlotsToAdd.Last());
			return;
		}

		FWidgetSlot* ParentSlot = InitializeSlotHierarchy(WidgetPath);
		if (ensureAlways(ParentSlot))
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
	}
	void UnregisterWidget(const char* WidgetPath)
	{
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
	FImGuiMenuContainer::FWidgetSlot* Slot = ParentSlot ? ParentSlot->GetChildren().FindByKey(WidgetPath) : nullptr;
	return Slot ? &Slot->bIsActive : nullptr;
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
			.SetDisplayName(FText::FromString(ANSI_TO_TCHAR(RegisterParams.GetWidetName())))
			.SetTooltipText(FText::FromString(ANSI_TO_TCHAR(RegisterParams.WidgetDescription)))
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
					.SetDisplayName(FText::FromString(ANSI_TO_TCHAR(RegisterParams.GetWidetName())))
					.SetTooltipText(FText::FromString(ANSI_TO_TCHAR(RegisterParams.WidgetDescription)))
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
		OwningWorld = InArgs._OwningWorld;

		UImGuiSubsystem::OnBeginImGuiFrame.AddRaw(this, &SImGuiMainMenuWidget::BeginFrame);
		UImGuiSubsystem::OnEndImGuiFrame.AddRaw(this, &SImGuiMainMenuWidget::EndFrame);
	}

	~SImGuiMainMenuWidget()
	{
		UImGuiSubsystem::OnBeginImGuiFrame.RemoveAll(this);
		UImGuiSubsystem::OnEndImGuiFrame.RemoveAll(this);
	}

	const UWorld* GetWorld() const { return OwningWorld.Get(); }

private:
	void BeginFrame()
	{
		FImGuiTickScope Scope{ GetTickContext() };

		BeginImGuiFrame(GetCachedGeometry());

		if (!bIsDockNodeValid)
		{
			bIsDockNodeValid = true;
			const bool bIsEditorWorld = GIsEditor && !GetWorld();
			ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport(), bIsEditorWorld ? ImGuiDockNodeFlags_None : ImGuiDockNodeFlags_PassthruCentralNode);
		}
	}

	void EndFrame()
	{
		FImGuiTickScope Scope{ GetTickContext() };

		EndImGuiFrame();
		bIsDockNodeValid = false;
	}

private:
	TWeakObjectPtr<const UWorld> OwningWorld;

	// cached during tick for easier access
	bool bIsDockNodeValid = false;
	UImGuiSubsystem* ImGuiSubsystem = nullptr;
	FImGuiImageBindingParams ExpandedMenuIcon{};
	FImGuiImageBindingParams CollapsedMenuIcon{};

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
				FImGuiImageBindingParams Icon = Slot.Icon ? ImGuiSubsystem->RegisterOneFrameResource(Slot.Icon, ImGui::GetTextLineHeight()) : FImGuiImageBindingParams{};
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
			FImGui::SubMenu(Slot.GetName(), ExpandedMenuIcon, CollapsedMenuIcon,
				[&]()
				{
					for (auto& ChildSlot : Slot.GetChildren())
					{
						TickMainMenuBar(MenuContainer, ChildSlot, TickContext);
					}
				});
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
		ImGuiSubsystem = UImGuiSubsystem::Get();
		FImGuiMenuContainer& MenuContainer = GetMenuContainerForWorld(GetWorld());
		TArray<FImGuiMenuContainer::FWidgetSlot>& Slots = MenuContainer.StartIterationOverMainMenuWidgetSlots();
		ON_SCOPE_EXIT
		{
			MenuContainer.EndIterationOverMainMenuWidgetSlots();
			ImGuiSubsystem = nullptr;
		};

		if (Slots.IsEmpty())
		{
			return;
		}

		FImGuiTickScope Scope{ TickContext };

		ExpandedMenuIcon = ImGuiSubsystem->RegisterOneFrameResource(IMGUI_STYLE_ICON_BRUSH("CoreStyle", "Icons.FolderOpen"), ImGui::GetTextLineHeight());
		CollapsedMenuIcon = ImGuiSubsystem->RegisterOneFrameResource(IMGUI_STYLE_ICON_BRUSH("CoreStyle", "Icons.FolderClosed"), ImGui::GetTextLineHeight());

		if (!bIsDockNodeValid)
		{
			const bool bIsEditorWorld = GIsEditor && !GetWorld();
			ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport(), bIsEditorWorld ? ImGuiDockNodeFlags_None : ImGuiDockNodeFlags_PassthruCentralNode);
		}

		if (ImGui::BeginMainMenuBar())
		{
			TickContext->bIsTickingMainMenuBar = true;

			for (FImGuiMenuContainer::FWidgetSlot& Slot : Slots)
			{
				if (!Slot.GetChildren().IsEmpty() && ImGui::BeginMenu(Slot.GetName()))
				{
					for (auto& ChildSlot : Slot.GetChildren())
					{
						TickMainMenuBar(MenuContainer, ChildSlot, TickContext);
					}
					ImGui::EndMenu();
				}
			}
			ImGui::EndMainMenuBar();

			TickContext->bIsTickingMainMenuBar = false;
		}

		for (FImGuiMenuContainer::FWidgetSlot& Slot : Slots)
		{
			TickMainMenuWidgets(MenuContainer, Slot, TickContext);
		}

		// we are done with the dock node
		// if we enter `TickImGuiInternal` again that would mean the event is coming from window resizing
		// so will have to add the dock node again!
		bIsDockNodeValid = false;
	}
};

///////////////////////////////////////////////////////////////////////////////////////////////////////

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
		SETUP_DEFAULT_IMGUI_ALLOCATOR();

#if WITH_EDITOR
		m_ImGuiTabGroup = WorkspaceMenu::GetMenuStructure().GetToolsCategory()->AddGroup(
			LOCTEXT("ImGuiGroupName", "ImGui"),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Layout"), /*bSortChildren=*/true);

		FGlobalTabmanager::Get()->RegisterNomadTabSpawner(ImGuiTabName, FOnSpawnTab::CreateRaw(this, &FImGuiRuntimeModule::SpawnImGuiTab))
			.SetGroup(m_ImGuiTabGroup.ToSharedRef())
			.SetDisplayName(LOCTEXT("ImGuiMainTabTitle", "ImGui"))
			.SetTooltipText(LOCTEXT("ImGuiMainTabTooltip", "Window hosting static ImGui widgets"))
			.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Layout"));

		FWorldDelegates::OnWorldBeginTearDown.AddRaw(this, &FImGuiRuntimeModule::OnWorldBeginTearDown);
#else
		UGameViewportClient::OnViewportCreated().AddRaw(this, &FImGuiRuntimeModule::OnViewportCreated);
#endif

		m_OpenImGuiMenuCommand = MakeUnique<FAutoConsoleCommandWithWorld>(
			TEXT("imgui.ToggleMenu"),
			TEXT("Toggles ImGui menu."),
			FConsoleCommandWithWorldDelegate::CreateRaw(this, &FImGuiRuntimeModule::ToggleImGuiMainMenu));
	}

	virtual void ShutdownModule() override
	{
#if WITH_EDITOR
		if (FSlateApplication::IsInitialized())
		{
			FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(ImGuiTabName);
		}
		m_MainMenuWidgets.Reset();
		m_EditorMainMenuWidget.Reset();
#else
		m_MainMenuWidget.Reset();
#endif

		UGameViewportClient::OnViewportCreated().RemoveAll(this);

		m_OpenImGuiMenuCommand.Reset();
	}

#if WITH_EDITOR
	TSharedRef<SDockTab> SpawnImGuiTab(const FSpawnTabArgs& SpawnTabArgs)
	{
		TSharedPtr<SImGuiMainMenuWidget> MainMenuWidget = SNew(SImGuiMainMenuWidget)
			.MainViewportWindow(SpawnTabArgs.GetOwnerWindow());
		m_EditorMainMenuWidget = MainMenuWidget;

		return SNew(SDockTab)
			.TabRole(ETabRole::NomadTab)
			[
				MainMenuWidget.ToSharedRef()
			];
	}

	void OnWorldBeginTearDown(UWorld* World)
	{
		for (auto Itr = m_MainMenuWidgets.CreateIterator(); Itr; ++Itr)
		{
			TSharedPtr<SImGuiMainMenuWidget> Widget = Itr->Pin();
			const UWorld* WidgetWorld = Widget.IsValid() ? Widget->GetWorld() : nullptr;
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

	void ToggleImGuiMainMenu(UWorld* World)
	{
		if (!World->IsGameWorld())
		{
			return;
		}

		int32 MainMenuWidgetIndex = INDEX_NONE;
		TSharedPtr<SImGuiMainMenuWidget> MainMenuWidget;
		for (int32 WidgetIndex = 0; WidgetIndex < m_MainMenuWidgets.Num(); ++WidgetIndex)
		{
			MainMenuWidget = m_MainMenuWidgets[WidgetIndex].Pin();
			if (MainMenuWidget.IsValid() && MainMenuWidget->GetWorld() == World)
			{
				MainMenuWidgetIndex = WidgetIndex;
				break;
			}
			MainMenuWidget.Reset();
		}

		if (!MainMenuWidget.IsValid())
		{
			UGameViewportClient* GameViewport = World->bIsTearingDown ? nullptr : World->GetGameViewport();
			if (GameViewport)
			{
				MainMenuWidget = SNew(SImGuiMainMenuWidget)
					.MainViewportWindow(GameViewport->GetWindow())
					.OwningWorld(World);
				GameViewport->AddViewportWidgetContent(MainMenuWidget.ToSharedRef(), TNumericLimits<int32>::Max());
				m_MainMenuWidgets.Add(MainMenuWidget);

				// same logic as Shift+F1 , need to run on next tick as console will refocus the game viewport
				World->GetTimerManager().SetTimerForNextTick(
					[]()
					{
						FSlateApplication::Get().ClearKeyboardFocus(EFocusCause::SetDirectly);
						FSlateApplication::Get().ResetToDefaultInputSettings();
					});
			}
		}
		else
		{
			if (UGameViewportClient* GameViewport = World->GetGameViewport())
			{
				GameViewport->RemoveViewportWidgetContent(MainMenuWidget.ToSharedRef());

				FSlateApplication::Get().SetAllUserFocusToGameViewport(EFocusCause::SetDirectly);
			}
			if (MainMenuWidgetIndex != INDEX_NONE)
			{
				m_MainMenuWidgets.RemoveAt(MainMenuWidgetIndex);
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
			FSlateApplication::Get().ClearKeyboardFocus(EFocusCause::SetDirectly);
			FSlateApplication::Get().ResetToDefaultInputSettings();
		}
	}

	void ToggleImGuiMainMenu(UWorld* World)
	{
		TSharedPtr<SImGuiMainMenuWidget> MainMenuWidget = m_MainMenuWidget.Pin();
		
		if (!MainMenuWidget.IsValid())
		{
			if (UGameViewportClient* GameViewport = World->GetGameViewport())
			{
				MainMenuWidget = SNew(SImGuiMainMenuWidget)
					.MainViewportWindow(GameViewport->GetWindow());
				GameViewport->AddViewportWidgetContent(MainMenuWidget.ToSharedRef(), TNumericLimits<int32>::Max());
				m_MainMenuWidget = MainMenuWidget;

				// same logic as Shift+F1 , need to run on next tick as console will refocus the game viewport
				World->GetTimerManager().SetTimerForNextTick(
					[]()
					{
						FSlateApplication::Get().ClearKeyboardFocus(EFocusCause::SetDirectly);
						FSlateApplication::Get().ResetToDefaultInputSettings();
					});
			}
		}
		else
		{
			if (UGameViewportClient* GameViewport = World->GetGameViewport())
			{
				GameViewport->RemoveViewportWidgetContent(MainMenuWidget.ToSharedRef());
				FSlateApplication::Get().SetAllUserFocusToGameViewport(EFocusCause::SetDirectly);
			}
			m_MainMenuWidget.Reset();
		}
	}
#endif //#if WITH_EDITOR

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
			TSharedPtr<SImGuiMainMenuWidget> MainMenuWidget = m_EditorMainMenuWidget.Pin();
			if (MainMenuWidget.IsValid())
			{
				return GetTickContextFromWidget(MainMenuWidget.Get());
			}
		}
		else
		{
			for (int32 WidgetIndex = 0; WidgetIndex < m_MainMenuWidgets.Num(); ++WidgetIndex)
			{
				TSharedPtr<SImGuiMainMenuWidget> MainMenuWidget = m_MainMenuWidgets[WidgetIndex].Pin();
				if (MainMenuWidget.IsValid() && MainMenuWidget->GetWorld() == World)
				{
					return GetTickContextFromWidget(MainMenuWidget.Get());
				}
			}
		}
#else
		TSharedPtr<SImGuiMainMenuWidget> MainMenuWidget = m_MainMenuWidget.Pin();
		if (MainMenuWidget.IsValid())
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
	TWeakPtr<SImGuiMainMenuWidget> m_EditorMainMenuWidget;
	TArray<TWeakPtr<SImGuiMainMenuWidget>> m_MainMenuWidgets;
#else
	FImGuiMenuContainer m_MenuContainer;
	TWeakPtr<SImGuiMainMenuWidget> m_MainMenuWidget;
#endif

	TUniquePtr<FAutoConsoleCommandWithWorld> m_OpenImGuiMenuCommand = nullptr;
};

#if WITH_EDITOR
const FName FImGuiRuntimeModule::ImGuiTabName = TEXT("ImGuiTab");

TSharedRef<FWorkspaceItem> GetImGuiTabGroup()
{
	static FImGuiRuntimeModule& ImGuiModule = FModuleManager::GetModuleChecked<FImGuiRuntimeModule>("ImGuiRuntime");
	return ImGuiModule.m_ImGuiTabGroup.ToSharedRef();
}
#endif

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
