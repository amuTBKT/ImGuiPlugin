// Copyright 2024-25 Amit Kumar Mehar. All Rights Reserved.

#include "Modules/ModuleManager.h"
#include "Modules/ModuleInterface.h"

#include "Engine/World.h"
#include "SImGuiWidgets.h"
#include "ImGuiSubsystem.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformFileManager.h"
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

FAutoRegisterMainWindowWidget::FAutoRegisterMainWindowWidget(FStaticWidgetRegisterParams RegisterParams)
{
	if (!ensureAlways(RegisterParams.IsValid()))
	{
		return;
	}

	if (UImGuiSubsystem* ImGuiSubsystem = UImGuiSubsystem::Get())
	{
		RegisterParams.InitFunction();
		ImGuiSubsystem->GetMainWindowTickDelegate().AddStatic(RegisterParams.TickFunction);
	}
	else
	{
		UImGuiSubsystem::OnSubsystemInitialized().AddLambda(
			[RegisterParams](UImGuiSubsystem* ImGuiSubsystem)
			{
				RegisterParams.InitFunction();
				ImGuiSubsystem->GetMainWindowTickDelegate().AddStatic(RegisterParams.TickFunction);
			});
	}
}

#if WITH_EDITOR
TSharedRef<FWorkspaceItem> GetImGuiTabGroup();

static TSharedRef<SDockTab> SpawnWidgetTab(const FSpawnTabArgs& SpawnTabArgs, FStaticWidgetRegisterParams RegisterParams)
{
	FOnTickImGuiWidgetDelegate TickDelegate;
	TickDelegate.BindStatic(RegisterParams.TickFunction);

	return SNew(SDockTab)
		.TabRole(ETabRole::NomadTab)
		[
			SNew(SImGuiWidget)
			.MainViewportWindow(SpawnTabArgs.GetOwnerWindow())
			.OnTickDelegate(TickDelegate)
			.ConfigFileName(RegisterParams.WidgetName)
			.bEnableViewports(RegisterParams.bEnableViewports)
		];
}

FAutoRegisterStandaloneWidget::FAutoRegisterStandaloneWidget(FStaticWidgetRegisterParams RegisterParams)
{
	if (!ensureAlways(RegisterParams.IsValid()))
	{
		return;
	}

	if (UImGuiSubsystem* ImGuiSubsystem = UImGuiSubsystem::Get())
	{
		RegisterParams.InitFunction();
		FGlobalTabmanager::Get()->RegisterNomadTabSpawner(FName(RegisterParams.WidgetName), FOnSpawnTab::CreateStatic(&SpawnWidgetTab, RegisterParams))
			.SetGroup(GetImGuiTabGroup())
			.SetDisplayName(FText::FromString(ANSI_TO_TCHAR(RegisterParams.WidgetName)))
			.SetTooltipText(FText::FromString(ANSI_TO_TCHAR(RegisterParams.WidgetDescription)))
			.SetIcon(RegisterParams.WidgetIcon);
	}
	else
	{
		UImGuiSubsystem::OnSubsystemInitialized().AddLambda(
			[RegisterParams](UImGuiSubsystem* ImGuiSubsystem)
			{
				RegisterParams.InitFunction();
				FGlobalTabmanager::Get()->RegisterNomadTabSpawner(FName(RegisterParams.WidgetName), FOnSpawnTab::CreateStatic(&SpawnWidgetTab, RegisterParams))
					.SetGroup(GetImGuiTabGroup())
					.SetDisplayName(FText::FromString(ANSI_TO_TCHAR(RegisterParams.WidgetName)))
					.SetTooltipText(FText::FromString(ANSI_TO_TCHAR(RegisterParams.WidgetDescription)))
					.SetIcon(RegisterParams.WidgetIcon);
			});
	}
}
#endif //#if WITH_EDITOR

///////////////////////////////////////////////////////////////////////////////////////////////////////

/* Main window widget, only one instance active at a time */
class SImGuiMainWindowWidget : public SImGuiWidgetBase
{
	using Super = SImGuiWidgetBase;
public:
	SLATE_BEGIN_ARGS(SImGuiMainWindowWidget)
		: _MainViewportWindow(nullptr)
		{
		}
		SLATE_ARGUMENT(TSharedPtr<SWindow>, MainViewportWindow);
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs)
	{
		Super::Construct(
			Super::FArguments()
			.MainViewportWindow(InArgs._MainViewportWindow)
			.ConfigFileName("ImGui"));
	}

