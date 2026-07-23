// Copyright 2024-26 Amit Kumar Mehar. All Rights Reserved.

#include "ImGuiSubsystem.h"

#include "Misc/App.h"
#include "SImGuiWidgets.h"
#include "HAL/FileManager.h"
#include "Widgets/SWindow.h"
#include "RenderingThread.h"
#include "Misc/EngineVersion.h"
#include "Misc/ConfigCacheIni.h"
#include "Brushes/SlateDynamicImageBrush.h"
#include "Framework/Application/SlateApplication.h"

#if WITH_ENGINE
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

UImGuiSubsystem::FImGuiFontTextureEntry::~FImGuiFontTextureEntry()
{
#if WITH_ENGINE
	UTextureRenderTarget2D* Texture = Brush ? (UTextureRenderTarget2D*)Brush->GetResourceObject() : nullptr;
	if (::IsValid(Texture))
	{
		Texture->RemoveFromRoot();
	}
#endif
}

/*--------------------------------------------------------------------------------------------------------------------------*/

UImGuiSubsystem::FOnSubsystemInitialized UImGuiSubsystem::OnSubsystemInitialized = {};
UImGuiSubsystem* UImGuiSubsystem::SubsystemInstance = nullptr;
FSimpleMulticastDelegate UImGuiSubsystem::OnBeginImGuiFrame = {};
FSimpleMulticastDelegate UImGuiSubsystem::OnEndImGuiFrame = {};

const FString& UImGuiSubsystem::GetSaveDataConfigFilepath()
{
	// uses AppData folder to keep the widget data in sync b/w editor and packaged builds
	const TCHAR* UserSettingsDir = FPlatformProcess::UserSettingsDir();
	const FString EngineVersion = FEngineVersion::Current().ToString(EVersionComponent::Minor);
	static const FString ConfigFilepath =
		FPaths::Combine(UserSettingsDir, *FApp::GetEpicProductIdentifier(), EngineVersion, TEXT("Config/ImGui/ImGuiSaveData.ini"));
	return ConfigFilepath;
}

bool UImGuiSubsystem::ShouldCreateSubsystem() const
{
	return ShouldEnableImGui();
}

void UImGuiSubsystem::InitializeSubsystem()
{
	if (UImGuiSubsystem::ShouldEnableImGui())
	{
		UImGuiSubsystem::SubsystemInstance = NewObject<UImGuiSubsystem>();
		UImGuiSubsystem::SubsystemInstance->Initialize();
		UImGuiSubsystem::SubsystemInstance->AddToRoot();
	}
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

	// NOTE: Add reference to make sure ImGuiContext destructor cannot release the font atlas
	m_SharedFontAtlas = MakeShared<ImFontAtlas, ESPMode::NotThreadSafe>();
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

	// upto 8 shared font textures at a time (to account for repacking)
	// when spammed ImGui can cycle through a lot of atlases (most I encountered was 5)
	m_SharedFontAtlasTextures.SetNum(8);

	uint32 MagentaColor = FColor::Magenta.DWColor();
	m_MissingImageBrush = FSlateDynamicImageBrush::CreateWithImageData(FName("ImGui_MissingImage"), FVector2D(1, 1), TArray((uint8*)&MagentaColor, sizeof(MagentaColor)));
	static_assert(ImTextureID_Invalid == MissingImageTextureIndex);

	OnSubsystemInitialized.Broadcast(this);

	// first frame setup
	BeginImGuiFrame();

	FCoreDelegates::OnBeginFrame.AddUObject(this, &UImGuiSubsystem::BeginImGuiFrame);
	FCoreDelegates::OnEndFrame.AddUObject(this, &UImGuiSubsystem::EndImGuiFrame);
	FCoreDelegates::OnExit.AddLambda(
		[]() mutable
		{
			check(UImGuiSubsystem::SubsystemInstance->GetSharedFontAtlas()->RefCount == 1);
			UImGuiSubsystem::SubsystemInstance->Deinitialize();
			UImGuiSubsystem::SubsystemInstance->RemoveFromRoot();
			UImGuiSubsystem::SubsystemInstance = nullptr;
		});
}

