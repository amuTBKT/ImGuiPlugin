# ImGuiPlugin
A simple plugin for integrating [Dear ImGui](https://github.com/ocornut/imgui) in Unreal Engine 5.

## :warning:About
* The plugin is not intended to be used in a production environment.<br>
* This is just a silly plugin built to add some QOL improvements to editor workflows.
* The plugin is not compatible with UE4.

## Installation
* Clone the repo in GameProject/Plugins folder along with submodule dependency<br>
  `git clone --recurse-submodules git@github.com:amuTBKT/ImGuiPlugin.git`
* ImGui and Implot binaries need to be compiled separately. The way unreal build setup works makes it difficult to include third party libraries, so they have to be compiled separately.<br>
  `Source/Thirdparty/ImGui/Build.bat`<br>
  Running the batch script should generate required libs under "/ImGuiPlugin/Source/Thirdparty/ImGui/Binaries" and "/ImGuiPlugin/Source/Thirdparty/ImGui/Intermediate" directories.
* Include the plugin in your own modules.
* Feel free to use [ImGuiExamples](https://github.com/amuTBKT/ImGuiExamples) as reference.

## :construction_worker:Usage
[ImGuiExamples](https://github.com/amuTBKT/ImGuiExamples) :sweat_smile:
