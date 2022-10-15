#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"
#include "Textures/SlateShaderResource.h"

#include "ImGuiPluginDelegates.h"
#include "ImGuiPluginTypes.h"

#if WITH_EDITOR
class SDockTab;
#endif

class SWindow;
class FAutoConsoleCommand;

DECLARE_STATS_GROUP(TEXT("ImGui"), STATGROUP_ImGui, STATCAT_Advanced);

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
	
	const FSlateResourceHandle& GetResourceHandle(uint32 Index) const
	{
		if (!OneFrameResourceHandles.IsValidIndex(Index))
		{
			Index = ImGuiIDToIndex(m_MissingImageParams.Id);
		}

		check(OneFrameResourceHandles.IsValidIndex(Index));
		return OneFrameResourceHandles[Index];
	}
	const FSlateResourceHandle& GetResourceHandle(ImTextureID ID) const { return GetResourceHandle(ImGuiIDToIndex(ID)); }
	const TArray<FSlateResourceHandle>& GetResourceHandles() const { return OneFrameResourceHandles; }

	FImGuiImageBindingParams RegisterOneFrameResource(const FSlateBrush* SlateBrush, FVector2D LocalSize, float DrawScale);
	FImGuiImageBindingParams RegisterOneFrameResource(const FSlateBrush* SlateBrush);
	FImGuiImageBindingParams RegisterOneFrameResource(UTexture2D* Texture);
	
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
	TArray<FSlateResourceHandle> OneFrameResourceHandles;

#if WITH_EDITOR
	static const FName ImGuiTabName;
#endif

	TUniquePtr<FAutoConsoleCommand> m_OpenImGuiWindowCommand = nullptr;
	TSharedPtr<SWindow> m_ImGuiMainWindow = nullptr;
};