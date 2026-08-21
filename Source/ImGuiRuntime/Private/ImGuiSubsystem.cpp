// Copyright 2024-26 Amit Kumar Mehar. All Rights Reserved.

#include "ImGuiSubsystem.h"

#include "Misc/App.h"
#include "SImGuiWidgets.h"
#include "HAL/FileManager.h"
#include "Widgets/SWindow.h"
#include "RenderingThread.h"
#include "Misc/EngineVersion.h"
#include "Misc/ConfigCacheIni.h"
#include "Utils/ImGuiImageCache.h"
#include "Framework/Application/SlateApplication.h"

#if WITH_ENGINE
#include "UObject/Package.h"
#include "TextureResource.h"
#include "Engine/Texture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#endif

static int32 GCaptureNextGpuFrames = 0;
static FAutoConsoleVariableRef CVarRenderCaptureNextImGuiFrame(
	TEXT("imgui.CaptureGpuFrames"),
	GCaptureNextGpuFrames,
	TEXT("Enable capturing of ImGui rendering for the next N draws"));

#if WITH_FREETYPE
#include "imgui/misc/freetype/imgui_freetype.cpp"

static TAutoConsoleVariable<bool> CVarEnableFreeType(
	TEXT("imgui.EnableFreeType"),
	true,
	TEXT("Enable FreeType font loader."),
	ECVF_ReadOnly);
#endif

/*--------------------------------------------------------------------------------------------------------------------------*/

const FSlateShaderResourceProxy* FImGuiTextureResource::GetSlateShaderResourceProxy() const
{
	check(Storage.IsType<FSlateResourceHandle>());

	const FSlateResourceHandle& ResourceHandle = Storage.Get<FSlateResourceHandle>();
	return ResourceHandle.GetResourceProxy();
}

FSlateShaderResource* FImGuiTextureResource::GetSlateShaderResource() const
{
	if (Storage.IsType<FSlateResourceHandle>())
	{
		const FSlateResourceHandle& ResourceHandle = Storage.Get<FSlateResourceHandle>();
		const FSlateShaderResourceProxy* ResourcProxy = ResourceHandle.GetResourceProxy();
		return ResourcProxy ? ResourcProxy->Resource : nullptr;
	}
	else if (Storage.IsType<FSlateShaderResource*>())
	{
		return Storage.Get<FSlateShaderResource*>();
	}
	ensureAlwaysMsgf(false, TEXT("Resource type not handled!"));
	return nullptr;
}

/*--------------------------------------------------------------------------------------------------------------------------*/

UImGuiSubsystem::FOnSubsystemInitialized UImGuiSubsystem::OnSubsystemInitialized = {};
TUniquePtr<UImGuiSubsystem> UImGuiSubsystem::SubsystemInstance;
FSimpleMulticastDelegate UImGuiSubsystem::OnBeginImGuiFrame = {};
FSimpleMulticastDelegate UImGuiSubsystem::OnEndImGuiFrame = {};
FSimpleMulticastDelegate UImGuiSubsystem::OnShutdown = {};

const FString& UImGuiSubsystem::GetSaveDataConfigFilepath()
{
	// uses AppData folder to keep the widget data in sync b/w editor and packaged builds
	const TCHAR* UserSettingsDir = FPlatformProcess::UserSettingsDir();
	const FString EngineVersion = FEngineVersion::Current().ToString(EVersionComponent::Minor);
	static const FString ConfigFilepath =
		FPaths::Combine(UserSettingsDir, *FApp::GetEpicProductIdentifier(), EngineVersion, TEXT("Config/ImGui/ImGuiSaveData.ini"));
	return ConfigFilepath;
}

void UImGuiSubsystem::InitializeSubsystemInstance()
{
	if (UImGuiSubsystem::ShouldEnableImGui())
	{
		SubsystemInstance = MakeUnique<UImGuiSubsystem>();
		SubsystemInstance->Initialize();
	}
}

