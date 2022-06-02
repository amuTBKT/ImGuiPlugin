#include "ImGuiRuntimeModule.h"

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
const FName FImGuiRuntimeModule::ImGuiTabName = TEXT("ImGuiTab");
#endif

FOnImGuiPluginInitialized FImGuiRuntimeModule::OnPluginInitialized = {};

void FImGuiRuntimeModule::StartupModule()
{
	IMGUI_CHECKVERSION();

#if WITH_EDITOR
	FTabSpawnerEntry& SpawnerEntry = FGlobalTabmanager::Get()->RegisterNomadTabSpawner(ImGuiTabName, FOnSpawnTab::CreateStatic(&FImGuiRuntimeModule::SpawnImGuiTab))
		.SetDisplayName(LOCTEXT("ImGuiTabTitle", "ImGui"))
		.SetTooltipText(LOCTEXT("ImGuiTabToolTip", "ImGui UI"))
		.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "ClassIcon.WidgetBlueprint"))
		.SetGroup(WorkspaceMenu::GetMenuStructure().GetDeveloperToolsLogCategory());
#endif

	UTexture2D* DefaultFontTexture = LoadObject<UTexture2D>(nullptr, TEXT("/ImGuiPlugin/T_DefaultFont.T_DefaultFont"));
	checkf(DefaultFontTexture, TEXT("T_DefaultFont texture not found, did you mark it for cooking?"));
	m_DefaultFontSlateBrush.SetResourceObject(DefaultFontTexture);
	
	UTexture2D* MissingImageTexture = LoadObject<UTexture2D>(nullptr, TEXT("/ImGuiPlugin/T_MissingImage.T_MissingImage"));
	checkf(MissingImageTexture, TEXT("T_MissingImage texture not found, did you mark it for cooking?"));
	m_MissingImageSlateBrush.SetResourceObject(MissingImageTexture);
	
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

	FCoreDelegates::OnBeginFrame.AddRaw(this, &FImGuiRuntimeModule::OnBeginFrame);

	OnPluginInitialized.Broadcast(*this);
}

void FImGuiRuntimeModule::ShutdownModule()
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
TSharedRef<SDockTab> FImGuiRuntimeModule::SpawnImGuiTab(const FSpawnTabArgs& SpawnTabArgs)
{
	const TSharedRef<SDockTab> ImguiTab =
		SNew(SDockTab)
		.TabRole(ETabRole::NomadTab);
	ImguiTab->SetTabIcon(FAppStyle::GetBrush("ClassIcon.WidgetBlueprint"));
	ImguiTab->SetContent(SNew(SImGuiMainWindow));

	return ImguiTab;
}
#endif

void FImGuiRuntimeModule::OpenImGuiMainWindow()
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

TSharedPtr<SWindow> FImGuiRuntimeModule::CreateWidget(const FString& WindowName, const FVector2D& WindowSize, FOnTickImGuiWidgetDelegate TickDelegate)
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

void FImGuiRuntimeModule::OnBeginFrame()
{
	OneFrameResourceHandles.Reset();
	CreatedSlateBrushes.Reset();
	
	m_DefaultFontImageParams = RegisterOneFrameResource(&m_DefaultFontSlateBrush);
	m_MissingImageParams = RegisterOneFrameResource(&m_MissingImageSlateBrush);
}

FImGuiImageBindParams FImGuiRuntimeModule::RegisterOneFrameResource(const FSlateBrush* SlateBrush, FVector2D LocalSize, float DrawScale)
{	
	const uint32 NewIndex = OneFrameResourceHandles.Num();

	const FSlateResourceHandle& ResourceHandle = SlateBrush->GetRenderingResource(LocalSize, DrawScale);
	OneFrameResourceHandles.Add(ResourceHandle);

	const FSlateShaderResourceProxy* Proxy = ResourceHandle.GetResourceProxy();
	const FVector2f StartUV = Proxy->StartUV;
	const FVector2f SizeUV = Proxy->SizeUV;

	FImGuiImageBindParams Params = {};
	Params.Size = ImVec2(LocalSize.X, LocalSize.Y);
	Params.UV0 = ImVec2(StartUV.X, StartUV.Y);
	Params.UV1 = ImVec2(StartUV.X + SizeUV.X, StartUV.Y + SizeUV.Y);
	Params.Id = IndexToImGuiID(NewIndex);

	return Params;
}

FImGuiImageBindParams FImGuiRuntimeModule::RegisterOneFrameResource(const FSlateBrush* SlateBrush)
{
	return RegisterOneFrameResource(SlateBrush, SlateBrush->GetImageSize(), 1.0f);
}

FImGuiImageBindParams FImGuiRuntimeModule::RegisterOneFrameResource(UTexture2D* Texture)
{
	FSlateBrush& NewBrush = CreatedSlateBrushes.AddDefaulted_GetRef();
	NewBrush.SetResourceObject(Texture);

	return RegisterOneFrameResource(&NewBrush);
}

IMPLEMENT_MODULE(FImGuiRuntimeModule, ImGuiRuntime)

#undef LOCTEXT_NAMESPACE
