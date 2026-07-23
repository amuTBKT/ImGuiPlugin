// Copyright 2024-26 Amit Kumar Mehar. All Rights Reserved.

#pragma once

#include "ImGuiPluginTypes.h"
#include "Styling/SlateBrush.h"
#include "ImGuiPluginDelegates.h"
#include "Containers/AnsiString.h"
#include "Textures/SlateShaderResource.h"

#include "ImGuiSubsystem.generated.h"

class UWorld;
class SWindow;
class FConfigFile;
class FSlateShaderResource;
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

	FSlateResourceHandle GetResourceHandle() const { check(UsesResourceHandle()); return Storage.Get<FSlateResourceHandle>(); }

private:
	TVariant<FSlateResourceHandle, FSlateShaderResource*> Storage;
};

enum class EImGuiMainMenuWidgetFlags : uint8
{
	None				= 0,
	TickInMenuBar		= 1 << 0,	// allow ticking the widget in menu bar
	SkipWindowCreation	= 1 << 1,	// widget callback handles window creation
};
ENUM_CLASS_FLAGS(EImGuiMainMenuWidgetFlags);

UCLASS(MinimalAPI)
class UImGuiSubsystem : public UObject
{
	GENERATED_BODY()

public:
	bool ShouldCreateSubsystem() const;
	void Initialize();
	void Deinitialize();

	// initialization
	IMGUIRUNTIME_API static bool ShouldEnableImGui();
	static void InitializeSubsystem();
	static UImGuiSubsystem* Get() { return SubsystemInstance; }

	// events
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnSubsystemInitialized, UImGuiSubsystem* /*Subsystem*/)
	IMGUIRUNTIME_API static FOnSubsystemInitialized OnSubsystemInitialized;
	static FSimpleMulticastDelegate OnBeginImGuiFrame;
	static FSimpleMulticastDelegate OnEndImGuiFrame;

	const char* GetIniDirectoryPath()	const { return *m_IniDirectoryPath; }

	IMGUIRUNTIME_API static const FString& GetSaveDataConfigFilepath();
	FConfigFile* GetSaveDataConfigFile() const { return m_SaveDataConfigFile; }
	IMGUIRUNTIME_API bool SaveConfigToDisk() const;

	// resources
	IMGUIRUNTIME_API FImGuiImageBindingParams RegisterOneFrameResource(const FSlateBrush* SlateBrush, FVector2f LocalSize, float DrawScale = 1.f);
	IMGUIRUNTIME_API FImGuiImageBindingParams RegisterOneFrameResource(const FSlateBrush* SlateBrush, float UniformSize) { return RegisterOneFrameResource(SlateBrush, FVector2f(UniformSize)); }
	IMGUIRUNTIME_API FImGuiImageBindingParams RegisterOneFrameResource(const FSlateBrush* SlateBrush);
	IMGUIRUNTIME_API FImGuiImageBindingParams RegisterOneFrameResource(FSlateShaderResource* SlateShaderResource);
	const TArray<FImGuiTextureResource>&	  GetOneFrameResources() const { return m_OneFrameResources; }

	// widget
	IMGUIRUNTIME_API TSharedPtr<SWindow> CreateWidget(const FString& WindowName, FVector2f WindowSize, FOnTickImGuiWidgetDelegate TickDelegate);

#if WITH_ENGINE
	IMGUIRUNTIME_API FImGuiImageBindingParams RegisterOneFrameResource(class UTexture2D* Texture);

	IMGUIRUNTIME_API void RegisterMainMenuWidget(
		const UWorld* World, const char* WidgetPath, const char* WidgetToolTip, const FSlateBrush* WidgetIcon,
		FOnTickImGuiWidgetDelegate TickDelegate, EImGuiMainMenuWidgetFlags WidgetFlags = EImGuiMainMenuWidgetFlags::None) const;
	IMGUIRUNTIME_API void UnregisterMainMenuWidget(const UWorld* World, const char* WidgetPath) const;
	IMGUIRUNTIME_API bool* GetMainMenuWidgetActiveState(const UWorld* World, const char* WidgetPath) const;
	IMGUIRUNTIME_API FImGuiTickContext* GetWidgetTickContext(const UWorld* World) const;
#endif

	void UpdateFontAtlasTexture(ImTextureData* TexData);
	IMGUIRUNTIME_API ImTextureRef GetSharedFontTextureID() const;
	ImFontAtlas* GetSharedFontAtlas() const { return m_SharedFontAtlas.Get(); }

	static ImTextureID	IndexToImGuiID(uint32 Index)	{ return static_cast<ImTextureID>(Index); }
	static uint32		ImGuiIDToIndex(ImTextureID ID)  { return static_cast<uint32>(ID); }
	static ImTextureID	GetMissingImageTextureID()		{ return MissingImageTextureIndex; }
	static uint32		GetMissingImageTextureIndex()	{ return ImGuiIDToIndex(MissingImageTextureIndex); }

	bool CaptureGpuFrame() const;

private:
	void BeginImGuiFrame();
	void EndImGuiFrame();

	int32 AllocateFontAtlasTexture(int32 SizeX, int32 SizeY);
	void ReleaseFontAtlasTexture(int32 Index);

private:
	IMGUIRUNTIME_API static UImGuiSubsystem* SubsystemInstance;

	FConfigFile* m_SaveDataConfigFile = nullptr;

	FAnsiString m_IniDirectoryPath;

	struct FImGuiFontTextureEntry
	{
		TSharedPtr<FSlateBrush> Brush = nullptr;
		bool bInUse = false;

		~FImGuiFontTextureEntry();
	};
	TArray<FImGuiFontTextureEntry> m_SharedFontAtlasTextures;
	TSharedPtr<FSlateBrush> m_MissingImageBrush = nullptr;

	int32 m_FontAtlasBuilderFrameCount = 0;
	TSharedPtr<ImFontAtlas, ESPMode::NotThreadSafe> m_SharedFontAtlas;

	FImGuiImageBindingParams m_MissingImageParams = {};
	static constexpr uint32 MissingImageTextureIndex = 0u;
	static constexpr uint32 FontAtlasTextureStartIndex = MissingImageTextureIndex + 1u;

	TArray<FSlateBrush> m_OneFrameSlateBrushes;
	TArray<FImGuiTextureResource> m_OneFrameResources;
};
