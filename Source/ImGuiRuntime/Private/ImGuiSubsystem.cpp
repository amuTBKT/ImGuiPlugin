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

/*--------------------------------------------------------------------------------------------------------------------------*/

FSlateShaderResource* FImGuiTextureResource::GetSlateShaderResource() const
{
	if (UnderlyingResource.IsType<FSlateResourceHandle>())
	{
		const FSlateResourceHandle& ResourceHandle = UnderlyingResource.Get<FSlateResourceHandle>();
		const FSlateShaderResourceProxy* ResourcProxy = ResourceHandle.GetResourceProxy();
		return ResourcProxy ? ResourcProxy->Resource : nullptr;
	}
	else if (UnderlyingResource.IsType<FSlateShaderResource*>())
	{
		return (FSlateShaderResource*)UnderlyingResource.Get<FSlateShaderResource*>();
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

	// NOTE: Add reference to make sure ImGuiContext destructor cannot release font atlas
	m_SharedFontAtlas = MakeShared<ImFontAtlas, ESPMode::NotThreadSafe>();
	m_SharedFontAtlas->RefCount = 1;
	m_SharedFontAtlas->AddFontDefault();

	// shared font texture
	m_SharedFontTexture = NewObject<UTextureRenderTarget2D>(this, FName("ImGui_SharedFontTexture"));
	m_SharedFontTexture->Filter = TextureFilter::TF_Bilinear;
	m_SharedFontTexture->RenderTargetFormat = ETextureRenderTargetFormat::RTF_RGBA8;
	m_SharedFontTexture->ClearColor = FLinearColor(0.f, 0.f, 0.f, 0.f);
	m_SharedFontTexture->InitAutoFormat(1, 1);
	m_SharedFontTexture->UpdateResourceImmediate(true);

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
	
	m_SharedFontSlateBrush.SetResourceObject(m_SharedFontTexture);
	m_MissingImageSlateBrush.SetResourceObject(m_MissingImageTexture);

	FCoreDelegates::OnBeginFrame.AddUObject(this, &UImGuiSubsystem::OnBeginFrame);
	
	// Need to ensure shared font atlas is released after all slate windows have exited
	// Note: subsystem is deinitialized before slate so copy the pointer for callback
	FCoreDelegates::OnPreExit.AddLambda(
		[FontAtlasToDestroy=m_SharedFontAtlas]() mutable
		{
			ensure(FontAtlasToDestroy->RefCount == 1);
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
		.IsTopmostWindow(true)
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
		.ConfigFileName(TCHAR_TO_ANSI(*WindowName))
		.bUseOpaqueBackground(true);

	Window->SetContent(ImGuiWindow.ToSharedRef());

	return Window;
}

void UImGuiSubsystem::OnBeginFrame()
{
	m_OneFrameResources.Reset();
	m_CreatedSlateBrushes.Reset();

	// queue font updates
	ImFontAtlasUpdateNewFrame(m_SharedFontAtlas.Get(), FontAtlasBuilderFrameCount++, true);

	m_MissingImageParams = RegisterOneFrameResource(&m_MissingImageSlateBrush);
	m_SharedFontImageParams = RegisterOneFrameResource(&m_SharedFontSlateBrush);
	check(MissingImageTexID == m_MissingImageParams.Id);
	check(SharedFontTexID == m_SharedFontImageParams.Id);

	GCaptureNextGpuFrames = FMath::Max(0, GCaptureNextGpuFrames - 1);
}

void UImGuiSubsystem::UpdateTextureData(ImTextureData* TexData) const
{
	// TODO: this function really only cares about the shared font texture atm
	if (TexData->Status == ImTextureStatus_WantDestroy && TexData->UnusedFrames > 2)
	{
		// latest shared font texture data should never be destroyed!
		check(TexData != m_SharedFontAtlas->TexData);

		TexData->SetStatus(ImTextureStatus_Destroyed);
		TexData->SetTexID(ImTextureID_Invalid);
	}
	else if (TexData == m_SharedFontAtlas->TexData)
	{
		check(IsValid(m_SharedFontTexture));

		const int32 FontAtlasWidth = TexData->Width;
		const int32 FontAtlasHeight = TexData->Height;
		const int32 BytesPerPixel = TexData->BytesPerPixel;
		check(BytesPerPixel == GPixelFormats[PF_R8G8B8A8].BlockBytes);

		if (TexData->Status == ImTextureStatus_WantCreate || TexData->Status == ImTextureStatus_WantUpdates)
		{
			bool bReuploadTexture = (TexData->Status == ImTextureStatus_WantCreate);
			if (m_SharedFontTexture->SizeX != FontAtlasWidth || m_SharedFontTexture->SizeY != FontAtlasHeight)
			{
				m_SharedFontTexture->ResizeTarget(FontAtlasWidth, FontAtlasHeight);
				bReuploadTexture = true;
			}

			const ImTextureRect UpdateRect = bReuploadTexture ? ImTextureRect(0, 0, TexData->Width, TexData->Height) : TexData->UpdateRect;
			ENQUEUE_RENDER_COMMAND(UpdateFontTexture)(
				[this,
				SrcPitch=TexData->GetPitch(),
				SrcData = (uint8*)TexData->GetPixelsAt(UpdateRect.x, UpdateRect.y),
				UpdateRegion = FUpdateTextureRegion2D(UpdateRect.x, UpdateRect.y, 0, 0, UpdateRect.w, UpdateRect.h),
				TexResource=m_SharedFontTexture->GameThread_GetRenderTargetResource()](FRHICommandListImmediate& RHICmdList)
				{
					RHICmdList.UpdateTexture2D(TexResource->GetTexture2DRHI(), 0, UpdateRegion, SrcPitch, SrcData);
				});

			TexData->SetStatus(ImTextureStatus_OK);
			TexData->SetTexID(SharedFontTexID);
		}
		else
		{
			check(TexData->Status == ImTextureStatus_OK);
		}
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
	
	uint32 ResourceHandleIndex = m_OneFrameResources.IndexOfByPredicate([Proxy](const auto& TextureResource) { return TextureResource.GetSlateShaderResource() == Proxy->Resource; });
	if (ResourceHandleIndex == INDEX_NONE)
	{
		ResourceHandleIndex = m_OneFrameResources.Add(FImGuiTextureResource(ResourceHandle));
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

	FSlateBrush& NewBrush = m_CreatedSlateBrushes.AddDefaulted_GetRef();
	NewBrush.SetResourceObject(Texture);

	return RegisterOneFrameResource(&NewBrush);
}

FImGuiImageBindingParams UImGuiSubsystem::RegisterOneFrameResource(FSlateShaderResource* SlateShaderResource)
{
	if (!SlateShaderResource)
	{
		return {};
	}

	const uint32 ResourceHandleIndex = m_OneFrameResources.Add(FImGuiTextureResource(SlateShaderResource));

	FImGuiImageBindingParams Params = {};
	Params.Size = ImVec2(SlateShaderResource->GetWidth(), SlateShaderResource->GetHeight());
	Params.UV0 = ImVec2(0.f, 0.f);
	Params.UV1 = ImVec2(1.f, 1.f);
	Params.Id = IndexToImGuiID(ResourceHandleIndex);

	return Params;
}
