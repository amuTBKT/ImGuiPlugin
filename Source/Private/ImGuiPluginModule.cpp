#include "ImGuiPluginModule.h"

#if WITH_EDITOR
#include "EditorStyleSet.h"
#include "Widgets/Docking/SDockTab.h"
#include "Editor/WorkspaceMenuStructure/Public/WorkspaceMenuStructure.h"
#include "Editor/WorkspaceMenuStructure/Public/WorkspaceMenuStructureModule.h"
#endif

#include "HAL/IConsoleManager.h"
#include "Framework/Application/SlateApplication.h"

#include "Engine/Texture2D.h"
#include "Widgets/SWindow.h"
#include "SImGuiWindow.h"

#define LOCTEXT_NAMESPACE "ImGuiPlugin"

#if WITH_EDITOR
const FName FImGuiPluginModule::ImGuiTabName = TEXT("ImGuiTab");
#endif

FOnImGuiPluginInitialized FImGuiPluginModule::OnPluginInitialized = {};

void FImGuiPluginModule::StartupModule()
{
	IMGUI_CHECKVERSION();

#if WITH_EDITOR
	FTabSpawnerEntry& SpawnerEntry = FGlobalTabmanager::Get()->RegisterNomadTabSpawner(ImGuiTabName, FOnSpawnTab::CreateStatic(&FImGuiPluginModule::SpawnImGuiTab))
		.SetDisplayName(LOCTEXT("ImGuiTabTitle", "ImGui"))
		.SetTooltipText(LOCTEXT("ImGuiTabToolTip", "ImGui UI"))
		.SetIcon(FSlateIcon(FEditorStyle::GetStyleSetName(), "ClassIcon.WidgetBlueprint"))
		.SetGroup(WorkspaceMenu::GetMenuStructure().GetDeveloperToolsLogCategory());
#endif

	UTexture2D* MissingImageTexture = LoadObject<UTexture2D>(nullptr, TEXT("/ImGuiPlugin/T_MissingImage.T_MissingImage"));
	checkf(MissingImageTexture, TEXT("T_MissingImage texture not found, did you mark it for cooking?"));
	m_MissingImageTextureID = RegisterTexture(MissingImageTexture);
	
	UTexture2D* DefaultFontTexture = LoadObject<UTexture2D>(nullptr, TEXT("/ImGuiPlugin/T_DefaultFont.T_DefaultFont"));
	checkf(DefaultFontTexture, TEXT("T_DefaultFont texture not found, did you mark it for cooking?"));
	m_DefaultFontTextureID = RegisterTexture(DefaultFontTexture);
	
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
					FConsoleCommandDelegate::CreateRaw(this, &FImGuiPluginModule::OpenImGuiMainWindow));
			}
		}
	);

	OnPluginInitialized.Broadcast(*this);
}

void FImGuiPluginModule::ShutdownModule()
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
TSharedRef<SDockTab> FImGuiPluginModule::SpawnImGuiTab(const FSpawnTabArgs& SpawnTabArgs)
{
	const TSharedRef<SDockTab> ImguiTab =
		SNew(SDockTab)
		.Icon(FEditorStyle::GetBrush("ClassIcon.WidgetBlueprint"))
		.TabRole(ETabRole::NomadTab);

	ImguiTab->SetContent(SNew(SImGuiMainWindow));

	return ImguiTab;
}
#endif

void FImGuiPluginModule::OpenImGuiMainWindow()
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
		m_ImGuiMainWindow->SetContent(SNew(SImGuiMainWindow));

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

TSharedPtr<SWindow> FImGuiPluginModule::CreateWidget(const FString& WindowName, const FVector2D& WindowSize, FOnTickImGuiWidgetDelegate TickDelegate)
{	
	TSharedPtr<SWindow> Window = SNew(SWindow)
		.Title(FText::FromString(WindowName))
		.IsTopmostWindow(true)
		.ClientSize(WindowSize)
		.AutoCenter(EAutoCenter::None)
		.SupportsMaximize(false)
		.SupportsMinimize(false)
		.SizingRule(ESizingRule::UserSized);
	Window = FSlateApplication::Get().AddWindow(Window.ToSharedRef());

	TSharedPtr<SImGuiWidgetWindow> ImGuiWindow = SNew(SImGuiWidgetWindow).OnTickDelegate(TickDelegate);
	Window->SetContent(ImGuiWindow.ToSharedRef());

	return Window;
}

ImTextureID FImGuiPluginModule::RegisterTexture(UTexture2D* Texture)
{
	checkf(Texture, TEXT("Texture is invalid!"));
	if (Texture)
	{
		const FName TextureName = Texture->GetFName();

		if (const ImTextureID* ExistingTextureID = m_TextureIDMap.Find(TextureName))
		{
			return *ExistingTextureID;
		}

		const ImTextureID NewTextureID = IndexToImGuiID(m_TextureBrushes.Num());
		m_TextureIDMap.Add(TextureName, NewTextureID);

		FSlateBrush& SlateBrush = m_TextureBrushes.AddDefaulted_GetRef();
		SlateBrush.SetResourceObject(Texture);

		return NewTextureID;
	}
	return IndexToImGuiID((uint32)INDEX_NONE);
}

void FImGuiPluginModule::UnregisterTexture(UTexture2D* Texture)
{
	checkf(Texture, TEXT("Texture is invalid!"));
	if (Texture)
	{
		const FName TextureName = Texture->GetFName();

		ImTextureID TextureID;
		if (m_TextureIDMap.RemoveAndCopyValue(TextureName, TextureID))
		{
			const uint32 BrushIndex = ImGuiIDToIndex(TextureID);
			if (m_TextureBrushes.IsValidIndex(BrushIndex))
			{
				// NOTE: this will leave holes in the array, we can reuse freed indices.
				m_TextureBrushes[BrushIndex].SetResourceObject(nullptr);
			}
		}
	}
}

IMPLEMENT_MODULE(FImGuiPluginModule, ImGuiPlugin)

#undef LOCTEXT_NAMESPACE
