// Copyright 2024 Amit Kumar Mehar. All Rights Reserved.

#include "ImGuiSubsystem.h"

#include "Engine.h"
#include "Misc/App.h"
#include "SImGuiWidgets.h"
#include "HAL/FileManager.h"
#include "TextureResource.h"
#include "Widgets/SWindow.h"
#include "Engine/Texture2D.h"
#include "Styling/AppStyle.h"
#include "ImGuiRemoteConnection.h"
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
		m_IniFilePath = FAnsiString(FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("ImGui"), TEXT("imgui.ini")));

		IFileManager::Get().MakeDirectory(ANSI_TO_TCHAR(*m_IniDirectoryPath), true);
	}

	m_DefaultFontAtlas.Build();

	auto CreateTextureRGBA8 = [](FName DebugName, int32 Width, int32 Height, const uint8* ImageData) -> UTexture2D*
	{
		UTexture2D* Texture = UTexture2D::CreateTransient(Width, Height, PF_R8G8B8A8, DebugName);
		Texture->CompressionSettings = TextureCompressionSettings::TC_Default;
		Texture->SRGB = false;
		Texture->LODGroup = TEXTUREGROUP_UI;
		Texture->Filter = TextureFilter::TF_Bilinear;
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
	m_DefaultFontAtlas.GetTexDataAsRGBA32(&FontAtlasData, &FontAtlasWidth, &FontAtlasHeight, &BytesPerPixel);
	check(BytesPerPixel == GPixelFormats[PF_R8G8B8A8].BlockBytes && FontAtlasData);

	m_DefaultFontTexture = CreateTextureRGBA8(FName("ImGui_DefaultFontAtlas"), FontAtlasWidth, FontAtlasHeight, (uint8_t*)FontAtlasData);

	// 1x1 magenta texture
	const int32 MissingImageSize = 1;
	const uint32 MissingPixelData = FColor::Magenta.DWColor();
	m_MissingImageTexture = CreateTextureRGBA8(FName("ImGui_MissingImage"), MissingImageSize, MissingImageSize, (uint8_t*)&MissingPixelData);

	m_DefaultFontSlateBrush.SetResourceObject(m_DefaultFontTexture);
	m_MissingImageSlateBrush.SetResourceObject(m_MissingImageTexture);

	FCoreDelegates::OnBeginFrame.AddUObject(this, &UImGuiSubsystem::OnBeginFrame);
	FCoreDelegates::OnEndFrame.AddUObject(this, &UImGuiSubsystem::OnEndFrame);

	OnSubsystemInitializedDelegate.Broadcast(this);

	// first frame setup
	OnBeginFrame();

#if 0 //TODO: enable this at some point
	RemoteConnection = new FImGuiRemoteConnection();
	RemoteConnection->OnConnected = FSimpleDelegate::CreateUObject(this, &UImGuiSubsystem::OnRemoteConnectionEstablished);
	RemoteConnection->OnDisconnected = FSimpleDelegate::CreateUObject(this, &UImGuiSubsystem::OnRemoteConnectionClosed);
	RemoteConnection->Connect(TEXT("127.0.0.1"), 7002);
#endif
}

void UImGuiSubsystem::Deinitialize()
{
	if (RemoteConnection)
	{
		delete RemoteConnection;
		RemoteConnection = nullptr;
	}

	Super::Deinitialize();
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
	m_OneFrameResources.Reset();
	m_CreatedSlateBrushes.Reset();
	
	m_MissingImageParams = RegisterOneFrameResource(&m_MissingImageSlateBrush);
	m_DefaultFontImageParams = RegisterOneFrameResource(&m_DefaultFontSlateBrush);

	m_DefaultFontAtlas.TexID = GetDefaultFontTextureID();

	GCaptureNextGpuFrames = FMath::Max(0, GCaptureNextGpuFrames - 1);
}

void UImGuiSubsystem::OnEndFrame()
{
	TickRemoteConnection();
}

