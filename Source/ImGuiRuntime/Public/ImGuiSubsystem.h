// Copyright 2024-25 Amit Kumar Mehar. All Rights Reserved.

#pragma once

#include "ImGuiPluginTypes.h"
#include "Styling/SlateBrush.h"
#include "ImGuiPluginDelegates.h"
#include "Containers/AnsiString.h"
#include "Subsystems/EngineSubsystem.h"
#include "Textures/SlateShaderResource.h"

#include "ImGuiSubsystem.generated.h"

class SWindow;
class UTexture2D;
class FSlateShaderResource;
class UTextureRenderTarget2D;
class FSlateShaderResourceProxy;

DECLARE_STATS_GROUP(TEXT("ImGui"), STATGROUP_ImGui, STATCAT_Advanced);

class FImGuiTextureResource
{
public:
	explicit FImGuiTextureResource(FSlateShaderResource* InShaderResource)
		: Storage(TInPlaceType<FSlateShaderResource*>(), InShaderResource)
	{
	}
	explicit FImGuiTextureResource(const FSlateResourceHandle& InResourceHandle)
		: Storage(TInPlaceType<FSlateResourceHandle>(), InResourceHandle)
	{
	}

	const FSlateShaderResourceProxy* GetSlateShaderResourceProxy() const;
	FSlateShaderResource* GetSlateShaderResource() const;

	bool UsesResourceHandle() const { return Storage.IsType<FSlateResourceHandle>(); }
	bool UsesRawResource() const { return Storage.IsType<FSlateShaderResource*>(); }

private:
	TVariant<FSlateResourceHandle, FSlateShaderResource*> Storage;
};

USTRUCT()
struct FImGuiFontTextureEntry
{
	GENERATED_BODY()

	UPROPERTY(Transient)
	UTextureRenderTarget2D* Texture = nullptr;

	UPROPERTY(Transient)
	FSlateBrush SlateBrush;

	UPROPERTY(Transient)
	bool bInUse = false;
};

UCLASS(MinimalAPI)
class UImGuiSubsystem : public UEngineSubsystem
{
	GENERATED_BODY()

public:
	// USubsystem interface 
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	// USubsystem interface END

	// initialization
	IMGUIRUNTIME_API static bool ShouldEnableImGui();
	IMGUIRUNTIME_API static UImGuiSubsystem* Get();
	IMGUIRUNTIME_API static TMulticastDelegateRegistration<void(UImGuiSubsystem*)>& OnSubsystemInitialized()
	{
		return OnSubsystemInitializedDelegate;
	}

	IMGUIRUNTIME_API const char* GetIniDirectoryPath()	const { return *m_IniDirectoryPath; }
	IMGUIRUNTIME_API const char* GetIniFilePath()		const { return *m_IniFilePath; }

	// resources
	IMGUIRUNTIME_API FImGuiImageBindingParams RegisterOneFrameResource(const FSlateBrush* SlateBrush, FVector2f LocalSize, float DrawScale = 1.f);
	IMGUIRUNTIME_API FImGuiImageBindingParams RegisterOneFrameResource(const FSlateBrush* SlateBrush, float UniformSize) { return RegisterOneFrameResource(SlateBrush, FVector2f(UniformSize)); }
	IMGUIRUNTIME_API FImGuiImageBindingParams RegisterOneFrameResource(const FSlateBrush* SlateBrush);
	IMGUIRUNTIME_API FImGuiImageBindingParams RegisterOneFrameResource(UTexture2D* Texture);
	IMGUIRUNTIME_API FImGuiImageBindingParams RegisterOneFrameResource(FSlateShaderResource* SlateShaderResource);

	// widget
	IMGUIRUNTIME_API TSharedPtr<SWindow> CreateWidget(const FString& WindowName, FVector2f WindowSize, FOnTickImGuiWidgetDelegate TickDelegate);
	IMGUIRUNTIME_API FOnTickImGuiMainWindowDelegate& GetMainWindowTickDelegate() { return m_ImGuiMainWindowTickDelegate; }

	ImFontAtlas* GetSharedFontAtlas() const { return m_SharedFontAtlas.Get(); }
	IMGUIRUNTIME_API ImTextureRef GetSharedFontTextureID() const;

	static ImTextureID	IndexToImGuiID(uint32 Index)	{ return static_cast<ImTextureID>(Index); }
	static uint32		ImGuiIDToIndex(ImTextureID ID)  { return static_cast<uint32>(ID); }
	static ImTextureID	GetMissingImageTextureID()		{ return MissingImageTextureIndex; }
	static uint32		GetMissingImageTextureIndex()	{ return ImGuiIDToIndex(MissingImageTextureIndex); }

	const TArray<FImGuiTextureResource>& GetOneFrameResources() const { return m_OneFrameResources; }

	void UpdateFontAtlasTexture(ImTextureData* TexData);

	bool CaptureGpuFrame() const;

private:
	void OnBeginFrame();

	int32 AllocateFontAtlasTexture(int32 SizeX, int32 SizeY);
	void ReleaseFontAtlasTexture(int32 Index);

private:
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnSubsystemInitialized, UImGuiSubsystem* /*Subsystem*/)
	static FOnSubsystemInitialized OnSubsystemInitializedDelegate;

	FOnTickImGuiMainWindowDelegate m_ImGuiMainWindowTickDelegate;
	
	FAnsiString m_IniDirectoryPath;
	FAnsiString m_IniFilePath;

	UPROPERTY()
	TArray<FImGuiFontTextureEntry> m_SharedFontAtlasTextures;
	
	UPROPERTY()
	UTexture2D* m_MissingImageTexture = nullptr;

	int32 m_FontAtlasBuilderFrameCount = 0;
	TSharedPtr<ImFontAtlas, ESPMode::NotThreadSafe> m_SharedFontAtlas;

	FSlateBrush m_MissingImageSlateBrush = {};
	FImGuiImageBindingParams m_MissingImageParams = {};
	static constexpr uint32 MissingImageTextureIndex = 0u;
	static constexpr uint32 FontAtlasTextureStartIndex = MissingImageTextureIndex + 1u;

	TArray<FSlateBrush> m_OneFrameSlateBrushes;
	TArray<FImGuiTextureResource> m_OneFrameResources;
};
