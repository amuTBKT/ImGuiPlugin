// Copyright 2024 Amit Kumar Mehar. All Rights Reserved.

#include "ImGuiSubsystem.h"

#include "Engine.h"
#include "Misc/App.h"
#include "SImGuiWidgets.h"
#include "TextureResource.h"
#include "Widgets/SWindow.h"
#include "Engine/Texture2D.h"
#include "Framework/Application/SlateApplication.h"

static int32 CaptureNextGpuFrames = 0;
static FAutoConsoleVariableRef CVarRenderCaptureNextImGuiFrame(
	TEXT("imgui.CaptureGpuFrames"),
	CaptureNextGpuFrames,
	TEXT("Enable capturing of ImGui rendering for the next N draws"));

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

	DefaultFontAtlas = MakeUnique<ImFontAtlas>();
	DefaultFontAtlas->Build();

	auto CreateTextureRGBA8 = [](FName DebugName, int32 Width, int32 Height, uint8* ImageData)
	{
		UTexture2D* Texture = UTexture2D::CreateTransient(Width, Height, PF_R8G8B8A8, DebugName);
		// compression and mip settings
		Texture->CompressionSettings = TextureCompressionSettings::TC_Default;
		Texture->LODGroup = TEXTUREGROUP_UI;
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

	FCoreDelegates::OnBeginFrame.AddUObject(this, &UImGuiSubsystem::OnBeginFrame);

	OnSubsystemInitializedDelegate.Broadcast(this);

	// first frame setup
	OnBeginFrame();
}

void UImGuiSubsystem::Deinitialize()
{
	if (DefaultFontTexture)
	{
		DefaultFontTexture->RemoveFromRoot();
	}

	if (MissingImageTexture)
	{
		MissingImageTexture->RemoveFromRoot();
	}
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

TSharedPtr<SWindow> UImGuiSubsystem::CreateWindow(const FString& WindowName, const FVector2D& WindowSize, FOnTickImGuiWidgetDelegate TickDelegate)
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

void UImGuiSubsystem::OnBeginFrame()
{
	OneFrameResources.Reset();
	CreatedSlateBrushes.Reset();
	
	m_MissingImageParams = RegisterOneFrameResource(&m_MissingImageSlateBrush);
	m_DefaultFontImageParams = RegisterOneFrameResource(&m_DefaultFontSlateBrush);

	DefaultFontAtlas->TexID = GetDefaultFontTextureID();

	CaptureNextGpuFrames = FMath::Max(0, CaptureNextGpuFrames - 1);
}

bool UImGuiSubsystem::CaptureGpuFrame() const
{
	return CaptureNextGpuFrames > 0;
}

FImGuiImageBindingParams UImGuiSubsystem::RegisterOneFrameResource(const FSlateBrush* SlateBrush, FVector2D LocalSize, float DrawScale)
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
	
	uint32 ResourceHandleIndex = OneFrameResources.IndexOfByPredicate([Proxy](const auto& TextureResource) { return TextureResource.GetSlateShaderResource() == Proxy->Resource; });
	if (ResourceHandleIndex == INDEX_NONE)
	{
		ResourceHandleIndex = OneFrameResources.Add(FImGuiTextureResource(ResourceHandle));
	}

	const FVector2f StartUV = Proxy->StartUV;
	const FVector2f SizeUV = Proxy->SizeUV;

	FImGuiImageBindingParams Params = {};
	Params.Size = ImVec2(LocalSize.X, LocalSize.Y);
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

	FSlateBrush& NewBrush = CreatedSlateBrushes.AddDefaulted_GetRef();
	NewBrush.SetResourceObject(Texture);

	return RegisterOneFrameResource(&NewBrush);
}

FImGuiImageBindingParams UImGuiSubsystem::RegisterOneFrameResource(FSlateShaderResource* SlateShaderResource)
{
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
		const FSlateShaderResourceProxy* ResourcProxy = ResourceHandle.GetResourceProxy();		
		return ResourcProxy ? ResourcProxy->Resource : nullptr;
	}
	else if (UnderlyingResource.IsType<FSlateShaderResource*>())
	{
		return (FSlateShaderResource*)UnderlyingResource.Get<FSlateShaderResource*>();
	}
	return nullptr;
}
