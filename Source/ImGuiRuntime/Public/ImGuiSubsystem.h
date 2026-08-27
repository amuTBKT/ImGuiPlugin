// Copyright 2024-26 Amit Kumar Mehar. All Rights Reserved.

#pragma once

#include "ImGuiPluginTypes.h"
#include "UObject/GCObject.h"
#include "Styling/SlateBrush.h"
#include "ImGuiPluginDelegates.h"
#include "Containers/AnsiString.h"
#include "Textures/SlateShaderResource.h"

class UWorld;
class SWindow;
class UTexture2D;
class FConfigFile;
class FTextureResource;
class FSlateShaderResource;
class UTextureRenderTarget2D;
class FSlateShaderResourceProxy;

namespace ImGuiUtils
{
	class FImGuiImageCache;
}

DECLARE_STATS_GROUP(TEXT("ImGui"), STATGROUP_ImGui, STATCAT_Advanced);

class FImGuiTextureResource
{
public:
	explicit FImGuiTextureResource(const FSlateResourceHandle& InResourceHandle)
		: Storage(TInPlaceType<FSlateResourceHandle>(), InResourceHandle)
	{
	}
	explicit FImGuiTextureResource(FSlateShaderResource* InShaderResource)
		: Storage(TInPlaceType<FSlateShaderResource*>(), InShaderResource)
	{
	}
	explicit FImGuiTextureResource(FTextureResource* InTextureResource)
		: Storage(TInPlaceType<FTextureResource*>(), InTextureResource)
	{
	}

	FSlateShaderResource*			 GetSlateShaderResource() const;
	const FSlateShaderResourceProxy* GetSlateShaderResourceProxy() const { return GetResourceHandle().GetResourceProxy(); }
	FTextureResource*				 GetTextureResource() const { check(UsesRawTextureResource()); return Storage.Get<FTextureResource*>(); }
	const FSlateResourceHandle&		 GetResourceHandle() const { check(UsesSlateResourceHandle()); return Storage.Get<FSlateResourceHandle>(); }

	bool UsesSlateResourceHandle()	const { return Storage.IsType<FSlateResourceHandle>(); }
	bool UsesRawSlateResource()		const { return Storage.IsType<FSlateShaderResource*>(); }
	bool UsesRawTextureResource()	const { return Storage.IsType<FTextureResource*>(); }

private:
	TVariant<FSlateResourceHandle, FSlateShaderResource*, FTextureResource*> Storage;
};

enum class EImGuiMainMenuWidgetFlags : uint8
{
	None				= 0,
	TickInMenuBar		= 1 << 0,	// allow ticking the widget in menu bar
	SkipWindowCreation	= 1 << 1,	// widget callback handles window creation
	RightAligned		= 1 << 2,	// draw the widget from right side of the window
};
ENUM_CLASS_FLAGS(EImGuiMainMenuWidgetFlags);

class UImGuiSubsystem : FNoncopyable, FGCObject
{
public:
	void Initialize();
	void Deinitialize();

	// initialization
	IMGUIRUNTIME_API static UImGuiSubsystem* Get();
	IMGUIRUNTIME_API static bool ShouldEnableImGui();
	static void InitializeSubsystemInstance();
	static void ReleaseSubsystemInstance();

	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
	virtual FString GetReferencerName() const override { return TEXT("UImGuiSubsystem"); }

	// events
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnSubsystemInitialized, UImGuiSubsystem* /*Subsystem*/)
	IMGUIRUNTIME_API static FOnSubsystemInitialized OnSubsystemInitialized;
	static FSimpleMulticastDelegate OnBeginImGuiFrame;
	static FSimpleMulticastDelegate OnEndImGuiFrame;
	static FSimpleMulticastDelegate OnShutdown;

	const char* GetIniDirectoryPath()	const { return *m_IniDirectoryPath; }

	IMGUIRUNTIME_API static const FString& GetSaveDataConfigFilepath();
	FConfigFile* GetSaveDataConfigFile() const { return m_SaveDataConfigFile; }
	IMGUIRUNTIME_API bool SaveConfigToDisk() const;

	// resources
	IMGUIRUNTIME_API FImGuiImageBindingParams RegisterOneFrameResource(const FSlateBrush* SlateBrush, FVector2f LocalSize, float DrawScale = 1.f);
	FImGuiImageBindingParams RegisterOneFrameResource(const FSlateBrush* SlateBrush, float UniformSize) { return RegisterOneFrameResource(SlateBrush, FVector2f(UniformSize)); }
	FImGuiImageBindingParams RegisterOneFrameResource(const FSlateBrush* SlateBrush) { return SlateBrush ? RegisterOneFrameResource(SlateBrush, SlateBrush->GetImageSize(), 1.0f) : FImGuiImageBindingParams(); }
	IMGUIRUNTIME_API FImGuiImageBindingParams RegisterOneFrameResource(FSlateShaderResource* SlateShaderResource);
