// Copyright 2024 Amit Kumar Mehar. All Rights Reserved.

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
		FTabSpawnerEntry& SpawnerEntry = FGlobalTabmanager::Get()->RegisterNomadTabSpawner(ImGuiTabName, FOnSpawnTab::CreateStatic(&FImGuiRuntimeModule::SpawnImGuiTab))
			.SetDisplayName(LOCTEXT("ImGuiTabTitle", "ImGui"))
			.SetTooltipText(LOCTEXT("ImGuiTabToolTip", "ImGui UI"))
			.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Layout"))
			.SetGroup(WorkspaceMenu::GetMenuStructure().GetToolsCategory());
#endif

		// console command makes sense for Game worlds, for editor we should use ImGuiTab
		FWorldDelegates::OnPreWorldInitialization.AddLambda(
			[this](UWorld* World, const UWorld::InitializationValues IVS)
			{
				m_OpenImGuiWindowCommand.Reset();

				if (World->WorldType == EWorldType::Game)
				{
					m_OpenImGuiWindowCommand = MakeUnique<FAutoConsoleCommand>(
						TEXT("imgui.OpenWindow"),
						TEXT("Opens ImGui window."),
						FConsoleCommandDelegate::CreateRaw(this, &FImGuiRuntimeModule::OpenImGuiMainWindow));
				}
			}
		);
	}

	virtual void ShutdownModule() override
	{
#if WITH_EDITOR
		if (FSlateApplication::IsInitialized())
		{
			FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(ImGuiTabName);
		}
#endif

		m_OpenImGuiWindowCommand = nullptr;
		m_ImGuiMainWindow = nullptr;
	}

#if WITH_EDITOR
	static TSharedRef<SDockTab> SpawnImGuiTab(const FSpawnTabArgs& SpawnTabArgs)
	{
		const TSharedRef<SDockTab> ImguiTab =
			SNew(SDockTab)
			.TabRole(ETabRole::NomadTab);
		ImguiTab->SetTabIcon(FAppStyle::GetBrush("Icons.Layout"));
		ImguiTab->SetContent(SNew(SImGuiMainWindowWidget));

		return ImguiTab;
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
			m_ImGuiMainWindow->SetContent(SNew(SImGuiMainWindowWidget));

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

private:
#if WITH_EDITOR
	static const FName ImGuiTabName;
#endif

	TUniquePtr<FAutoConsoleCommand> m_OpenImGuiWindowCommand = nullptr;
	TSharedPtr<SWindow> m_ImGuiMainWindow = nullptr;
};

#if WITH_EDITOR
const FName FImGuiRuntimeModule::ImGuiTabName = TEXT("ImGuiTab");
#endif

IMPLEMENT_MODULE(FImGuiRuntimeModule, ImGuiRuntime)

#undef LOCTEXT_NAMESPACE