private:
	virtual void TickImGuiInternal(FImGuiTickContext* TickContext) override
	{
		UImGuiSubsystem* ImGuiSubsystem = UImGuiSubsystem::Get();
		if (ImGuiSubsystem->GetMainWindowTickDelegate().IsBound())
		{
			ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport());
			ImGuiSubsystem->GetMainWindowTickDelegate().Broadcast(TickContext);
		}
		else
		{
			const ImGuiID MainDockSpaceID = ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport());
			ImGui::SetNextWindowDockID(MainDockSpaceID);
			if (ImGui::Begin("Empty", nullptr))
			{
				ImGui::Text("Nothing to display...");
			}
			ImGui::End();
		}
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
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Layout"));

		FGlobalTabmanager::Get()->RegisterNomadTabSpawner(ImGuiTabName, FOnSpawnTab::CreateStatic(&FImGuiRuntimeModule::SpawnImGuiTab))
			.SetGroup(m_ImGuiTabGroup.ToSharedRef())
			.SetDisplayName(LOCTEXT("ImGuiMainTabTitle", "ImGui"))
			.SetTooltipText(LOCTEXT("ImGuiMainTabTooltip", "Window hosting static ImGui widgets"))
			.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Layout"));
#endif

		if (!GIsEditor)
		{
			// game world uses console command instead of tabs
			m_OpenImGuiWindowCommand = MakeUnique<FAutoConsoleCommand>(
				TEXT("imgui.OpenWindow"),
				TEXT("Opens ImGui window."),
				FConsoleCommandDelegate::CreateRaw(this, &FImGuiRuntimeModule::OpenImGuiMainWindow));
		}
	}

	virtual void ShutdownModule() override
	{
#if WITH_EDITOR
		if (FSlateApplication::IsInitialized())
		{
			FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(ImGuiTabName);
		}
#endif

		m_ImGuiMainWindow.Reset();
		m_OpenImGuiWindowCommand.Reset();
	}

#if WITH_EDITOR
	static TSharedRef<SDockTab> SpawnImGuiTab(const FSpawnTabArgs& SpawnTabArgs)
	{
		return SNew(SDockTab)
			.TabRole(ETabRole::NomadTab)
			[
				SNew(SImGuiMainWindowWidget)
				.MainViewportWindow(SpawnTabArgs.GetOwnerWindow())
			];
	}
#endif
	
	void OpenImGuiMainWindow()
	{
		if (!m_ImGuiMainWindow.IsValid())
		{
			m_ImGuiMainWindow = SNew(SWindow)
				.Title(LOCTEXT("ImGuiWindowTitle", "ImGui"))
				.ClientSize(FVector2f(512.f, 512.f))
				.AutoCenter(EAutoCenter::None)
				.SupportsMaximize(true)
				.SupportsMinimize(true)
				.UseOSWindowBorder(false)
				//.LayoutBorder(FMargin(0.f))
				//.UserResizeBorder(FMargin(0.f))
				.SizingRule(ESizingRule::UserSized);
			m_ImGuiMainWindow = FSlateApplication::Get().AddWindow(m_ImGuiMainWindow.ToSharedRef());
			m_ImGuiMainWindow->SetContent(SNew(SImGuiMainWindowWidget).MainViewportWindow(m_ImGuiMainWindow));

			m_ImGuiMainWindow->GetOnWindowClosedEvent().AddLambda(
				[this](const TSharedRef<SWindow>& Window)
				{
					m_ImGuiMainWindow = nullptr;
				}
			);
		}
		else
		{
			UE_LOG(LogTemp, Log, TEXT("Only one instance of ImGui window is supported"))
		}
	}

public:
#if WITH_EDITOR
	static const FName ImGuiTabName;

	// tab group for adding static widgets
	TSharedPtr<FWorkspaceItem> m_ImGuiTabGroup;
#endif

	TSharedPtr<SWindow> m_ImGuiMainWindow = nullptr;
	TUniquePtr<FAutoConsoleCommand> m_OpenImGuiWindowCommand = nullptr;
};

#if WITH_EDITOR
const FName FImGuiRuntimeModule::ImGuiTabName = TEXT("ImGuiTab");

TSharedRef<FWorkspaceItem> GetImGuiTabGroup()
{
	FImGuiRuntimeModule& ImGuiModule = FModuleManager::GetModuleChecked<FImGuiRuntimeModule>("ImGuiRuntime");
	return ImGuiModule.m_ImGuiTabGroup.ToSharedRef();
}
#endif

IMPLEMENT_MODULE(FImGuiRuntimeModule, ImGuiRuntime)

#undef LOCTEXT_NAMESPACE