void UImGuiSubsystem::ReleaseSubsystemInstance()
{
	if (SubsystemInstance)
	{
		OnShutdown.Broadcast();

		SubsystemInstance->Deinitialize();
		SubsystemInstance = nullptr;
	}
}

UImGuiSubsystem* UImGuiSubsystem::Get()
{
	check(IsInGameThread());
	return SubsystemInstance.Get();
}

void UImGuiSubsystem::Initialize()
{
	// setup config file for storing widget specific data
	{
		m_SaveDataConfigFile = GConfig->Find(GetSaveDataConfigFilepath());
		if (!m_SaveDataConfigFile)
		{
			m_SaveDataConfigFile = &GConfig->Add(GetSaveDataConfigFilepath(), FConfigFile{});
		}

		if (m_SaveDataConfigFile)
		{
			// needed to enable saving
			m_SaveDataConfigFile->NoSave = false;
		}
	}

	// setup ini file path directory
	{
		m_IniDirectoryPath = FAnsiString(FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("ImGui")));
		IFileManager::Get().MakeDirectory(UTF8_TO_TCHAR(*m_IniDirectoryPath), true);
	}

	// NOTE: Add reference to make sure ImGuiContext cannot release the font atlas
	m_SharedFontAtlas = MakeShared<ImFontAtlas, ESPMode::NotThreadSafe>();
	m_SharedFontAtlas->TexMinWidth  = 512;
	m_SharedFontAtlas->TexMinHeight = 512;
	m_SharedFontAtlas->RefCount = 1;
#if WITH_FREETYPE
	if (CVarEnableFreeType.GetValueOnAnyThread())
	{
		m_SharedFontAtlas->SetFontLoader(ImGuiFreeType::GetFontLoader());
	}
