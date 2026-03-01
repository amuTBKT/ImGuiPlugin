# ImGuiPlugin
A simple plugin for integrating [Dear ImGui](https://github.com/ocornut/imgui) in Unreal Engine 5.

## Installation
* Clone the repo in Project/Plugins folder `git clone git@github.com:amuTBKT/ImGuiPlugin.git` <br>
  or download zip and extract under Project/Plugins folder.
* Include the plugin in your own modules.
* Feel free to use [ImGuiExamples](https://github.com/amuTBKT/ImGuiExamples) as reference.

### Static Lib
For editor builds the plugin also supports linking ImGui as a static library. This can improve performance when widgets make signficant amount of ImGui calls. <br>
Calling `Source/Thirdparty/ImGui/Build.bat` from MSVC command prompt (x64 Native Tools Command Prompt) should generate the required libs under "/ImGuiPlugin/Source/Thirdparty/ImGui/Binaries" directory. <br>
Make sure to regenerate project files or touch `Source/Thirdparty/ImGui/ImGui.Build.cs` in order to update the library dependency.

## Usage
At it's core the plugin is just a Slate widget with ImGui renderer, yes it's that simple :) <br>
The plugin manages a global widget/window which can be used to add global widgets (managers, stats visualizer etc.) <br>
User can also create `SImGuiWidget` widget which works like any other Slate widget with an addition of Tick delegate which can be used to add ImGui widgets. <br>

### Adding widgets to main window
* Using `IMGUI_REGISTER_MAIN_WINDOW_WIDGET(RegisterParams)`
* Dynamically binding to main window delegate <br>
  ```
  UImGuiSubsystem* Subsystem = UImGuiSubsystem::Get();
  MainWindowTickHandle = Subsystem->GetMainWindowTickDelegate().Add(TickDelegate);`
  ```
### Creating a standalone widget
* Using `IMGUI_REGISTER_STANDALONE_WIDGET`, works in editor only. In packaged builds all widgets are added to the main ImGui window.
* Creating a standalone ImGui widget
  ```
  UImGuiSubsystem* Subsystem = UImGuiSubsystem::Get();
  ImGuiWindow = Subsystem->CreateWidget("WidgetName", /*WindowSize=*/{ 300., 300. }, TickDelegate);
  ```
### Creating a slate widget
Users can create `SImGuiWidget` widget and add them to viewport or Slate containers, works similar to any other slate widget.
