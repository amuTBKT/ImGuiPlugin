#pragma once

#include "CoreMinimal.h"
#include "Styling/SlateBrush.h"
#include "Modules/ModuleManager.h"
#include "Modules/ModuleInterface.h"
#include "Textures/SlateShaderResource.h"

#include "ImGuiPluginDelegates.h"
#include "ImGuiPluginTypes.h"

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

class IMGUIRUNTIME_API FImGuiRuntimeModule : public IModuleInterface
{
private:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

#if WITH_EDITOR
	static TSharedRef<SDockTab> SpawnImGuiTab(const FSpawnTabArgs& SpawnTabArgs);
#endif

	void OpenImGuiMainWindow();

public:
	TSharedPtr<SWindow> CreateWindow(const FString& WindowName, const FVector2D& WindowSize, FOnTickImGuiWidgetDelegate TickDelegate);

	static ImTextureID IndexToImGuiID(uint32 Index) { return reinterpret_cast<ImTextureID>(static_cast<uintptr_t>(Index));  }
	static uint32 ImGuiIDToIndex(ImTextureID ID) { return static_cast<uint32>(reinterpret_cast<uintptr_t>(ID));  }

	ImTextureID GetDefaultFontTextureID()	const { return m_DefaultFontImageParams.Id; }
	ImTextureID GetMissingImageTextureID()	const { return m_MissingImageParams.Id; }
	uint32 GetDefaultFontTextureIndex()		const { return ImGuiIDToIndex(m_DefaultFontImageParams.Id); }
	uint32 GetMissingImageTextureIndex()	const { return ImGuiIDToIndex(m_MissingImageParams.Id); }
	
	const FImGuiTextureResource& GetResource(uint32 Index) const
	{
		if (!OneFrameResources.IsValidIndex(Index))
		{
			Index = ImGuiIDToIndex(m_MissingImageParams.Id);
		}

		check(OneFrameResources.IsValidIndex(Index));
		return OneFrameResources[Index];
	}
	const FImGuiTextureResource& GetResource(ImTextureID ID) const { return GetResource(ImGuiIDToIndex(ID)); }
	const TArray<FImGuiTextureResource>& GetOneFrameResources() const { return OneFrameResources; }

	FImGuiImageBindingParams RegisterOneFrameResource(const FSlateBrush* SlateBrush, FVector2D LocalSize, float DrawScale);
	FImGuiImageBindingParams RegisterOneFrameResource(const FSlateBrush* SlateBrush);
	FImGuiImageBindingParams RegisterOneFrameResource(UTexture2D* Texture);
	FImGuiImageBindingParams RegisterOneFrameResource(FSlateShaderResource* SlateShaderResource);
	
	FOnTickImGuiMainWindowDelegate& GetMainWindowTickDelegate() { return m_ImGuiMainWindowTickDelegate; }
	const FOnTickImGuiMainWindowDelegate& GetMainWindowTickDelegate() const { return m_ImGuiMainWindowTickDelegate; }

	bool CaptureGpuFrame() const;

	static FOnImGuiPluginInitialized OnPluginInitialized;
	static bool IsPluginInitialized;

protected:
	void OnBeginFrame();

private:
	FOnTickImGuiMainWindowDelegate m_ImGuiMainWindowTickDelegate;
	
	FSlateBrush m_DefaultFontSlateBrush = {};
	FSlateBrush m_MissingImageSlateBrush = {};

	FImGuiImageBindingParams m_DefaultFontImageParams = {};
	FImGuiImageBindingParams m_MissingImageParams = {};

	TArray<FSlateBrush> CreatedSlateBrushes;
	TArray<FImGuiTextureResource> OneFrameResources;

#if WITH_EDITOR
	static const FName ImGuiTabName;
#endif

	TUniquePtr<FAutoConsoleCommand> m_OpenImGuiWindowCommand = nullptr;
	TSharedPtr<SWindow> m_ImGuiMainWindow = nullptr;
};