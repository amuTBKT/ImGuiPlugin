#pragma once

#include "CoreMinimal.h"

DECLARE_MULTICAST_DELEGATE_OneParam(FOnImGuiPluginInitialized, class FImGuiPluginModule& /* ImGuiPluginModule */);

DECLARE_MULTICAST_DELEGATE_OneParam(FOnTickImGuiMainWindowDelegate, float /* DeltaTime */);

DECLARE_DELEGATE_OneParam(FOnTickImGuiWidgetDelegate, float /* DeltaTime */);