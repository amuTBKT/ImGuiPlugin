cmake_minimum_required(VERSION 3.10)

project(ImGui)

set(HEADER_FILES
    imgui/imgui.h
    imgui/imconfig.h)

set(SOURCE_FILES
    imgui/imgui.cpp
    imgui/imgui_draw.cpp
    imgui/imgui_tables.cpp
    imgui/imgui_widgets.cpp)

add_library(ImGui STATIC ${HEADER_FILES} ${SOURCE_FILES})

set_target_properties(ImGui
	PROPERTIES
	ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/../Binaries/")