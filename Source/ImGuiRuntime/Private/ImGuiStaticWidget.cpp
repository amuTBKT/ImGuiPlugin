// Copyright 2024 Amit Kumar Mehar. All Rights Reserved.

#pragma once

#include "ImGuiStaticWidget.h"

#include "SImGuiWidgets.h"
#include "ImGuiSubsystem.h"

#if WITH_EDITOR
#include "Widgets/Docking/SDockTab.h"
#include "Framework/Docking/TabManager.h"
#include "Editor/WorkspaceMenuStructure/Public/WorkspaceMenuStructure.h"
#include "Editor/WorkspaceMenuStructure/Public/WorkspaceMenuStructureModule.h"
#endif

FAutoRegisterMainWindowWidget::FAutoRegisterMainWindowWidget(void(*InitFunction)(void), void(*TickFunction)(ImGuiContext* Context))
{
	if (UImGuiSubsystem* Subsystem = UImGuiSubsystem::Get())
	{
		InitFunction();
		Subsystem->GetMainWindowTickDelegate().AddStatic(TickFunction);
	}
	else
	{
		UImGuiSubsystem::OnSubsystemInitialized().AddLambda(
			[InitFunction, TickFunction](UImGuiSubsystem* Subsystem)
			{
				InitFunction();
				Subsystem->GetMainWindowTickDelegate().AddStatic(TickFunction);
			});
	}
}

#if WITH_EDITOR

// defined in ImGuiRuntimeModule.cpp
extern TSharedRef<FWorkspaceItem> GetImGuiTabGroup();

static TSharedRef<SDockTab> SpawnWidgetTab(const FSpawnTabArgs& SpawnTabArgs, FAutoRegisterStandaloneWidget::FParams RegisterParams)
{
	FOnTickImGuiWidgetDelegate TickDelegate;
	TickDelegate.BindStatic(RegisterParams.TickFunction);

	TSharedPtr<SImGuiWidget> ImGuiWindow =
		SNew(SImGuiWidget)
		.OnTickDelegate(TickDelegate);
	
	const TSharedRef<SDockTab> ImguiTab =
		SNew(SDockTab)
		.TabRole(ETabRole::NomadTab);
	ImguiTab->SetTabIcon(RegisterParams.TabIcon.GetIcon());
	ImguiTab->SetContent(ImGuiWindow.ToSharedRef());

	return ImguiTab;
}

FAutoRegisterStandaloneWidget::FAutoRegisterStandaloneWidget(FAutoRegisterStandaloneWidget::FParams RegisterParams)
{
	if (!ensure(RegisterParams.IsValid()))
	{
		return;
	}

	if (UImGuiSubsystem* Subsystem = UImGuiSubsystem::Get())
	{
		RegisterParams.InitFunction();		
		FGlobalTabmanager::Get()->RegisterNomadTabSpawner(FName(RegisterParams.TabName), FOnSpawnTab::CreateStatic(&SpawnWidgetTab, RegisterParams))
			.SetGroup(GetImGuiTabGroup())
			.SetDisplayName(FText::FromString(ANSI_TO_TCHAR(RegisterParams.TabName)))
			.SetTooltipText(FText::FromString(ANSI_TO_TCHAR(RegisterParams.TabTooltip)))
			.SetIcon(RegisterParams.TabIcon);
	}
	else
	{
		UImGuiSubsystem::OnSubsystemInitialized().AddLambda(
			[RegisterParams](UImGuiSubsystem* Subsystem)
			{
				RegisterParams.InitFunction();
				FGlobalTabmanager::Get()->RegisterNomadTabSpawner(FName(RegisterParams.TabName), FOnSpawnTab::CreateStatic(&SpawnWidgetTab, RegisterParams))
					.SetGroup(GetImGuiTabGroup())
					.SetDisplayName(FText::FromString(ANSI_TO_TCHAR(RegisterParams.TabName)))
					.SetTooltipText(FText::FromString(ANSI_TO_TCHAR(RegisterParams.TabTooltip)))
					.SetIcon(RegisterParams.TabIcon);
			});
	}
}

#endif //#if WITH_EDITOR
