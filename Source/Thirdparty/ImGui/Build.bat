@echo off
cd /D "%~dp0"

set INCLUDE_PATH="imgui/"
set SOURCE_FILE="ImGuiLib.cpp"
set DEFINES_COMMON=/DIMGUI_DISABLE_OBSOLETE_FUNCTIONS=1 /DIMGUI_DISABLE_DEFAULT_ALLOCATORS=1 /DIMGUI_DISABLE_DEMO_WINDOWS=1
set DEFINES_DEBUG=%DEFINES_COMMON% /DBUILD_DEBUG=1
set DEFINES_RELEASE=%DEFINES_COMMON% /DIMGUI_DISABLE_DEBUG_TOOLS=1 /DBUILD_DEBUG=0

set CL_OUTPUT_FILE="Intermediate\ImGuiLib.obj"
set CL_CLEANUP_INTERMEDIATES=if exist %CL_OUTPUT_FILE% del %CL_OUTPUT_FILE%
set CL_COMMON=/nologo /c /Fo%CL_OUTPUT_FILE% %SOURCE_FILE% /I%INCLUDE_PATH%
set CL_DEBUG=call cl /Od /Ob1 /Zi /Fd"Binaries/Debug/Win64/ImGui.pdb" %CL_COMMON% %DEFINES_DEBUG%
set CL_RELEASE=call cl /O2 %CL_COMMON% %DEFINES_RELEASE%
set CL_LINK_DEBUG=lib /nologo /out:"Binaries/Debug/Win64/ImGui.lib" %CL_OUTPUT_FILE%
set CL_LINK_RELEASE=lib /nologo /out:"Binaries/Release/Win64/ImGui.lib" %CL_OUTPUT_FILE%

if not exist Intermediate mkdir Intermediate
if not exist Binaries\Debug\Win64 mkdir Binaries\Debug\Win64
if not exist Binaries\Release\Win64 mkdir Binaries\Release\Win64

echo Compiling Debug Config...
%CL_CLEANUP_INTERMEDIATES%
%CL_DEBUG%
%CL_LINK_DEBUG%

echo Compiling Release Config...
%CL_CLEANUP_INTERMEDIATES%
%CL_RELEASE%
%CL_LINK_RELEASE%