void UImGuiSubsystem::Deinitialize()
{
	m_SharedFontAtlas = nullptr;
	m_SharedFontAtlasTextures.Reset();
}

bool UImGuiSubsystem::ShouldEnableImGui()
{
	if (IsRunningCommandlet())
	{
		return false;
	}
	return true;
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

void UImGuiSubsystem::RegisterMainMenuWidget(
	const UWorld* World, const char* WidgetPath, const char* WidgetToolTip, const FSlateBrush* WidgetIcon,
	FOnTickImGuiWidgetDelegate TickDelegate, EImGuiMainMenuWidgetFlags WidgetFlags) const
{
	// defined in ImGuiRuntimeModule.cpp
	extern void RegisterMainMenuWidgetForWorld(
		const UWorld* World, const char* WidgetPath, const char* WidgetToolTip, const FSlateBrush * WidgetIcon,
		FOnTickImGuiWidgetDelegate TickDelegate, EImGuiMainMenuWidgetFlags WidgetFlags);

	RegisterMainMenuWidgetForWorld(World, WidgetPath, WidgetToolTip, WidgetIcon, TickDelegate, WidgetFlags);
}

void UImGuiSubsystem::UnregisterMainMenuWidget(const UWorld* World, const char* WidgetPath) const
{
	// defined in ImGuiRuntimeModule.cpp
	extern void UnregisterMainMenuWidgetForWorld(const UWorld* World, const char* WidgetPath);

	UnregisterMainMenuWidgetForWorld(World, WidgetPath);
}

bool* UImGuiSubsystem::GetMainMenuWidgetActiveState(const UWorld* World, const char* WidgetPath) const
{
	// defined in ImGuiRuntimeModule.cpp
	extern bool* GetMainMenuWidgetActiveStateForWorld(const UWorld* World, const char* WidgetPath);

	return GetMainMenuWidgetActiveStateForWorld(World, WidgetPath);
}

FImGuiTickContext* UImGuiSubsystem::GetWidgetTickContext(const UWorld* World) const
{
	// defined in ImGuiRuntimeModule.cpp
	extern FImGuiTickContext* GetWidgetTickContextForWorld(const UWorld* World);

	return GetWidgetTickContextForWorld(World);
}
#endif //#if WITH_ENGINE

void UImGuiSubsystem::BeginImGuiFrame()
{
	m_OneFrameResources.Reset();
	m_OneFrameSlateBrushes.Reset();

	// queue font updates
	ImFontAtlasUpdateNewFrame(m_SharedFontAtlas.Get(), ++m_FontAtlasBuilderFrameCount, true);

	m_MissingImageParams = RegisterOneFrameResource(m_MissingImageBrush.Get());
	check(MissingImageTextureIndex == ImGuiIDToIndex(m_MissingImageParams.Id));

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

int32 UImGuiSubsystem::AllocateFontAtlasTexture(int32 SizeX, int32 SizeY)
{
	static const FName FontTextureName = TEXT("ImGui_SharedFontTexture");

	for (int32 TextureIndex = 0; TextureIndex < m_SharedFontAtlasTextures.Num(); ++TextureIndex)
	{
		if (!m_SharedFontAtlasTextures[TextureIndex].bInUse)
		{
#if WITH_ENGINE
			if (!m_SharedFontAtlasTextures[TextureIndex].Brush)
			{
				m_SharedFontAtlasTextures[TextureIndex].Brush = MakeShared<FSlateBrush>();
			}

			UTextureRenderTarget2D* Texture = (UTextureRenderTarget2D*)m_SharedFontAtlasTextures[TextureIndex].Brush->GetResourceObject();
			if (!Texture)
			{
				Texture = NewObject<UTextureRenderTarget2D>(this, FName(FontTextureName, TextureIndex + 1));
				Texture->Filter = TextureFilter::TF_Bilinear;
				Texture->RenderTargetFormat = ETextureRenderTargetFormat::RTF_RGBA8;
				Texture->OverrideFormat = PF_R8G8B8A8;
				Texture->ClearColor = FLinearColor(0, 0, 0, 0);
				Texture->bNoFastClear = true;
				Texture->InitAutoFormat(SizeX, SizeY);
				Texture->UpdateResourceImmediate(/*bClearRenderTarget=*/false);

				Texture->AddToRoot();
				m_SharedFontAtlasTextures[TextureIndex].Brush->SetResourceObject(Texture);
				m_OneFrameResources[FontAtlasTextureStartIndex + TextureIndex] = FImGuiTextureResource{ m_SharedFontAtlasTextures[TextureIndex].Brush->GetRenderingResource() };
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

void UImGuiSubsystem::UpdateFontAtlasTexture(ImTextureData* TexData)
{
	if (TexData->Status == ImTextureStatus_WantCreate || TexData->Status == ImTextureStatus_WantUpdates)
	{
		const int32 FontAtlasWidth = TexData->Width;
		const int32 FontAtlasHeight = TexData->Height;

		if (TexData->Status == ImTextureStatus_WantCreate)
		{
			check(TexData->BytesPerPixel == GPixelFormats[PF_R8G8B8A8].BlockBytes);
			TexData->SetTexID(AllocateFontAtlasTexture(FontAtlasWidth, FontAtlasHeight) + FontAtlasTextureStartIndex);
		}

#if WITH_ENGINE
		UTextureRenderTarget2D* AtlasTexture = (UTextureRenderTarget2D*)m_SharedFontAtlasTextures[ImGuiIDToIndex(TexData->GetTexID()) - FontAtlasTextureStartIndex].Brush->GetResourceObject();

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

		int32 TextureIndex = (ImGuiIDToIndex(TexData->GetTexID()) - FontAtlasTextureStartIndex);
		m_SharedFontAtlasTextures[TextureIndex].Brush = FSlateDynamicImageBrush::CreateWithImageData(FName(FontTextureName, ++FontTextureNameCounter),
			FVector2D(FontAtlasWidth, FontAtlasHeight),
			TArray((uint8*)TexData->GetPixelsAt(0, 0), FontAtlasWidth * FontAtlasHeight * TexData->BytesPerPixel));
		m_OneFrameResources[FontAtlasTextureStartIndex + TextureIndex] = FImGuiTextureResource{ m_SharedFontAtlasTextures[TextureIndex].Brush->GetRenderingResource() };
#endif

		TexData->SetStatus(ImTextureStatus_OK);
	}
	else if (TexData->Status == ImTextureStatus_WantDestroy && TexData->UnusedFrames > 1)
	{
		// latest shared font texture data should never be destroyed!
		check(TexData != m_SharedFontAtlas->TexData);

		ReleaseFontAtlasTexture(ImGuiIDToIndex(TexData->GetTexID()) - FontAtlasTextureStartIndex);

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
	if (SlateBrush && FApp::CanEverRender())
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
			Params.Id = IndexToImGuiID(ResourceHandleIndex);
		}
	}
	return Params;
}

FImGuiImageBindingParams UImGuiSubsystem::RegisterOneFrameResource(const FSlateBrush* SlateBrush)
{
	if (!SlateBrush)
	{
		return {};
	}
	return RegisterOneFrameResource(SlateBrush, SlateBrush->GetImageSize(), 1.0f);
}

FImGuiImageBindingParams UImGuiSubsystem::RegisterOneFrameResource(FSlateShaderResource* SlateShaderResource)
{
	FImGuiImageBindingParams Params = {};
	if (SlateShaderResource)
	{
		uint32 ResourceHandleIndex = m_OneFrameResources.IndexOfByPredicate([&](const auto& TextureResource) { return TextureResource.GetSlateShaderResource() == SlateShaderResource; });
		if (ResourceHandleIndex == INDEX_NONE)
		{
			ResourceHandleIndex = m_OneFrameResources.Emplace(SlateShaderResource);
		}

		Params.Size = ImVec2(SlateShaderResource->GetWidth(), SlateShaderResource->GetHeight());
		Params.UV0 = ImVec2(0.f, 0.f);
		Params.UV1 = ImVec2(1.f, 1.f);
		Params.Id = IndexToImGuiID(ResourceHandleIndex);
	}
	return Params;
}
