// Copyright 2024 Amit Kumar Mehar. All Rights Reserved.

#include "Modules/ModuleManager.h"
#include "Modules/ModuleInterface.h"

#include "Engine/World.h"
#include "SImGuiWidgets.h"
#include "ImGuiSubsystem.h"
#include "HAL/IConsoleManager.h"
#include "Interfaces/IPluginManager.h"
#include "Framework/Application/SlateApplication.h"

#if WITH_EDITOR
#include "EditorStyleSet.h"
#include "Widgets/Docking/SDockTab.h"
#include "Framework/Docking/TabManager.h"
#include "Editor/WorkspaceMenuStructure/Public/WorkspaceMenuStructure.h"
#include "Editor/WorkspaceMenuStructure/Public/WorkspaceMenuStructureModule.h"
#endif

#include "Styling/AppStyle.h"
#include "Styling/SlateStyle.h"
#include "Styling/SlateStyleMacros.h"
#include "Styling/SlateStyleRegistry.h"

#define LOCTEXT_NAMESPACE "ImGuiPlugin"

class FImGuiStyleSet final : public FSlateStyleSet
{
public:
	FImGuiStyleSet()
		: FSlateStyleSet("ImGuiStyle")
	{
		SetParentStyleName(FAppStyle::GetAppStyleSetName());

		const FString IconDirectory = FPaths::Combine(IPluginManager::Get().FindPlugin(TEXT("ImGuiPlugin"))->GetBaseDir(), TEXT("Content/Editor/Slate"));
		SetContentRoot(IconDirectory);
		SetCoreContentRoot(FPaths::EngineContentDir() / TEXT("Slate"));

		const FVector2D Icon16x16 = FVector2D(16.0, 16.0);

		Set("Icon.EngineFolder", new IMAGE_BRUSH_SVG("Icons/icon_engine_content", Icon16x16));
		Set("Icon.ProjectFolder", new IMAGE_BRUSH_SVG("Icons/icon_project_content", Icon16x16));
		Set("Icon.PluginFolder", new IMAGE_BRUSH_SVG("Icons/icon_plugin_content", Icon16x16));
		Set("Icon.DeveloperFolder", new IMAGE_BRUSH_SVG("Icons/icon_developer_content", Icon16x16));
		Set("Icon.AssetCollection", new IMAGE_BRUSH_SVG("Icons/icon_collection_content", Icon16x16));
		Set("Icon.LocalizedFolder", new IMAGE_BRUSH_SVG("Icons/icon_localized_content", Icon16x16));
		Set("Icon.DropDownArrow", new IMAGE_BRUSH_SVG("Icons/icon_dropdown_arrow", Icon16x16));
		Set("Icon.UseSelectedAsset", new IMAGE_BRUSH_SVG("Icons/icon_use_selected_asset", Icon16x16));
		Set("Icon.ResetToDefault", new IMAGE_BRUSH_SVG("Icons/icon_reset_to_default", Icon16x16));
		Set("Icon.BrowseToAsset", new IMAGE_BRUSH_SVG("Icons/icon_browse_to_asset", Icon16x16));
		Set("Icon.FallbackAssetIcon", new IMAGE_BRUSH_SVG("Icons/icon_fallback_asset_icon", Icon16x16));
		Set("Icon.CheckerPattern", new IMAGE_BRUSH_SVG("Icons/icon_checker_pattern", Icon16x16));
		Set("Icon.Find", new IMAGE_BRUSH_SVG("Icons/icon_find", Icon16x16));
		Set("Icon.FrameSelected", new IMAGE_BRUSH_SVG("Icons/icon_frame_selected", Icon16x16));

		FSlateStyleRegistry::RegisterSlateStyle(*this);
	}

	~FImGuiStyleSet()
	{
		FSlateStyleRegistry::UnRegisterSlateStyle(*this);
	}
};

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

		m_ImGuiStyleSet = MakeUnique<FImGuiStyleSet>();
	}

	virtual void ShutdownModule() override
	{
#if WITH_EDITOR
		if (FSlateApplication::IsInitialized())
		{
			FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(ImGuiTabName);
		}
#endif

		m_ImGuiStyleSet.Reset();
		m_ImGuiMainWindow.Reset();
		m_OpenImGuiWindowCommand.Reset();
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

public:
#if WITH_EDITOR
	static const FName ImGuiTabName;

	// tab group for adding static widgets
	TSharedPtr<FWorkspaceItem> m_ImGuiTabGroup;
#endif

	TSharedPtr<SWindow> m_ImGuiMainWindow = nullptr;
	TUniquePtr<FImGuiStyleSet> m_ImGuiStyleSet = nullptr;
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
