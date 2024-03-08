#include "ImGuiRuntimeModule.h"

#if WITH_EDITOR
#include "EditorStyleSet.h"
#include "Widgets/Docking/SDockTab.h"
#include "Framework/Docking/TabManager.h"
#include "Editor/WorkspaceMenuStructure/Public/WorkspaceMenuStructure.h"
#include "Editor/WorkspaceMenuStructure/Public/WorkspaceMenuStructureModule.h"
#endif

#include "HAL/IConsoleManager.h"
#include "Framework/Application/SlateApplication.h"

#include "TextureResource.h"
#include "Engine/Texture2D.h"
#include "Widgets/SWindow.h"
#include "SImGuiWidgets.h"
#include "Engine/World.h"
#include "Misc/App.h"

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
	if (!FApp::CanEverRender())
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

	DefaultFontAtlas = MakeUnique<ImFontAtlas>();
	DefaultFontAtlas->Build();

	auto CreateTextureRGBA8 = [](FName DebugName, int32 Width, int32 Height, uint8* ImageData)
	{
		UTexture2D* Texture = UTexture2D::CreateTransient(Width, Height, PF_R8G8B8A8, DebugName);
		// compression and mip settings
		Texture->CompressionSettings = TextureCompressionSettings::TC_Default;
		Texture->LODGroup = TEXTUREGROUP_UI;
		Texture->MipGenSettings = TMGS_NoMipmaps;
		Texture->SRGB = false;
		Texture->AddressX = TextureAddress::TA_Clamp;
		Texture->AddressY = TextureAddress::TA_Clamp;

		uint8* MipData = static_cast<uint8*>(Texture->GetPlatformData()->Mips[0].BulkData.Lock(LOCK_READ_WRITE));
		FMemory::Memcpy(MipData, ImageData, Texture->GetPlatformData()->Mips[0].BulkData.GetBulkDataSize());
		Texture->GetPlatformData()->Mips[0].BulkData.Unlock();
		Texture->UpdateResource();

		return Texture;
	};

	int32 FontAtlasWidth, FontAtlasHeight, BytesPerPixel;
	unsigned char* FontAtlasData;
	DefaultFontAtlas->GetTexDataAsRGBA32(&FontAtlasData, &FontAtlasWidth, &FontAtlasHeight, &BytesPerPixel);
	check(BytesPerPixel == GPixelFormats[PF_R8G8B8A8].BlockBytes && FontAtlasData);

	// 1x1 magenta texture
	int32 MissingImageWidth = 1, MissingImageHeight = 1;
	uint32 MissingPixelData = FColor::Magenta.DWColor();

	DefaultFontTexture = CreateTextureRGBA8(FName(TEXT("ImGui_DefaultFontAtlas")), FontAtlasWidth, FontAtlasHeight, FontAtlasData);
	DefaultFontTexture->AddToRoot();

	MissingImageTexture = CreateTextureRGBA8(FName(TEXT("ImGui_MissingImage")), MissingImageWidth, MissingImageHeight, (uint8_t*)&MissingPixelData);
	MissingImageTexture->AddToRoot();

	m_DefaultFontSlateBrush.SetResourceObject(DefaultFontTexture);
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

	IsPluginInitialized = true;
	OnPluginInitialized.Broadcast(*this);

	// first frame setup
	OnBeginFrame();
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

	if (DefaultFontTexture)
	{
		DefaultFontTexture->RemoveFromRoot();
	}

	if (MissingImageTexture)
	{
		MissingImageTexture->RemoveFromRoot();
	}
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
	if (!IsPluginInitialized)
	{
		return;
	}

	OneFrameResources.Reset();
	CreatedSlateBrushes.Reset();
	
	m_MissingImageParams = RegisterOneFrameResource(&m_MissingImageSlateBrush);
	m_DefaultFontImageParams = RegisterOneFrameResource(&m_DefaultFontSlateBrush);

	DefaultFontAtlas->TexID = GetDefaultFontTextureID();

	CaptureNextGpuFrames = FMath::Max(0, CaptureNextGpuFrames - 1);
}

bool FImGuiRuntimeModule::CaptureGpuFrame() const
{
	return CaptureNextGpuFrames > 0;
}

FImGuiImageBindingParams FImGuiRuntimeModule::RegisterOneFrameResource(const FSlateBrush* SlateBrush, FVector2D LocalSize, float DrawScale)
{
	check(IsPluginInitialized);
	
	if (!SlateBrush)
	{
		return {};
	}

	const FSlateResourceHandle& ResourceHandle = SlateBrush->GetRenderingResource(LocalSize, DrawScale);
	const FSlateShaderResourceProxy* Proxy = ResourceHandle.GetResourceProxy();
	if (!Proxy)
	{
		return {};
	}
	
	const uint32 ResourceHandleIndex = OneFrameResources.Add(FImGuiTextureResource(ResourceHandle));

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
	check(IsPluginInitialized);

	if (!SlateBrush)
	{
		return {};
	}

	return RegisterOneFrameResource(SlateBrush, SlateBrush->GetImageSize(), 1.0f);
}

FImGuiImageBindingParams FImGuiRuntimeModule::RegisterOneFrameResource(UTexture2D* Texture)
{
	check(IsPluginInitialized);

	if (!Texture)
	{
		return {};
	}

	FSlateBrush& NewBrush = CreatedSlateBrushes.AddDefaulted_GetRef();
	NewBrush.SetResourceObject(Texture);

	return RegisterOneFrameResource(&NewBrush);
}

FImGuiImageBindingParams FImGuiRuntimeModule::RegisterOneFrameResource(FSlateShaderResource* SlateShaderResource)
{
	check(IsPluginInitialized);

	if (!SlateShaderResource)
	{
		return {};
	}

	const uint32 ResourceHandleIndex = OneFrameResources.Add(FImGuiTextureResource(SlateShaderResource));

	FImGuiImageBindingParams Params = {};
	Params.Size = ImVec2(SlateShaderResource->GetWidth(), SlateShaderResource->GetHeight());
	Params.UV0 = ImVec2(0.f, 0.f);
	Params.UV1 = ImVec2(1.f, 1.f);
	Params.Id = IndexToImGuiID(ResourceHandleIndex);

	return Params;
}

FSlateShaderResource* FImGuiTextureResource::GetSlateShaderResource() const
{
	if (UnderlyingResource.IsType<FSlateResourceHandle>())
	{
		const FSlateResourceHandle& ResourceHandle = UnderlyingResource.Get<FSlateResourceHandle>();
		if (ResourceHandle.GetResourceProxy() && ResourceHandle.GetResourceProxy()->Resource)
		{
			return ResourceHandle.GetResourceProxy()->Resource;
		}
	}
	else if (UnderlyingResource.IsType<FSlateShaderResource*>())
	{
		return (FSlateShaderResource*)UnderlyingResource.Get<FSlateShaderResource*>();
	}
	return nullptr;
}

IMPLEMENT_MODULE(FImGuiRuntimeModule, ImGuiRuntime)

#undef LOCTEXT_NAMESPACE
