#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"
#include "Textures/SlateShaderResource.h"

#include "ImGuiPluginDelegates.h"
#include "imgui.h"

#if WITH_EDITOR
class SDockTab;
#endif

class SWindow;
class FAutoConsoleCommand;

DECLARE_STATS_GROUP(TEXT("ImGui"), STATGROUP_ImGui, STATCAT_Advanced);

struct FImGuiImageBindParams
{
	ImVec2 Size = ImVec2(1.f, 1.f);
	ImVec2 UV0 = ImVec2(0.f, 0.f);
	ImVec2 UV1 = ImVec2(1.f, 1.f);
	ImTextureID Id = 0;
};

class IMGUIPLUGIN_API FImGuiPluginModule : public IModuleInterface
{
private:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

#if WITH_EDITOR
	static TSharedRef<SDockTab> SpawnImGuiTab(const FSpawnTabArgs& SpawnTabArgs);
#endif

	void OpenImGuiMainWindow();

public:
	TSharedPtr<SWindow> CreateWidget(const FString& WindowName, const FVector2D& WindowSize, FOnTickImGuiWidgetDelegate TickDelegate);

	static ImTextureID IndexToImGuiID(uint32 Index) { return reinterpret_cast<ImTextureID>(static_cast<uintptr_t>(Index));  }
	static uint32 ImGuiIDToIndex(ImTextureID ID) { return static_cast<uint32>(reinterpret_cast<uintptr_t>(ID));  }

	ImTextureID GetDefaultFontTextureID() const
	{
		return m_DefaultFontImageParams.Id;
	}
	
	const FSlateResourceHandle& GetResourceHandle(uint32 Index) const
	{
		if (!OneFrameResourceHandles.IsValidIndex(Index))
		{
			Index = ImGuiIDToIndex(m_MissingImageParams.Id);
		}

		check(OneFrameResourceHandles.IsValidIndex(Index));
		return OneFrameResourceHandles[Index];
	}

	const FSlateResourceHandle& GetResourceHandle(ImTextureID ID) const
	{
		return GetResourceHandle(ImGuiIDToIndex(ID));
	}

	FImGuiImageBindParams RegisterOneFrameResource(const FSlateBrush* SlateBrush, FVector2D LocalSize, float DrawScale);
	FImGuiImageBindParams RegisterOneFrameResource(const FSlateBrush* SlateBrush);
	FImGuiImageBindParams RegisterOneFrameResource(UTexture2D* Texture);
	
	FOnTickImGuiMainWindowDelegate& GetMainWindowTickDelegate() { return m_ImGuiMainWindowTickDelegate; }
	const FOnTickImGuiMainWindowDelegate& GetMainWindowTickDelegate() const { return m_ImGuiMainWindowTickDelegate; }

	static FOnImGuiPluginInitialized OnPluginInitialized;

protected:
	void OnBeginFrame();

private:
	FOnTickImGuiMainWindowDelegate m_ImGuiMainWindowTickDelegate;
	
	FSlateBrush m_DefaultFontSlateBrush = {};
	FSlateBrush m_MissingImageSlateBrush = {};

	FImGuiImageBindParams m_DefaultFontImageParams = {};
	FImGuiImageBindParams m_MissingImageParams = {};

	TArray<FSlateBrush> OneFrameSlateBrushes;
	TArray<FSlateResourceHandle> OneFrameResourceHandles;

#if WITH_EDITOR
	static const FName ImGuiTabName;
#endif

	TUniquePtr<FAutoConsoleCommand> m_OpenImGuiWindowCommand = nullptr;
	TSharedPtr<SWindow> m_ImGuiMainWindow = nullptr;
};