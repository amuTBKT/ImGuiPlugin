# ImGuiPlugin
A simple plugin for integrating [Dear ImGui](https://github.com/ocornut/imgui) in Unreal Engine 5.

## :warning:About
* The plugin is not production ready.<br>
* The plugin is just something silly built to add QOL improvements to editor.
* The plugin is not compatible with UE4.

## Installation
* Clone the repo in GameProject/Plugins folder along with submodule dependency<br>
  `git clone --recurse-submodules git@github.com:amuTBKT/ImGuiPlugin.git`
* ImGui and Implot binaries need to be compiled separately. The way unreal build setup works makes it difficult to include third party libraries, so they have to be compiled separately.<br>
  `Source/Thirdparty/ImGui/Build.bat`, might require some modifications to patch cmake reference and setting compiler options (visual studio version etc) <br>
  Running the batch script should generate required libs under "/ImGuiPlugin/Source/Thirdparty/ImGui/Binaries" and "/ImGuiPlugin/Source/Thirdparty/ImGui/Intermediate" directories.
* Include the plugin in your own modules.
* Feel free to use [ImGuiExamples](https://github.com/amuTBKT/ImGuiExamples) as reference.

## :construction_worker:Usage
[ImGuiExamples](https://github.com/amuTBKT/ImGuiExamples) :sweat_smile:

At it's core the plugin is just a Slate widget with ImGui renderer, yes it's that simple :) <br>
The plugin manages a global widget/window which can be used to add global widgets (managers, stats visualizer etc.) <br>
User can also create `SImGuiWidget` widget which works like any other Slate widget with an addition of Tick delegate which can be used to add ImGui widgets. <br>

### Adding to main window
* Using `IMGUI_REGISTER_STATIC_WIDGET(InitFunction, TickFunction)`
* Dynamically binding to main window delegate <br>
  ```
  FImGuiRuntimeModule& ImGuiRuntimeModule = FModuleManager::GetModuleChecked<FImGuiRuntimeModule>("ImGuiRuntime");
  MainWindowTickHandle = ImGuiRuntimeModule.GetMainWindowTickDelegate().Add(TickDelegate);`
  ```
### Creating a standalone window
  ```
  FImGuiRuntimeModule& ImGuiRuntimeModule = FModuleManager::GetModuleChecked<FImGuiRuntimeModule>("ImGuiRuntime");
  ImGuiWindow = ImGuiRuntimeModule.CreateWindow("TestWindow", { 300., 300. }, TickDelegate);
  ```
### Creating a slate widget
Users can create `SImGuiWidget` widget and add them to viewport or Slate containers, works similar to any other slate widget.
 