#ifdef IMGUI_USE_NATIVE_RENDERER
	IMGUIRUNTIME_API FImGuiImageBindingParams RegisterOneFrameResource(UTexture2D* Texture);
#endif
	const FImGuiTextureResource* GetOneFrameResource(ImTextureID ResourceId) const
	{
		const FOneFrameResource* TextureEntry = m_OneFrameResources.FindByKey(ResourceId);
		return TextureEntry ? &TextureEntry->Resource : nullptr;
	}

	// widget
	IMGUIRUNTIME_API TSharedPtr<SWindow> CreateWidget(const FString& WindowName, FVector2f WindowSize, FOnTickImGuiWidgetDelegate TickDelegate);

#ifdef IMGUI_ALLOW_MENUBAR_EXTENSION
	IMGUIRUNTIME_API void RegisterMainMenuWidget(
		const UWorld* World, const char* WidgetPath, const char* WidgetToolTip, const FSlateBrush* WidgetIcon,
		FOnTickImGuiWidgetDelegate TickDelegate, EImGuiMainMenuWidgetFlags WidgetFlags = EImGuiMainMenuWidgetFlags::None) const;
	void RegisterMainMenuWidget(
		const UWorld* World, const char* WidgetPath,
		FOnTickImGuiWidgetDelegate TickDelegate, EImGuiMainMenuWidgetFlags WidgetFlags = EImGuiMainMenuWidgetFlags::None) const
	{
		RegisterMainMenuWidget(World, WidgetPath, "", nullptr, MoveTemp(TickDelegate), WidgetFlags);
	}
	IMGUIRUNTIME_API void UnregisterMainMenuWidget(const UWorld* World, const char* WidgetPath) const;
	IMGUIRUNTIME_API bool* GetMainMenuWidgetActiveState(const UWorld* World, const char* WidgetPath) const;
	IMGUIRUNTIME_API FImGuiTickContext* GetMainMenuWidgetTickContext(const UWorld* World) const;
#endif

	void UpdateFontAtlasTextures(ImTextureData** Textures, int32 TextureCount);
	IMGUIRUNTIME_API void CommitSharedFontAtlasChanges();
	IMGUIRUNTIME_API ImTextureRef GetSharedFontTextureID() const;
	ImFontAtlas* GetSharedFontAtlas() const { return m_SharedFontAtlas.Get(); }

	bool CaptureGpuFrame() const;

private:
	void BeginImGuiFrame();
	void EndImGuiFrame();

	FImGuiImageBindingParams RegisterOneFrameResource(ImTextureID ResourceId, FImGuiTextureResource&& Resource, float Width, float Height);

	void UpdateFontAtlasTexture(ImTextureData* TexData);
	ImTextureID AllocateFontAtlasTexture(int32 SizeX, int32 SizeY);
	void ReleaseFontAtlasTexture(ImTextureID ResourceId);

private:
	static TUniquePtr<UImGuiSubsystem> SubsystemInstance;

	FConfigFile* m_SaveDataConfigFile = nullptr;

	FAnsiString m_IniDirectoryPath;

	struct FImGuiFontTextureEntry
	{
#ifdef IMGUI_USE_NATIVE_RENDERER
		TObjectPtr<UTextureRenderTarget2D> Texture = nullptr;
#else
		TSharedPtr<FSlateBrush> Brush = nullptr;
#endif
		bool bInUse = false;
	};
	TArray<TUniquePtr<FImGuiFontTextureEntry>> m_SharedFontAtlasTextures;

	int32 m_FontAtlasBuilderFrameCount = 0;
	TSharedPtr<ImFontAtlas, ESPMode::NotThreadSafe> m_SharedFontAtlas;
	TUniquePtr<ImGuiUtils::FImGuiImageCache> m_ImageCache;

	struct FOneFrameResource
	{
		FImGuiTextureResource	Resource;
		ImTextureID				ResourceId;

		bool operator==(ImTextureID InResourceId) const { return ResourceId == InResourceId; }
	};
	TArray<FOneFrameResource> m_OneFrameResources;
};
