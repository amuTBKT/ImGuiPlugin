// Copyright 2024 Amit Kumar Mehar. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

DECLARE_MULTICAST_DELEGATE_OneParam(FOnTickImGuiMainWindowDelegate, struct ImGuiContext* /* Context */);

DECLARE_DELEGATE_OneParam(FOnTickImGuiWidgetDelegate, struct ImGuiContext* /* Context */);
