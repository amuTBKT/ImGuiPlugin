// Copyright 2024-25 Amit Kumar Mehar. All Rights Reserved.

#include "ImGuiSubsystem.h"

#include "Engine.h"
#include "Misc/App.h"
#include "SImGuiWidgets.h"
#include "HAL/FileManager.h"
#include "Widgets/SWindow.h"
#include "TextureResource.h"
#include "Engine/Texture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Framework/Application/SlateApplication.h"

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

UImGuiSubsystem::FOnSubsystemInitialized UImGuiSubsystem::OnSubsystemInitializedDelegate = {};

bool UImGuiSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	if (!Super::ShouldCreateSubsystem(Outer))
	{
		return false;
	}

	return ShouldEnableImGui();
}

void UImGuiSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// setup ini file path and directory
	{
		m_IniDirectoryPath = FAnsiString(FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("ImGui")));
		m_IniFilePath = FAnsiString(FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("ImGui"), TEXT("ImGui.ini")));

		IFileManager::Get().MakeDirectory(ANSI_TO_TCHAR(*m_IniDirectoryPath), true);
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
	m_SharedFontAtlas->AddFontDefaultBitmap();

	// upto 8 shared font textures at a time (to account for repacking)
	// when spammed ImGui can cycle through a lot of atlases (most I encountered was 5)
	m_SharedFontAtlasTextures.SetNum(8);

	// 1x1 magenta texture
	const uint32 MissingPixelData = FColor::Magenta.DWColor();
	m_MissingImageTexture = UTexture2D::CreateTransient(1, 1, PF_R8G8B8A8, FName("ImGui_MissingImage"));
	m_MissingImageTexture->CompressionSettings = TextureCompressionSettings::TC_Default;
	m_MissingImageTexture->SRGB = false;
	m_MissingImageTexture->LODGroup = TEXTUREGROUP_UI;
	m_MissingImageTexture->Filter = TextureFilter::TF_Bilinear;
	m_MissingImageTexture->AddressX = TextureAddress::TA_Clamp;
	m_MissingImageTexture->AddressY = TextureAddress::TA_Clamp;
	if (uint8* MipData = static_cast<uint8*>(m_MissingImageTexture->GetPlatformData()->Mips[0].BulkData.Lock(LOCK_READ_WRITE)))
	{
		FMemory::Memcpy(MipData, (uint8*)&MissingPixelData, sizeof(uint32));
		m_MissingImageTexture->GetPlatformData()->Mips[0].BulkData.Unlock();
	}
	m_MissingImageTexture->UpdateResource();
	m_MissingImageSlateBrush.SetResourceObject(m_MissingImageTexture);
	static_assert(ImTextureID_Invalid == MissingImageTextureIndex);

	FCoreDelegates::OnBeginFrame.AddUObject(this, &UImGuiSubsystem::OnBeginFrame);
	
	// Need to ensure shared font atlas is released after all slate windows have exited
	// Note: Subsystem is deinitialized before slate so copy the pointer for callback
	FCoreDelegates::OnExit.AddLambda(
		[FontAtlasToDestroy=m_SharedFontAtlas]() mutable
		{
			check(FontAtlasToDestroy->RefCount == 1);
			FontAtlasToDestroy->RefCount = 0;
			FontAtlasToDestroy = nullptr;
		});

	OnSubsystemInitializedDelegate.Broadcast(this);

	// first frame setup
	OnBeginFrame();
}

void UImGuiSubsystem::Deinitialize()
{
	Super::Deinitialize();

	m_SharedFontAtlas = nullptr;
}

bool UImGuiSubsystem::ShouldEnableImGui()
{
	if (!FApp::CanEverRender() || IsRunningCommandlet())
	{
		return false;
	}
	return true;
}

UImGuiSubsystem* UImGuiSubsystem::Get()
{
	if (GEngine)
	{
		return GEngine->GetEngineSubsystem<UImGuiSubsystem>();
	}
	return nullptr;
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

void UImGuiSubsystem::OnBeginFrame()
{
	m_OneFrameResources.Reset();
	m_OneFrameSlateBrushes.Reset();

	// queue font updates
	ImFontAtlasUpdateNewFrame(m_SharedFontAtlas.Get(), m_FontAtlasBuilderFrameCount++, true);

	m_MissingImageParams = RegisterOneFrameResource(&m_MissingImageSlateBrush);
	check(MissingImageTextureIndex == ImGuiIDToIndex(m_MissingImageParams.Id));

	// register all font altases
	for (const FImGuiFontTextureEntry& TextureEntry : m_SharedFontAtlasTextures)
	{
		if (TextureEntry.Texture)
		{
			RegisterOneFrameResource(&TextureEntry.SlateBrush);
		}
		else
		{
			// queue an empty slot which may get populated by UpdateFontAtlasTexture
			m_OneFrameResources.Add(FImGuiTextureResource{nullptr});
		}
	}

	GCaptureNextGpuFrames = FMath::Max(0, GCaptureNextGpuFrames - 1);
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
			UTextureRenderTarget2D* Texture = m_SharedFontAtlasTextures[TextureIndex].Texture;
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

				m_SharedFontAtlasTextures[TextureIndex].Texture = Texture;
				m_SharedFontAtlasTextures[TextureIndex].SlateBrush.SetResourceObject(Texture);
				
				m_OneFrameResources[FontAtlasTextureStartIndex + TextureIndex] = FImGuiTextureResource{ m_SharedFontAtlasTextures[TextureIndex].SlateBrush.GetRenderingResource() };
			}
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
	//m_SharedFontAtlasTextures[Index].Texture = nullptr;
	//m_SharedFontAtlasTextures[Index].SlateBrush = {};
	m_SharedFontAtlasTextures[Index].bInUse = false;
}

