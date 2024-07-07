// Copyright 2024 Amit Kumar Mehar. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ImGuiPluginTypes.h"
#include "Styling/SlateBrush.h"
#include "ImGuiPluginDelegates.h"
#include "Subsystems/EngineSubsystem.h"
#include "Textures/SlateShaderResource.h"
#include "ImGuiSubsystem.generated.h"

#if WITH_EDITOR
class SDockTab;
class FSpawnTabArgs;
#endif

class SWindow;
class UTexture2D;
class FAutoConsoleCommand;
class FSlateShaderResource;

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
	//USubsystem interface 
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	//USubsystem interface END

	// initialization
	IMGUIRUNTIME_API static bool ShouldEnableImGui();
	IMGUIRUNTIME_API static UImGuiSubsystem* Get();
	IMGUIRUNTIME_API static TMulticastDelegateRegistration<void(UImGuiSubsystem*)>& OnSubsystemInitialized()
	{
		return OnSubsystemInitializedDelegate;
	}

	// resources
	IMGUIRUNTIME_API FImGuiImageBindingParams RegisterOneFrameResource(const FSlateBrush* SlateBrush, FVector2D LocalSize, float DrawScale);
	IMGUIRUNTIME_API FImGuiImageBindingParams RegisterOneFrameResource(const FSlateBrush* SlateBrush);
	IMGUIRUNTIME_API FImGuiImageBindingParams RegisterOneFrameResource(UTexture2D* Texture);
	IMGUIRUNTIME_API FImGuiImageBindingParams RegisterOneFrameResource(FSlateShaderResource* SlateShaderResource);

	// widget
	IMGUIRUNTIME_API TSharedPtr<SWindow> CreateWindow(const FString& WindowName, const FVector2D& WindowSize, FOnTickImGuiWidgetDelegate TickDelegate);
	IMGUIRUNTIME_API FOnTickImGuiMainWindowDelegate& GetMainWindowTickDelegate() { return m_ImGuiMainWindowTickDelegate; }
	IMGUIRUNTIME_API const FOnTickImGuiMainWindowDelegate& GetMainWindowTickDelegate() const { return m_ImGuiMainWindowTickDelegate; }

	static ImTextureID  IndexToImGuiID(uint32 Index)	{ return reinterpret_cast<ImTextureID>(static_cast<uintptr_t>(Index));  }
	static uint32		ImGuiIDToIndex(ImTextureID ID)  { return static_cast<uint32>(reinterpret_cast<uintptr_t>(ID));  }

	FORCEINLINE ImFontAtlas* GetDefaultImGuiFontAtlas()		const { return DefaultFontAtlas.Get(); }
	FORCEINLINE ImTextureID  GetDefaultFontTextureID()		const { return m_DefaultFontImageParams.Id; }
	FORCEINLINE ImTextureID  GetMissingImageTextureID()		const { return m_MissingImageParams.Id; }
	FORCEINLINE uint32		 GetDefaultFontTextureIndex()	const { return ImGuiIDToIndex(m_DefaultFontImageParams.Id); }
	FORCEINLINE uint32		 GetMissingImageTextureIndex()	const { return ImGuiIDToIndex(m_MissingImageParams.Id); }
	
	FORCEINLINE const FImGuiTextureResource& GetResource(uint32 Index) const
	{
		if (!OneFrameResources.IsValidIndex(Index))
		{
			Index = ImGuiIDToIndex(m_MissingImageParams.Id);
		}

		check(OneFrameResources.IsValidIndex(Index));
		return OneFrameResources[Index];
	}
	FORCEINLINE const FImGuiTextureResource& GetResource(ImTextureID ID) const { return GetResource(ImGuiIDToIndex(ID)); }
	FORCEINLINE const TArray<FImGuiTextureResource>& GetOneFrameResources() const { return OneFrameResources; }

	bool CaptureGpuFrame() const;

private:
	void OnBeginFrame();

private:
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnSubsystemInitialized, UImGuiSubsystem* /* Subsystem */)
	static FOnSubsystemInitialized OnSubsystemInitializedDelegate;

	FOnTickImGuiMainWindowDelegate m_ImGuiMainWindowTickDelegate;
	
	UTexture2D* DefaultFontTexture = nullptr;
	UTexture2D* MissingImageTexture = nullptr;
	TUniquePtr<ImFontAtlas> DefaultFontAtlas = nullptr;

	FSlateBrush m_DefaultFontSlateBrush = {};
	FSlateBrush m_MissingImageSlateBrush = {};

	FImGuiImageBindingParams m_DefaultFontImageParams = {};
	FImGuiImageBindingParams m_MissingImageParams = {};

	TArray<FSlateBrush> CreatedSlateBrushes;
	TArray<FImGuiTextureResource> OneFrameResources;
};
