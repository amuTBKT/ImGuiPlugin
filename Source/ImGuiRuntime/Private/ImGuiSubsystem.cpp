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

#ifdef IMGUI_USE_NATIVE_RENDERER
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

static TAutoConsoleVariable<bool> CVarDisableMouseCursorBitmaps(
	TEXT("imgui.DisableMouseCursorBitmaps"),
	true,
	TEXT("Don't build software mouse cursors into ImGui texture atlas (saves a little texture memory)"));

#if WITH_FREETYPE
#include "imgui/misc/freetype/imgui_freetype.cpp"

static TAutoConsoleVariable<bool> CVarEnableFreeType(
	TEXT("imgui.EnableFreeType"),
	true,
	TEXT("Enable FreeType font loader."),
	ECVF_ReadOnly);
#endif

#if defined(WITH_NET_IMGUI) || !defined(IMGUI_USE_NATIVE_RENDERER)
// 1. Needed with NetImGui to allow uploading slate brushes to the server
// 2. Needed for slate rendering path as slate brushes don't play well with custom verts setup
#define USE_LOCAL_IMAGE_CACHE 1
#endif

/*--------------------------------------------------------------------------------------------------------------------------*/

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
	else if (Storage.IsType<FTextureResource*>())
	{
		return nullptr;
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

	m_SharedFontAtlas = MakeShared<ImFontAtlas, ESPMode::NotThreadSafe>();
	m_SharedFontAtlas->TexMinWidth	= 512;
	m_SharedFontAtlas->TexMinHeight	= 512;
	m_SharedFontAtlas->RefCount = 1; // add reference to make sure ImGuiContext cannot release the font atlas
	if (CVarDisableMouseCursorBitmaps.GetValueOnAnyThread())
	{
		m_SharedFontAtlas->Flags = ImFontAtlasFlags_NoMouseCursors;
	}
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

#ifdef USE_LOCAL_IMAGE_CACHE
	// slate brush cache which writes directly into ImGuiFontAtlas
	m_ImageCache = MakeUnique<ImGuiUtils::FImGuiImageCache>(m_SharedFontAtlas);
#endif

	m_SharedFontAtlasTextures.Add(MakeUnique<FImGuiFontTextureEntry>());

	OnSubsystemInitialized.Broadcast(this);

	// first frame setup
	BeginImGuiFrame();

	FCoreDelegates::OnBeginFrame.AddRaw(this, &UImGuiSubsystem::BeginImGuiFrame);
	FCoreDelegates::OnEndFrame.AddRaw(this, &UImGuiSubsystem::EndImGuiFrame);
}

