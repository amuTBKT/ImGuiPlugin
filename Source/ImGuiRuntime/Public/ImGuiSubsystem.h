// Copyright 2024 Amit Kumar Mehar. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ImGuiPluginTypes.h"
#include "Styling/SlateBrush.h"
#include "Textures/SlateIcon.h"
#include "ImGuiPluginDelegates.h"
#include "Containers/AnsiString.h"
#include "Subsystems/EngineSubsystem.h"
#include "Textures/SlateShaderResource.h"
#include "ImGuiSubsystem.generated.h"

#if WITH_EDITOR
class SDockTab;
class FSpawnTabArgs;
#endif

class SWindow;
class UTexture2D;
class SImGuiWidgetBase;
class FAutoConsoleCommand;
class FSlateShaderResource;
class FImGuiRemoteConnection;

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
	IMGUIRUNTIME_API FImGuiImageBindingParams RegisterOneFrameResource(const FSlateBrush* SlateBrush, FVector2D LocalSize, float DrawScale = 1.f);
	IMGUIRUNTIME_API FImGuiImageBindingParams RegisterOneFrameResource(const FSlateBrush* SlateBrush);
	IMGUIRUNTIME_API FImGuiImageBindingParams RegisterOneFrameResource(UTexture2D* Texture);
	IMGUIRUNTIME_API FImGuiImageBindingParams RegisterOneFrameResource(FSlateShaderResource* SlateShaderResource);

	// widget
	IMGUIRUNTIME_API TSharedPtr<SWindow> CreateWindow(const FString& WindowName, const FVector2D& WindowSize, FOnTickImGuiWidgetDelegate TickDelegate);
	IMGUIRUNTIME_API FOnTickImGuiMainWindowDelegate& GetMainWindowTickDelegate() { return m_ImGuiMainWindowTickDelegate; }
	IMGUIRUNTIME_API const FOnTickImGuiMainWindowDelegate& GetMainWindowTickDelegate() const { return m_ImGuiMainWindowTickDelegate; }

	FORCEINLINE ImFontAtlas* GetDefaultImGuiFontAtlas()			  { return &m_DefaultFontAtlas; }
	FORCEINLINE ImTextureID  GetDefaultFontTextureID()		const { return m_DefaultFontImageParams.Id; }
	FORCEINLINE ImTextureID  GetMissingImageTextureID()		const { return m_MissingImageParams.Id; }
	FORCEINLINE uint32		 GetDefaultFontTextureIndex()	const { return ImGuiIDToIndex(m_DefaultFontImageParams.Id); }
	FORCEINLINE uint32		 GetMissingImageTextureIndex()	const { return ImGuiIDToIndex(m_MissingImageParams.Id); }
	
	FORCEINLINE const TArray<FImGuiTextureResource>& GetOneFrameResources() const { return m_OneFrameResources; }

	static ImTextureID  IndexToImGuiID(uint32 Index)	{ return static_cast<ImTextureID>(Index); }
	static uint32		ImGuiIDToIndex(ImTextureID ID)  { return static_cast<uint32>(ID); }

	bool CaptureGpuFrame() const;

	bool IsRemoteConnectionActive() const;
	void RegisterWidgetForRemoteClient(TSharedPtr<SImGuiWidgetBase> Widget);

private:
	void OnBeginFrame();
	void OnEndFrame();

	void OnRemoteConnectionEstablished();
	void OnRemoteConnectionClosed();
	void TickRemoteConnection();

private:
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnSubsystemInitialized, UImGuiSubsystem* /*Subsystem*/)
	static FOnSubsystemInitialized OnSubsystemInitializedDelegate;

	FOnTickImGuiMainWindowDelegate m_ImGuiMainWindowTickDelegate;
	
	FAnsiString m_IniDirectoryPath;
	FAnsiString m_IniFilePath;

	UPROPERTY()
	UTexture2D* m_DefaultFontTexture = nullptr;
	
	UPROPERTY()
	UTexture2D* m_MissingImageTexture = nullptr;

	ImFontAtlas m_DefaultFontAtlas = {};

	FSlateBrush m_DefaultFontSlateBrush = {};
	FSlateBrush m_MissingImageSlateBrush = {};

	FImGuiImageBindingParams m_DefaultFontImageParams = {};
	FImGuiImageBindingParams m_MissingImageParams = {};

	TArray<FSlateBrush> m_CreatedSlateBrushes;
	TArray<FImGuiTextureResource> m_OneFrameResources;

	TArray<TWeakPtr<SImGuiWidgetBase>> RegisteredImGuiWidgets;
	FImGuiRemoteConnection* RemoteConnection = nullptr;
};
