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
#include "SImGuiWidgets.h"

#define LOCTEXT_NAMESPACE "ImGuiPlugin"

static int32 CaptureNextGpuFrames = 0;
static FAutoConsoleVariableRef CVarRenderCaptureNextImGuiFrame(
	TEXT("imgui.CaptureGpuFrames"),
	CaptureNextGpuFrames,
	TEXT("Enable capturing of ImGui rendering for the next N draws"));

#if WITH_EDITOR
const FName FImGuiRuntimeModule::ImGuiTabName = TEXT("ImGuiTab");
#endif

FOnImGuiPluginInitialized FImGuiRuntimeModule::OnPluginInitialized = {};
bool FImGuiRuntimeModule::IsPluginInitialized = false;

void FImGuiRuntimeModule::StartupModule()
{
	IMGUI_CHECKVERSION();

	SETUP_DEFAULT_IMGUI_ALLOCATOR();

#if WITH_EDITOR
	FTabSpawnerEntry& SpawnerEntry = FGlobalTabmanager::Get()->RegisterNomadTabSpawner(ImGuiTabName, FOnSpawnTab::CreateStatic(&FImGuiRuntimeModule::SpawnImGuiTab))
		.SetDisplayName(LOCTEXT("ImGuiTabTitle", "ImGui"))
		.SetTooltipText(LOCTEXT("ImGuiTabToolTip", "ImGui UI"))
		.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Layout"))
		.SetGroup(WorkspaceMenu::GetMenuStructure().GetToolsCategory());
#endif

	UTexture2D* DefaultFontTexture = LoadObject<UTexture2D>(nullptr, TEXT("/ImGuiPlugin/T_DefaultFont.T_DefaultFont"));
	checkf(DefaultFontTexture, TEXT("T_DefaultFont texture not found, did you mark it for cooking?"));
	m_DefaultFontSlateBrush.SetResourceObject(DefaultFontTexture);
	
	UTexture2D* MissingImageTexture = LoadObject<UTexture2D>(nullptr, TEXT("/ImGuiPlugin/T_MissingImage.T_MissingImage"));
	checkf(MissingImageTexture, TEXT("T_MissingImage texture not found, did you mark it for cooking?"));
	m_MissingImageSlateBrush.SetResourceObject(MissingImageTexture);
	
	//[TODO] there are cases when NewFrame/Render can be called before we initialize these *required* resources, so register in advance.
	m_DefaultFontImageParams = RegisterOneFrameResource(&m_DefaultFontSlateBrush);
	m_MissingImageParams = RegisterOneFrameResource(&m_MissingImageSlateBrush);

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

	IsPluginInitialized = true;
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
	ImguiTab->SetTabIcon(FAppStyle::GetBrush("Icons.Layout"));
	ImguiTab->SetContent(SNew(SImGuiMainWindowWidget));

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

TSharedPtr<SWindow> FImGuiRuntimeModule::CreateWindow(const FString& WindowName, const FVector2D& WindowSize, FOnTickImGuiWidgetDelegate TickDelegate)
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

	TSharedPtr<SImGuiWidget> ImGuiWindow = SNew(SImGuiWidget).OnTickDelegate(TickDelegate);
	Window->SetContent(ImGuiWindow.ToSharedRef());

	return Window;
}

void FImGuiRuntimeModule::OnBeginFrame()
{
	OneFrameResourceHandles.Reset();
	CreatedSlateBrushes.Reset();
	
	m_DefaultFontImageParams = RegisterOneFrameResource(&m_DefaultFontSlateBrush);
	m_MissingImageParams = RegisterOneFrameResource(&m_MissingImageSlateBrush);

	CaptureNextGpuFrames = FMath::Max(0, CaptureNextGpuFrames - 1);
}

bool FImGuiRuntimeModule::CaptureGpuFrame() const
{
	return CaptureNextGpuFrames > 0;
}

FImGuiImageBindingParams FImGuiRuntimeModule::RegisterOneFrameResource(const FSlateBrush* SlateBrush, FVector2D LocalSize, float DrawScale)
{	
	const uint32 ResourceHandleIndex = OneFrameResourceHandles.Num();

	const FSlateResourceHandle& ResourceHandle = SlateBrush->GetRenderingResource(LocalSize, DrawScale);
	OneFrameResourceHandles.Add(ResourceHandle);

	const FSlateShaderResourceProxy* Proxy = ResourceHandle.GetResourceProxy();
	const FVector2f StartUV = Proxy->StartUV;
	const FVector2f SizeUV = Proxy->SizeUV;

	FImGuiImageBindingParams Params = {};
	Params.Size = ImVec2(LocalSize.X, LocalSize.Y);
	Params.UV0 = ImVec2(StartUV.X, StartUV.Y);
	Params.UV1 = ImVec2(StartUV.X + SizeUV.X, StartUV.Y + SizeUV.Y);
	Params.Id = IndexToImGuiID(ResourceHandleIndex);

	return Params;
}

FImGuiImageBindingParams FImGuiRuntimeModule::RegisterOneFrameResource(const FSlateBrush* SlateBrush)
{
	return RegisterOneFrameResource(SlateBrush, SlateBrush->GetImageSize(), 1.0f);
}

FImGuiImageBindingParams FImGuiRuntimeModule::RegisterOneFrameResource(UTexture2D* Texture)
{
	FSlateBrush& NewBrush = CreatedSlateBrushes.AddDefaulted_GetRef();
	NewBrush.SetResourceObject(Texture);

	return RegisterOneFrameResource(&NewBrush);
}

IMPLEMENT_MODULE(FImGuiRuntimeModule, ImGuiRuntime)

#undef LOCTEXT_NAMESPACE
