// Copyright 2024 Amit Kumar Mehar. All Rights Reserved.

#include "ImGuiShaders.h"

IMPLEMENT_GLOBAL_SHADER(FImGuiVS, "/Plugin/ImGui/ImGuiShader.usf", "MainVS", SF_Vertex);

IMPLEMENT_GLOBAL_SHADER(FImGuiPS, "/Plugin/ImGui/ImGuiShader.usf", "MainPS", SF_Pixel);
