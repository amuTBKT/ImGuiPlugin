// Copyright 2024-25 Amit Kumar Mehar. All Rights Reserved.

#include "Modules/ModuleManager.h"
#include "Modules/ModuleInterface.h"

#include "Engine/World.h"
#include "SImGuiWidgets.h"
#include "ImGuiSubsystem.h"
#include "HAL/IConsoleManager.h"
#include "Framework/Application/SlateApplication.h"

#if WITH_EDITOR
#include "EditorStyleSet.h"
#include "Widgets/Docking/SDockTab.h"
#include "Framework/Docking/TabManager.h"
#include "Editor/WorkspaceMenuStructure/Public/WorkspaceMenuStructure.h"
#include "Editor/WorkspaceMenuStructure/Public/WorkspaceMenuStructureModule.h"
#endif

#define LOCTEXT_NAMESPACE "ImGuiPlugin"

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
				.ClientSize(FVector2D(640, 640))
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
