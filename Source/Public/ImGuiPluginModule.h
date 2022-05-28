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
		return m_DefaultFontTextureID;
	}
	
	const FSlateResourceHandle& GetTextureResourceHandle(uint32 Index) const
	{
		if (!m_TextureBrushes.IsValidIndex(Index))
		{
			Index = ImGuiIDToIndex(m_MissingImageTextureID);
		}

		check(m_TextureBrushes.IsValidIndex(Index));
		return m_TextureBrushes[Index].GetRenderingResource();
	}

	const FSlateResourceHandle& GetTextureResourceHandle(ImTextureID ID) const
	{
		return GetTextureResourceHandle(ImGuiIDToIndex(ID));
	}

	ImTextureID RegisterTexture(UTexture2D* Texture);
	void UnregisterTexture(UTexture2D* Texture);

	ImTextureID RegisterSlateBrush(FSlateBrush SlateBrush) { checkf(false, TEXT("NOT IMPLEMENTED!")); return {}; };
	void UnregisterSlateBrush(UTexture2D* Texture) { checkf(false, TEXT("NOT IMPLEMENTED!")); };

	FOnTickImGuiMainWindowDelegate& GetMainWindowTickDelegate() { return m_ImGuiMainWindowTickDelegate; }
	const FOnTickImGuiMainWindowDelegate& GetMainWindowTickDelegate() const { return m_ImGuiMainWindowTickDelegate; }

	static FOnImGuiPluginInitialized OnPluginInitialized;

private:
	FOnTickImGuiMainWindowDelegate m_ImGuiMainWindowTickDelegate;
	TArray<FSlateBrush> m_TextureBrushes = {};
	TMap<FName, ImTextureID> m_TextureIDMap = {};
	
	ImTextureID m_MissingImageTextureID = {};
	ImTextureID m_DefaultFontTextureID = {};

#if WITH_EDITOR
	static const FName ImGuiTabName;
#endif

	TUniquePtr<FAutoConsoleCommand> m_OpenImGuiWindowCommand = nullptr;
	TSharedPtr<SWindow> m_ImGuiMainWindow = nullptr;
};