# Copyright 2024 Amit Kumar Mehar. All Rights Reserved.

cmake_minimum_required(VERSION 3.13)

project(ImGui)

include_directories(imgui/)

set(HEADER_FILES
    imgui/imgui.h
    imgui/imconfig.h
    implot/implot.h)

set(SOURCE_FILES
    imgui/imgui.cpp
    imgui/imgui_draw.cpp
    imgui/imgui_tables.cpp
    imgui/imgui_widgets.cpp
    implot/implot.cpp
    implot/implot_items.cpp)

add_definitions(-DIMGUI_DISABLE_OBSOLETE_FUNCTIONS=1)

add_library(ImGui STATIC ${HEADER_FILES} ${SOURCE_FILES})

set_target_properties(ImGui
	PROPERTIES
	ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/../Binaries/")