void UImGuiSubsystem::Deinitialize()
{
#ifdef USE_LOCAL_IMAGE_CACHE
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
#ifdef IMGUI_USE_NATIVE_RENDERER
	for (const auto& TextureEntry : m_SharedFontAtlasTextures)
	{
		if (TextureEntry->Texture)
		{
			Collector.AddReferencedObject(TextureEntry->Texture);
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

	// queue font updates
	ImFontAtlasUpdateNewFrame(m_SharedFontAtlas.Get(), ++m_FontAtlasBuilderFrameCount, true);

	// register all font altases
	for (const auto& TextureEntry : m_SharedFontAtlasTextures)
	{
		if (!TextureEntry->bInUse)
			continue;

#ifdef IMGUI_USE_NATIVE_RENDERER
		if (TextureEntry->Texture)
		{
			m_OneFrameResources.Emplace(ToImTextureID(TextureEntry.Get()), FImGuiTextureResource(TextureEntry->Texture->GetResource()));
		}
#else
		if (TextureEntry->Brush)
		{
			m_OneFrameResources.Emplace(ToImTextureID(TextureEntry.Get()), FImGuiTextureResource(TextureEntry->Brush->GetRenderingResource()));
		}
#endif
	}

	GCaptureNextGpuFrames = FMath::Max(0, GCaptureNextGpuFrames - 1);

#ifdef USE_LOCAL_IMAGE_CACHE
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

ImTextureID UImGuiSubsystem::AllocateFontAtlasTexture(int32 SizeX, int32 SizeY)
{
	static const FName FontTextureName = TEXT("ImGui_SharedFontTexture");

	int32 TextureIndex = 0;
	FImGuiFontTextureEntry* TextureEntry = nullptr;
	for (; TextureIndex < m_SharedFontAtlasTextures.Num(); ++TextureIndex)
	{
		if (!m_SharedFontAtlasTextures[TextureIndex]->bInUse)
		{
			TextureEntry = m_SharedFontAtlasTextures[TextureIndex].Get();
			break;
		}
	}
	if (!TextureEntry)
	{
		m_SharedFontAtlasTextures.Add(MakeUnique<FImGuiFontTextureEntry>());
		TextureEntry = m_SharedFontAtlasTextures.Last().Get();
	}

#if IMGUI_ALLOW_LOCAL_DRAWING && defined(IMGUI_USE_NATIVE_RENDERER)
	if (FSlateApplication::IsInitialized())
	{
		UTextureRenderTarget2D* Texture = TextureEntry->Texture;
		if (!Texture)
		{
			Texture = NewObject<UTextureRenderTarget2D>(GetTransientPackage(), FName(FontTextureName, TextureIndex + 1));
			Texture->AddressX = TA_Clamp;
			Texture->AddressY = TA_Clamp;
			Texture->Filter = TextureFilter::TF_Bilinear;
			Texture->RenderTargetFormat = ETextureRenderTargetFormat::RTF_RGBA8;
			Texture->OverrideFormat = PF_B8G8R8A8;
			Texture->ClearColor = FLinearColor(0, 0, 0, 0);
			Texture->bNoFastClear = true;
			Texture->InitAutoFormat(SizeX, SizeY);
			Texture->UpdateResourceImmediate(/*bClearRenderTarget=*/false);

			TextureEntry->Texture = Texture;
		}
	}
#endif

	TextureEntry->bInUse = true;
	return ToImTextureID(TextureEntry);
}

void UImGuiSubsystem::ReleaseFontAtlasTexture(ImTextureID ResourceId)
{
	// TODO: maybe add some logic to release unused textures after a few frames
	FImGuiFontTextureEntry* TextureEntry = FromImTextureID<FImGuiFontTextureEntry>(ResourceId);
	if (TextureEntry)
	{
		TextureEntry->bInUse = false;
	}
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
			ImTextureID ResourceId = TexData->GetTexID();
			FImGuiFontTextureEntry* TextureEntry = FromImTextureID<FImGuiFontTextureEntry>(ResourceId);

#ifdef IMGUI_USE_NATIVE_RENDERER
			UTextureRenderTarget2D* AtlasTexture = TextureEntry->Texture;

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

			auto OneFrameResource = m_OneFrameResources.FindByKey(ResourceId);
			if (!OneFrameResource)
			{
				m_OneFrameResources.Emplace(ResourceId, FImGuiTextureResource(AtlasTexture->GetResource()));
			}
			else
			{
				OneFrameResource->Resource = FImGuiTextureResource{ AtlasTexture->GetResource() };
			}
#else
			static const FName FontTextureName = TEXT("ImGui_SharedFontTexture");
			static int32 FontTextureNameCounter = 0;

			TextureEntry->Brush = FSlateDynamicImageBrush::CreateWithImageData(FName(FontTextureName, ++FontTextureNameCounter),
				FVector2D(FontAtlasWidth, FontAtlasHeight),
				TArray((uint8*)TexData->GetPixelsAt(0, 0), FontAtlasWidth * FontAtlasHeight * TexData->BytesPerPixel));

			auto OneFrameResource = m_OneFrameResources.FindByKey(ResourceId);
			if (!OneFrameResource)
			{
				m_OneFrameResources.Emplace(ResourceId, FImGuiTextureResource(TextureEntry->Brush->GetRenderingResource()));
			}
			else
			{
				OneFrameResource->Resource = FImGuiTextureResource{ TextureEntry->Brush->GetRenderingResource() };
			}
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
	Params.Id = m_SharedFontAtlas->TexRef;
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

#ifdef USE_LOCAL_IMAGE_CACHE
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
			ImTextureID ResourceId = ToImTextureID(Proxy->Resource);
			// NOTE: when updating slate atlases `Proxy->Resource` can return null which gets patched later in the frame.
			// So make sure we get a unique `ResourceId` here in order to allow shader to override the UV data.
			if (!Proxy->Resource)
			{
				ResourceId = ToImTextureID(Proxy);
			}

			if (!m_OneFrameResources.Contains(ResourceId))
			{
				m_OneFrameResources.Emplace(ResourceId, FImGuiTextureResource(ResourceHandle));
			}

			Params.UV0 = ImVec2(Proxy->StartUV.X, Proxy->StartUV.Y);
			Params.UV1 = ImVec2(Proxy->StartUV.X + Proxy->SizeUV.X, Proxy->StartUV.Y + Proxy->SizeUV.Y);
			Params.Id = ResourceId;
		}
	}
	return Params;
}

FImGuiImageBindingParams UImGuiSubsystem::RegisterOneFrameResource(FSlateShaderResource* SlateShaderResource)
{
	FImGuiImageBindingParams Params = {};
	if (SlateShaderResource)
	{
		Params = RegisterOneFrameResource(ToImTextureID(SlateShaderResource), FImGuiTextureResource(SlateShaderResource), SlateShaderResource->GetWidth(), SlateShaderResource->GetHeight());
	}
	else
	{
		Params.Id = m_SharedFontAtlas->TexRef;
	}
	return Params;
}

#ifdef IMGUI_USE_NATIVE_RENDERER
FImGuiImageBindingParams UImGuiSubsystem::RegisterOneFrameResource(UTexture2D* Texture)
{
	FImGuiImageBindingParams Params = {};
	if (Texture)
	{
		Params = RegisterOneFrameResource(ToImTextureID(Texture), FImGuiTextureResource(Texture->GetResource()), Texture->GetSizeX(), Texture->GetSizeY());
	}
	else
	{
		Params.Id = m_SharedFontAtlas->TexRef;
	}
	return Params;
}
#endif

FImGuiImageBindingParams UImGuiSubsystem::RegisterOneFrameResource(ImTextureID ResourceId, FImGuiTextureResource&& Resource, float Width, float Height)
{
	if (!m_OneFrameResources.Contains(ResourceId))
	{
		m_OneFrameResources.Emplace(ResourceId, MoveTemp(Resource));
	}

	FImGuiImageBindingParams Params = {};
	Params.Size = ImVec2(Width, Height);
	Params.UV0 = ImVec2(0.f, 0.f);
	Params.UV1 = ImVec2(1.f, 1.f);
	Params.Id = ResourceId;

	return Params;
}