void UImGuiSubsystem::UpdateFontAtlasTexture(ImTextureData* TexData)
{
	if (TexData->Status == ImTextureStatus_WantCreate || TexData->Status == ImTextureStatus_WantUpdates)
	{
		const int32 FontAtlasWidth = TexData->Width;
		const int32 FontAtlasHeight = TexData->Height;
		const int32 BytesPerPixel = TexData->BytesPerPixel;
		check(BytesPerPixel == GPixelFormats[PF_R8G8B8A8].BlockBytes);

		if (TexData->Status == ImTextureStatus_WantCreate)
		{
			TexData->SetTexID(AllocateFontAtlasTexture(FontAtlasWidth, FontAtlasHeight) + FontAtlasTextureStartIndex);
		}

		UTextureRenderTarget2D* AtlasTexture = m_SharedFontAtlasTextures[ImGuiIDToIndex(TexData->GetTexID()) - FontAtlasTextureStartIndex].Texture;

		bool bReuploadTexture = (TexData->Status == ImTextureStatus_WantCreate);
		if (AtlasTexture->SizeX != FontAtlasWidth || AtlasTexture->SizeY != FontAtlasHeight)
		{
			AtlasTexture->ResizeTarget(FontAtlasWidth, FontAtlasHeight);
			bReuploadTexture = true;
		}

		const ImTextureRect UpdateRect = bReuploadTexture ? ImTextureRect(0, 0, FontAtlasWidth, FontAtlasHeight) : TexData->UpdateRect;
		ENQUEUE_RENDER_COMMAND(UpdateFontTexture)(
			[this,
			SrcPitch=TexData->GetPitch(),
			SrcData=(uint8*)TexData->GetPixelsAt(UpdateRect.x, UpdateRect.y),
			UpdateRegion=FUpdateTextureRegion2D(UpdateRect.x, UpdateRect.y, 0, 0, UpdateRect.w, UpdateRect.h),
			TexResource=AtlasTexture->GameThread_GetRenderTargetResource()](FRHICommandListImmediate& RHICmdList)
			{
				RHICmdList.Transition(FRHITransitionInfo(TexResource->GetTexture2DRHI(), ERHIAccess::Unknown, ERHIAccess::CopyDest));
				RHICmdList.UpdateTexture2D(TexResource->GetTexture2DRHI(), 0, UpdateRegion, SrcPitch, SrcData);
				RHICmdList.Transition(FRHITransitionInfo(TexResource->GetTexture2DRHI(), ERHIAccess::CopyDest, ERHIAccess::SRVMask));
			});

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
	
	uint32 ResourceHandleIndex;
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

	const FVector2f StartUV = Proxy->StartUV;
	const FVector2f SizeUV = Proxy->SizeUV;

	FImGuiImageBindingParams Params = {};
	Params.Size = ImVec2(LocalSize.X, LocalSize.Y) * DrawScale;
	Params.UV0 = ImVec2(StartUV.X, StartUV.Y);
	Params.UV1 = ImVec2(StartUV.X + SizeUV.X, StartUV.Y + SizeUV.Y);
	Params.Id = IndexToImGuiID(ResourceHandleIndex);

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

FImGuiImageBindingParams UImGuiSubsystem::RegisterOneFrameResource(FSlateShaderResource* SlateShaderResource)
{
	if (!SlateShaderResource)
	{
		return {};
	}

	uint32 ResourceHandleIndex = m_OneFrameResources.IndexOfByPredicate([&](const auto& TextureResource) { return TextureResource.GetSlateShaderResource() == SlateShaderResource; });
	if (ResourceHandleIndex == INDEX_NONE)
	{
		ResourceHandleIndex = m_OneFrameResources.Emplace(SlateShaderResource);
	}

	FImGuiImageBindingParams Params = {};
	Params.Size = ImVec2(SlateShaderResource->GetWidth(), SlateShaderResource->GetHeight());
	Params.UV0 = ImVec2(0.f, 0.f);
	Params.UV1 = ImVec2(1.f, 1.f);
	Params.Id = IndexToImGuiID(ResourceHandleIndex);

	return Params;
}
