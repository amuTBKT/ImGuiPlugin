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

DECLARE_STATS_GROUP(TEXT("ImGui"), STATGROUP_ImGui, STATCAT_Advanced);

class FImGuiTextureResource
{
public:
	FImGuiTextureResource() = default;
	FImGuiTextureResource(const FImGuiTextureResource&) = default;
	FImGuiTextureResource& operator=(const FImGuiTextureResource&) = default;
	FImGuiTextureResource(FImGuiTextureResource&&) = default;
	FImGuiTextureResource& operator=(FImGuiTextureResource&&) = default;
	~FImGuiTextureResource() {}

	explicit FImGuiTextureResource(FSlateShaderResource* InShaderResource)
	{
		UnderlyingResource.Set<FSlateShaderResource*>(InShaderResource);
	}

	explicit FImGuiTextureResource(const FSlateResourceHandle& InResourceHandle)
	{
		UnderlyingResource.Set<FSlateResourceHandle>(InResourceHandle);
	}

	FSlateShaderResource* GetSlateShaderResource() const;

private:
	TVariant<FSlateResourceHandle, FSlateShaderResource*> UnderlyingResource;
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

	static ImTextureID  IndexToImGuiID(uint32 Index)	{ return static_cast<ImTextureID>(Index); }
	static uint32		ImGuiIDToIndex(ImTextureID ID)  { return static_cast<uint32>(ID); }

	ImFontAtlas*		 GetSharedFontAtlas()			{ return m_SharedFontAtlas.Get(); }
	static ImTextureID   GetSharedFontTextureID()		{ return SharedFontTexID; }
	static ImTextureID   GetMissingImageTextureID()		{ return MissingImageTexID; }
	static uint32		 GetSharedFontTextureIndex()	{ return ImGuiIDToIndex(SharedFontTexID); }
	static uint32		 GetMissingImageTextureIndex()	{ return ImGuiIDToIndex(MissingImageTexID); }
	
	const TArray<FImGuiTextureResource>& GetOneFrameResources() const { return m_OneFrameResources; }

	void UpdateTextureData(ImTextureData* TexData) const;

	bool CaptureGpuFrame() const;

private:
	void OnBeginFrame();

private:
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnSubsystemInitialized, UImGuiSubsystem* /*Subsystem*/)
	static FOnSubsystemInitialized OnSubsystemInitializedDelegate;

	FOnTickImGuiMainWindowDelegate m_ImGuiMainWindowTickDelegate;
	
	FAnsiString m_IniDirectoryPath;
	FAnsiString m_IniFilePath;

	UPROPERTY()
	UTextureRenderTarget2D* m_SharedFontTexture = nullptr;
	
	UPROPERTY()
	UTexture2D* m_MissingImageTexture = nullptr;

	int32 m_FontAtlasBuilderFrameCount = 0;
	TSharedPtr<ImFontAtlas, ESPMode::NotThreadSafe> m_SharedFontAtlas;
	FSlateBrush m_MissingImageSlateBrush = {};
	FSlateBrush m_SharedFontSlateBrush = {};
	FImGuiImageBindingParams m_MissingImageParams = {};
	FImGuiImageBindingParams m_SharedFontImageParams = {};
	static inline const ImTextureID MissingImageTexID = IndexToImGuiID(0u);
	static inline const ImTextureID SharedFontTexID = IndexToImGuiID(1u);

	TArray<FSlateBrush> m_CreatedSlateBrushes;
	TArray<FImGuiTextureResource> m_OneFrameResources;
};
