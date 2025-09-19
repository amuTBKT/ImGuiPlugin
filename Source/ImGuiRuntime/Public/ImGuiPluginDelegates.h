// Copyright 2024 Amit Kumar Mehar. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

struct FImGuiTickContext;

DECLARE_MULTICAST_DELEGATE_OneParam(FOnTickImGuiMainWindowDelegate, FImGuiTickContext* /* Context */);

DECLARE_DELEGATE_OneParam(FOnTickImGuiWidgetDelegate, FImGuiTickContext* /* Context */);
