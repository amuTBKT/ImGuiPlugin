#pragma once

#include "CoreMinimal.h"

DECLARE_MULTICAST_DELEGATE_OneParam(FOnImGuiPluginInitialized, class FImGuiRuntimeModule& /* ImGuiRuntimeModule */);

DECLARE_MULTICAST_DELEGATE_TwoParams(FOnTickImGuiMainWindowDelegate, struct ImGuiContext* /* Context */, float /* DeltaTime */);

DECLARE_DELEGATE_TwoParams(FOnTickImGuiWidgetDelegate, struct ImGuiContext* /* Context */, float /* DeltaTime */);