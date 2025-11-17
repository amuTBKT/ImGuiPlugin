// Copyright 2024-25 Amit Kumar Mehar. All Rights Reserved.

#include "SImGuiWidgets.h"
#include "ImGuiSubsystem.h"

#if WITH_EDITOR
#include "Widgets/Docking/SDockTab.h"
#include "Framework/Docking/TabManager.h"
#include "Editor/WorkspaceMenuStructure/Public/WorkspaceMenuStructure.h"
#include "Editor/WorkspaceMenuStructure/Public/WorkspaceMenuStructureModule.h"
#endif

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

// defined in ImGuiRuntimeModule.cpp
extern TSharedRef<FWorkspaceItem> GetImGuiTabGroup();

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
			.bUseOpaqueBackground(true)
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