bool UImGuiSubsystem::CaptureGpuFrame() const
{
	return GCaptureNextGpuFrames > 0;
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
	
	uint32 ResourceHandleIndex = m_OneFrameResources.IndexOfByPredicate([Proxy](const auto& TextureResource) { return TextureResource.GetSlateShaderResource() == Proxy->Resource; });
	if (ResourceHandleIndex == INDEX_NONE)
	{
		ResourceHandleIndex = m_OneFrameResources.Add(FImGuiTextureResource(ResourceHandle));
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

void UImGuiSubsystem::OnRemoteConnectionEstablished()
{
	if (!ensure(RemoteConnection && RemoteConnection->IsConnected()))
	{
		return;
	}

	// register persistent textures

	{
		const int32 MissingImageSize = 1;
		const uint32 MissingPixelData = FColor::Magenta.DWColor();
		RemoteConnection->SendTextureData(MissingImageSize, MissingImageSize, sizeof(uint32), (uint8*)&MissingPixelData);
	}
	{
		int32 FontAtlasWidth, FontAtlasHeight, BytesPerPixel;
		unsigned char* FontAtlasData;
		GetDefaultImGuiFontAtlas()->GetTexDataAsRGBA32(&FontAtlasData, &FontAtlasWidth, &FontAtlasHeight, &BytesPerPixel);
		RemoteConnection->SendTextureData(FontAtlasWidth, FontAtlasHeight, BytesPerPixel, (uint8*)FontAtlasData);
	}

	for (auto Itr = RegisteredImGuiWidgets.CreateIterator(); Itr; ++Itr)
	{
		const auto& WidgetWeakPtr = *Itr;
		TSharedPtr<SImGuiWidgetBase> WidgetPtr = WidgetWeakPtr.Pin();
		if (!WidgetPtr.IsValid())
		{
			Itr.RemoveCurrentSwap();
			continue;
		}

		WidgetPtr->SetEnabled(false);
	}
}

void UImGuiSubsystem::OnRemoteConnectionClosed()
{
	for (auto Itr = RegisteredImGuiWidgets.CreateIterator(); Itr; ++Itr)
	{
		const auto& WidgetWeakPtr = *Itr;
		TSharedPtr<SImGuiWidgetBase> WidgetPtr = WidgetWeakPtr.Pin();
		if (!WidgetPtr.IsValid())
		{
			Itr.RemoveCurrentSwap();
			continue;
		}

		WidgetPtr->SetEnabled(true);
	}
}

void UImGuiSubsystem::TickRemoteConnection()
{
	DECLARE_SCOPE_CYCLE_COUNTER(TEXT("Remote - Tick Connection"), STAT_ImGuiRemote_TickConnection, STATGROUP_ImGui);

	if (!RemoteConnection)
	{
		return;
	}

	RemoteConnection->NewFrame();

	if (!RegisteredImGuiWidgets.IsEmpty() && RemoteConnection->CanDrawWidgets())
	{
		TArray<ImDrawList*> RemoteDrawLists;
		SCOPED_NAMED_EVENT(ImGuiRemote_TickWidgets, FColor::Orange);

		const float DeltaTime = FSlateApplication::Get().GetDeltaTime();

		for (auto Itr = RegisteredImGuiWidgets.CreateIterator(); Itr; ++Itr)
		{
			const auto& WidgetWeakPtr = *Itr;
			TSharedPtr<SImGuiWidgetBase> WidgetPtr = WidgetWeakPtr.Pin();
			if (!WidgetPtr.IsValid())
			{
				Itr.RemoveCurrentSwap();
				continue;
			}

			const ImDrawData* DrawData = WidgetPtr->TickForRemoteClient(*RemoteConnection, DeltaTime);

			{
				DECLARE_SCOPE_CYCLE_COUNTER(TEXT("Remote - Send Draw Data"), STAT_ImGuiRemote_SendDrawData, STATGROUP_ImGui);

				RemoteConnection->SendDrawData(DrawData, WidgetPtr->GetMouseCursor());
			}

			break;
		}
	}
}

bool UImGuiSubsystem::IsRemoteConnectionActive() const
{
	return RemoteConnection && RemoteConnection->IsConnected();
}

void UImGuiSubsystem::RegisterWidgetForRemoteClient(TSharedPtr<SImGuiWidgetBase> Widget)
{
	if (IsRemoteConnectionActive())
	{
		Widget->SetEnabled(false);
	}
	RegisteredImGuiWidgets.AddUnique(Widget);
}