#endif
	if (!m_SharedFontAtlas->AddFontFromFileTTF(TCHAR_TO_ANSI(*(FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Regular.ttf"))), 15.f))
	{
		m_SharedFontAtlas->AddFontDefaultBitmap();
	}

#ifdef WITH_NET_IMGUI
	// Slate brush cache which writes directly into ImGuiFontAtlas
	// allows showing FSlateBrush on NetImGui server
	m_ImageCache = MakeUnique<ImGuiUtils::FImGuiImageCache>(m_SharedFontAtlas);
#endif

	// upto 8 shared font textures at a time (to account for repacking)
	// when spammed ImGui can cycle through a lot of atlases (most I encountered was 5)
	m_SharedFontAtlasTextures.SetNum(8);

	OnSubsystemInitialized.Broadcast(this);

	// first frame setup
	BeginImGuiFrame();

	FCoreDelegates::OnBeginFrame.AddRaw(this, &UImGuiSubsystem::BeginImGuiFrame);
	FCoreDelegates::OnEndFrame.AddRaw(this, &UImGuiSubsystem::EndImGuiFrame);
}

void UImGuiSubsystem::Deinitialize()
{
#ifdef WITH_NET_IMGUI
	m_ImageCache.Reset();
#endif

	// ensure all widgets have released the shared font reference (all slate widgets should be destroyed at this point)
	check(m_SharedFontAtlas->RefCount == 1);
	m_SharedFontAtlas = nullptr;

	m_SharedFontAtlasTextures.Reset();

	FCoreDelegates::OnBeginFrame.RemoveAll(this);
	FCoreDelegates::OnEndFrame.RemoveAll(this);
}

bool UImGuiSubsystem::ShouldEnableImGui()
{
	return !IsRunningCommandlet();
}

void UImGuiSubsystem::AddReferencedObjects(FReferenceCollector& Collector)
{
#if WITH_ENGINE
	for (FImGuiFontTextureEntry& TextureEntry : m_SharedFontAtlasTextures)
	{
		if (TextureEntry.BrushTexture)
		{
			Collector.AddReferencedObject(TextureEntry.BrushTexture);
		}
	}
#endif
}

TSharedPtr<SWindow> UImGuiSubsystem::CreateWidget(const FString& WindowName, FVector2f WindowSize, FOnTickImGuiWidgetDelegate TickDelegate)
{
	TSharedPtr<SWindow> Window = SNew(SWindow)
		.Title(FText::FromString(WindowName))
		.ClientSize(WindowSize)
		.AutoCenter(EAutoCenter::None)
		.SupportsMaximize(false)
		.SupportsMinimize(false)
		.SizingRule(ESizingRule::UserSized);
	Window = FSlateApplication::Get().AddWindow(Window.ToSharedRef());

	TSharedPtr<SImGuiWidget> ImGuiWindow =
		SNew(SImGuiWidget)
		.MainViewportWindow(Window)
		.OnTickDelegate(TickDelegate)
		.ConfigFileName(TCHAR_TO_UTF8(*WindowName));

	Window->SetContent(ImGuiWindow.ToSharedRef());

	return Window;
}

bool UImGuiSubsystem::SaveConfigToDisk() const
{
	if (m_SaveDataConfigFile)
	{
		GConfig->Flush(false, GetSaveDataConfigFilepath());
		return true;
	}
	return false;
}

#ifdef IMGUI_ALLOW_MENUBAR_EXTENSION
void UImGuiSubsystem::RegisterMainMenuWidget(
	const UWorld* World, const char* WidgetPath, const char* WidgetToolTip, const FSlateBrush* WidgetIcon,
	FOnTickImGuiWidgetDelegate TickDelegate, EImGuiMainMenuWidgetFlags WidgetFlags) const
{
	// defined in ImGuiMenuExtension.cpp
	extern void RegisterMainMenuWidgetForWorld(
		const UWorld* World, const char* WidgetPath, const char* WidgetToolTip, const FSlateBrush * WidgetIcon,
		FOnTickImGuiWidgetDelegate TickDelegate, EImGuiMainMenuWidgetFlags WidgetFlags);

	RegisterMainMenuWidgetForWorld(World, WidgetPath, WidgetToolTip, WidgetIcon, TickDelegate, WidgetFlags);
}

void UImGuiSubsystem::UnregisterMainMenuWidget(const UWorld* World, const char* WidgetPath) const
{
	// defined in ImGuiMenuExtension.cpp
	extern void UnregisterMainMenuWidgetForWorld(const UWorld* World, const char* WidgetPath);

	UnregisterMainMenuWidgetForWorld(World, WidgetPath);
}

bool* UImGuiSubsystem::GetMainMenuWidgetActiveState(const UWorld* World, const char* WidgetPath) const
{
	// defined in ImGuiMenuExtension.cpp
	extern bool* GetMainMenuWidgetActiveStateForWorld(const UWorld* World, const char* WidgetPath);

	return GetMainMenuWidgetActiveStateForWorld(World, WidgetPath);
}

FImGuiTickContext* UImGuiSubsystem::GetMainMenuWidgetTickContext(const UWorld* World) const
{
	// defined in ImGuiMenuExtension.cpp
	extern FImGuiTickContext* GetMainMenuWidgetTickContextForWorld(const UWorld* World);

	return GetMainMenuWidgetTickContextForWorld(World);
}
#endif //#ifdef IMGUI_ALLOW_MENUBAR_EXTENSION

void UImGuiSubsystem::BeginImGuiFrame()
{
	m_OneFrameResources.Reset();
	m_OneFrameSlateBrushes.Reset();

	// queue font updates
	ImFontAtlasUpdateNewFrame(m_SharedFontAtlas.Get(), ++m_FontAtlasBuilderFrameCount, true);

	// register all font altases
	for (const FImGuiFontTextureEntry& TextureEntry : m_SharedFontAtlasTextures)
	{
		if (TextureEntry.Brush)
		{
			RegisterOneFrameResource(TextureEntry.Brush.Get());
		}
		else
		{
			// queue an empty slot which may get populated by UpdateFontAtlasTexture
			m_OneFrameResources.Add(FImGuiTextureResource{nullptr});
		}
	}

	GCaptureNextGpuFrames = FMath::Max(0, GCaptureNextGpuFrames - 1);

#ifdef WITH_NET_IMGUI
	if (m_ImageCache)
	{
		m_ImageCache->OnBeginFrame();
	}
#endif

	OnBeginImGuiFrame.Broadcast();
}

void UImGuiSubsystem::EndImGuiFrame()
{
	OnEndImGuiFrame.Broadcast();
}

ImTextureRef UImGuiSubsystem::GetSharedFontTextureID() const
{
	return m_SharedFontAtlas->TexRef;
}

void UImGuiSubsystem::CommitSharedFontAtlasChanges()
{
	ImFontAtlasUpdateNewFrame(m_SharedFontAtlas.Get(), ++m_FontAtlasBuilderFrameCount, true);
}

int32 UImGuiSubsystem::AllocateFontAtlasTexture(int32 SizeX, int32 SizeY)
{
	static const FName FontTextureName = TEXT("ImGui_SharedFontTexture");

	for (int32 TextureIndex = 0; TextureIndex < m_SharedFontAtlasTextures.Num(); ++TextureIndex)
	{
		if (!m_SharedFontAtlasTextures[TextureIndex].bInUse)
		{
#if WITH_ENGINE && IMGUI_ALLOW_LOCAL_DRAWING
			if (FSlateApplication::IsInitialized())
			{
				if (!m_SharedFontAtlasTextures[TextureIndex].Brush)
				{
					m_SharedFontAtlasTextures[TextureIndex].Brush = MakeShared<FSlateBrush>();
				}

				UTextureRenderTarget2D* Texture = (UTextureRenderTarget2D*)m_SharedFontAtlasTextures[TextureIndex].Brush->GetResourceObject();
				if (!Texture)
				{
					Texture = NewObject<UTextureRenderTarget2D>(GetTransientPackage(), FName(FontTextureName, TextureIndex + 1));
					Texture->Filter = TextureFilter::TF_Bilinear;
					Texture->RenderTargetFormat = ETextureRenderTargetFormat::RTF_RGBA8;
					Texture->OverrideFormat = PF_R8G8B8A8;
					Texture->ClearColor = FLinearColor(0, 0, 0, 0);
					Texture->bNoFastClear = true;
					Texture->InitAutoFormat(SizeX, SizeY);
					Texture->UpdateResourceImmediate(/*bClearRenderTarget=*/false);

					m_SharedFontAtlasTextures[TextureIndex].BrushTexture = Texture;
					m_SharedFontAtlasTextures[TextureIndex].Brush->SetResourceObject(Texture);
					m_OneFrameResources[TextureIndex] = FImGuiTextureResource{ m_SharedFontAtlasTextures[TextureIndex].Brush->GetRenderingResource() };
				}
			}
#endif
			m_SharedFontAtlasTextures[TextureIndex].bInUse = true;
			return TextureIndex;
		}
	}
	// TODO: add logic to flush render thread and recycle textures here.
	checkNoEntry();
	return INDEX_NONE;
}

void UImGuiSubsystem::ReleaseFontAtlasTexture(int32 Index)
{
	// TODO: maybe add some logic to release unused textures after a few frames
	m_SharedFontAtlasTextures[Index].bInUse = false;
}

void UImGuiSubsystem::UpdateFontAtlasTextures(ImTextureData** Textures, int32 TextureCount)
{
	for (int32 TextureIndex = 0; TextureIndex < TextureCount; ++TextureIndex)
	{
		ImTextureData* TexData = Textures[TextureIndex];
		if (TexData->Status != ImTextureStatus_OK)
		{
			UpdateFontAtlasTexture(TexData);
		}
	}
}

void UImGuiSubsystem::UpdateFontAtlasTexture(ImTextureData* TexData)
{
	if (TexData->Status == ImTextureStatus_WantCreate || TexData->Status == ImTextureStatus_WantUpdates)
	{
		const int32 FontAtlasWidth = TexData->Width;
		const int32 FontAtlasHeight = TexData->Height;

		if (TexData->Status == ImTextureStatus_WantCreate)
		{
			check(TexData->BytesPerPixel == GPixelFormats[PF_R8G8B8A8].BlockBytes);
			TexData->SetTexID(AllocateFontAtlasTexture(FontAtlasWidth, FontAtlasHeight));
		}

#if IMGUI_ALLOW_LOCAL_DRAWING
		if (FSlateApplication::IsInitialized())
		{
#if WITH_ENGINE
			UTextureRenderTarget2D* AtlasTexture = (UTextureRenderTarget2D*)m_SharedFontAtlasTextures[TexData->GetTexID()].Brush->GetResourceObject();

			bool bReuploadTexture = (TexData->Status == ImTextureStatus_WantCreate);
			if (AtlasTexture->SizeX != FontAtlasWidth || AtlasTexture->SizeY != FontAtlasHeight)
			{
				AtlasTexture->ResizeTarget(FontAtlasWidth, FontAtlasHeight);
				bReuploadTexture = true;
			}

			if (FApp::CanEverRender())
			{
				const ImTextureRect UpdateRect = bReuploadTexture ? ImTextureRect(0, 0, FontAtlasWidth, FontAtlasHeight) : TexData->UpdateRect;
				ENQUEUE_RENDER_COMMAND(UpdateFontTexture)(
					[this,
					SrcPitch = TexData->GetPitch(),
					SrcData = (uint8*)TexData->GetPixelsAt(UpdateRect.x, UpdateRect.y),
					UpdateRegion = FUpdateTextureRegion2D(UpdateRect.x, UpdateRect.y, 0, 0, UpdateRect.w, UpdateRect.h),
					TexResource = AtlasTexture->GameThread_GetRenderTargetResource()](FRHICommandListImmediate& RHICmdList)
					{
						RHICmdList.Transition(FRHITransitionInfo(TexResource->GetTexture2DRHI(), ERHIAccess::Unknown, ERHIAccess::CopyDest));
						RHICmdList.UpdateTexture2D(TexResource->GetTexture2DRHI(), 0, UpdateRegion, SrcPitch, SrcData);
						RHICmdList.Transition(FRHITransitionInfo(TexResource->GetTexture2DRHI(), ERHIAccess::CopyDest, ERHIAccess::SRVMask));
					});
			}
#else
			static const FName FontTextureName = TEXT("ImGui_SharedFontTexture");
			static int32 FontTextureNameCounter = 0;

			int32 TextureIndex = TexData->GetTexID();
			m_SharedFontAtlasTextures[TextureIndex].Brush = FSlateDynamicImageBrush::CreateWithImageData(FName(FontTextureName, ++FontTextureNameCounter),
				FVector2D(FontAtlasWidth, FontAtlasHeight),
				TArray((uint8*)TexData->GetPixelsAt(0, 0), FontAtlasWidth * FontAtlasHeight * TexData->BytesPerPixel));
			m_OneFrameResources[TextureIndex] = FImGuiTextureResource{ m_SharedFontAtlasTextures[TextureIndex].Brush->GetRenderingResource() };
#endif
		}
#endif //#if IMGUI_ALLOW_LOCAL_DRAWING

		TexData->SetStatus(ImTextureStatus_OK);
	}
	else if (TexData->Status == ImTextureStatus_WantDestroy && TexData->UnusedFrames > 1)
	{
		// latest shared font texture data should never be destroyed!
		check(TexData != m_SharedFontAtlas->TexData);

		ReleaseFontAtlasTexture(TexData->GetTexID());

		TexData->SetStatus(ImTextureStatus_Destroyed);
		TexData->SetTexID(ImTextureID_Invalid);
	}
}

bool UImGuiSubsystem::CaptureGpuFrame() const
{
	return GCaptureNextGpuFrames > 0;
}

FImGuiImageBindingParams UImGuiSubsystem::RegisterOneFrameResource(const FSlateBrush* SlateBrush, FVector2f LocalSize, float DrawScale/*=1.f*/)
{
	FImGuiImageBindingParams Params{};
	Params.Size = ImVec2(LocalSize.X, LocalSize.Y) * DrawScale;
	if (!SlateBrush)
	{
		return Params;
	}

	const bool bIsValidImageBrush = (SlateBrush->GetImageType() != ESlateBrushImageType::NoImage) || ::IsValid(SlateBrush->GetResourceObject());
	if (!ensureMsgf(bIsValidImageBrush, TEXT("Prefer primitive drawing for colored slate brushes.")))
	{
		return Params;
	}

#ifdef WITH_NET_IMGUI
	if (m_ImageCache && ImGuiUtils::FImGuiImageCache::CanLoadBrush(*SlateBrush))
	{
		return m_ImageCache->GetOrLoadBrush(*SlateBrush, LocalSize, DrawScale);
	}
#endif

	if (FApp::CanEverRender())
	{
		const FSlateResourceHandle& ResourceHandle = SlateBrush->GetRenderingResource(LocalSize, DrawScale);
		const FSlateShaderResourceProxy* Proxy = ResourceHandle.GetResourceProxy();
		if (Proxy)
		{
			int32 ResourceHandleIndex;
			// NOTE: when updating slate atlases `Proxy->Resource` can return null which gets patched later in the frame.
			// So make sure we get a unique `ResourceHandleIndex` here in order to allow shader to override the UV data.
			if (Proxy->Resource)
			{
				ResourceHandleIndex = m_OneFrameResources.IndexOfByPredicate([Proxy](const auto& TextureResource) { return TextureResource.GetSlateShaderResource() == Proxy->Resource; });
			}
			else
			{
				ResourceHandleIndex = INDEX_NONE;
			}

			if (ResourceHandleIndex == INDEX_NONE)
			{
				ResourceHandleIndex = m_OneFrameResources.Emplace(ResourceHandle);
			}

			Params.UV0 = ImVec2(Proxy->StartUV.X, Proxy->StartUV.Y);
			Params.UV1 = ImVec2(Proxy->StartUV.X + Proxy->SizeUV.X, Proxy->StartUV.Y + Proxy->SizeUV.Y);
			Params.Id = ResourceHandleIndex;
		}
	}
	return Params;
}

FImGuiImageBindingParams UImGuiSubsystem::RegisterOneFrameResource(FSlateShaderResource* SlateShaderResource)
{
	FImGuiImageBindingParams Params = {};
	if (SlateShaderResource)
	{
		int32 ResourceHandleIndex = m_OneFrameResources.IndexOfByPredicate([&](const auto& TextureResource) { return TextureResource.GetSlateShaderResource() == SlateShaderResource; });
		if (ResourceHandleIndex == INDEX_NONE)
		{
			ResourceHandleIndex = m_OneFrameResources.Emplace(SlateShaderResource);
		}

		Params.Size = ImVec2(SlateShaderResource->GetWidth(), SlateShaderResource->GetHeight());
		Params.UV0 = ImVec2(0.f, 0.f);
		Params.UV1 = ImVec2(1.f, 1.f);
		Params.Id = ResourceHandleIndex;
	}
	return Params;
}

#if WITH_ENGINE
FImGuiImageBindingParams UImGuiSubsystem::RegisterOneFrameResource(UTexture2D* Texture)
{
	if (!Texture)
	{
		return {};
	}

	FSlateBrush& NewBrush = m_OneFrameSlateBrushes.AddDefaulted_GetRef();
	NewBrush.SetResourceObject(Texture);

	return RegisterOneFrameResource(&NewBrush);
}
#endif
