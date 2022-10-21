#pragma once

#include "CoreMinimal.h"

DECLARE_MULTICAST_DELEGATE_OneParam(FOnImGuiPluginInitialized, class FImGuiRuntimeModule& /* ImGuiRuntimeModule */);

DECLARE_MULTICAST_DELEGATE_OneParam(FOnTickImGuiMainWindowDelegate, struct ImGuiContext* /* Context */);

DECLARE_DELEGATE_OneParam(FOnTickImGuiWidgetDelegate, struct ImGuiContext* /* Context */);