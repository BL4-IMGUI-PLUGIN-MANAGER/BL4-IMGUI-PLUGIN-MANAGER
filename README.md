# Borderlands 4 Plugin Manager

A comprehensive plugin system for Borderlands 4 with DirectX 12 rendering support, ImGui interface, and extensible plugin architecture.

## Prerequisites

- Visual Studio 2022 with v143 toolset
- Windows 10 SDK
- C++20 or later support
- Dumper-7 for SDK generation

## SDK Dump Instructions

**IMPORTANT**: SDK files are NOT included in this repository due to copyright and size constraints. You must dump the SDK yourself using Dumper-7.

### Dumping the SDK

1. Download Dumper-7 from: https://github.com/Encryqed/Dumper-7
2. Follow the Borderlands 4-specific instructions at: https://github.com/Encryqed/Dumper-7/issues/407
3. Launch Borderlands 4
4. Inject Dumper-7 into the game process
5. Wait for the dump to complete (SDK files will be generated)

### Installing the SDK

After dumping, copy **ALL** generated SDK files from /CppSDK/  to:

```
plugin_manager_base/Plugin_Manager_Base_SDK/SDK/
```

The SDK folder should contain files like:
- `Basic.hpp` / `Basic.cpp`
- `CoreUObject_classes.hpp` / `CoreUObject_structs.hpp` / `CoreUObject_functions.cpp`
- `Engine_classes.hpp` / `Engine_structs.hpp` / `Engine_functions.cpp`
- `GbxGame_classes.hpp` / `GbxGame_structs.hpp` / `GbxGame_functions.cpp`
- And ALL other game-specific SDK files

**Note**: Only `plugin_manager_base` compiles the SDK `.cpp` files. The GUI and plugins only include SDK headers (`.hpp` files) - SDK functions are mostly inlined or accessed through the base DLL.

## Build Order

Build projects in this exact order:

### 1. Plugin Manager Base (plugin_manager_base)

The core plugin manager that handles hook registration and plugin lifecycle.

```bash
cd plugin_manager_base
"C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe" Plugin_Manager_Base.sln -p:Configuration=Release -p:Platform=x64
```

**Output**: `x64\Release\dwmapi.dll` (DLL hijacking proxy)

### 2. Plugin Manager GUI (gui-plugin-manager)

The DirectX 12 rendering layer and ImGui interface.

```bash
cd gui-plugin-manager
"C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe" Plugin_Manager_GUI.sln -p:Configuration=Release -p:Platform=x64
```

**Output**: `x64\Release\Plugin_Manager.dll`

### 3. Test Plugin (TestPlugin_Template)

Example plugin template demonstrating the plugin API.

```bash
cd TestPlugin_Template
"C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe" TestPlugin.sln -p:Configuration=Release -p:Platform=x64
```

**Output**: `x64\Release\TestPlugin.dll`

## Project Structure

```
sdk_CURRENT_claude/
├── plugin_manager_base/          # Core plugin manager (DLL hijacking)
│   └── Plugin_Manager_Base_SDK/  # Source code and SDK files
├── gui-plugin-manager/           # DirectX 12 GUI layer
│   ├── imgui/                    # ImGui library
│   └── PluginLib/                # Plugin API headers
├── TestPlugin_Template/          # Example plugin
├── 4tilities/                    # Utilities plugin (optional)
├── ObjectViewer/                 # Object viewer plugin (optional)
└── x64/Release/                  # Build output directory
```

## Installation

1. Build all projects in order (see Build Order above)
2. Copy `dwmapi.dll` from `x64\Release\` to `Borderlands 4\OakGame\Binaries\Win64\`
3. Create a folder `Borderlands 4\OakGame\Binaries\Win64\Plugin_Manager\`
4. Copy `Plugin_Manager.dll` to `Borderlands 4\OakGame\Binaries\Win64\Plugin_Manager\`
5. Copy any plugins (e.g., `TestPlugin.dll`) to `Borderlands 4\OakGame\Binaries\Win64\Plugin_Manager\Plugins`
6. Launch Borderlands 4

**Note**: The plugin manager uses DLL hijacking via `dwmapi.dll`. Windows will load our DLL instead of the system's Desktop Window Manager API DLL.

### Directory Structure After Installation
```
Borderlands 4/
├── Borderlands4.exe
├── dwmapi.dll                          # Plugin Manager Base (proxies to system dwmapi)
└── OakGame/
    └── Binaries/
        └── Win64/
            ├── Plugin_Manager/
            │   └── Plugin_Manager.dll  # GUI and plugin system
            │   └──Plugins/
            │       └──TestPlugin.dll          # Example plugin
            │       └── (other plugin DLLs)
```

## Usage

- **F1**: Toggle plugin menu

## Creating Custom Plugins

Use `TestPlugin_Template` as a starting point:

1. Copy the TestPlugin_Template folder
2. Rename the project and files
3. Implement the `IPlugin` interface:
   - `OnInit()`: Called when plugin loads
   - `OnFrame()`: Called every frame
   - `OnRender()`: Render ImGui UI
   - `OnShutdown()`: Cleanup when unloading

4. Add your plugin to the build order and rebuild

## Plugin API

Plugins have access to:
- **ImGui**: Full ImGui API for UI rendering
- **SDK**: Complete Borderlands 4 game SDK
- **Hook System**: Register function hooks via HookRegistry
- **State Manager**: Persistent settings storage
- **Hotkey Manager**: Keyboard input handling
- **Master API**: Access to plugin manager features

## Configuration

### Plugin Manager Settings

The plugin manager GUI creates a `plugin_manager_config.ini` file with the following settings:

```ini
[GUI]
ShowDebugWindow=false     # Show debug log window on startup
MenuHotkey=F1            # Hotkey to toggle the plugin menu

[Logging]
EnableLogging=true       # Enable logging to file
LogFilePath=Plugin_Manager_GUI.log
```

You can edit these settings through the in-game Settings menu (View → Settings) or manually edit the INI file.

### Plugin Settings

Individual plugin settings are stored in `config.ini` files using the StateManager system.

## TODO / Roadmap

### High Priority
- [ ] **Expand hooking system**: Add hook IDs and unhooking capabilities
- [ ] **Hot reload support**: Reload plugins without restarting the game
- [ ] **Signature scanning**: Add pattern scanning for finding game functions dynamically

Contributions welcome! Feel free to submit PRs for any of these features or suggest new ones.

## Troubleshooting

### Visual Studio IntelliSense Errors
If you see red squiggles or IntelliSense errors in Visual Studio (errors about `std::invocable`, `requires`, `char8_t`, etc.), these are **false positives** and can be safely ignored. The project uses C++20 features that IntelliSense sometimes struggles to parse.

**The project will compile successfully** despite these IntelliSense errors. The MSBuild command-line builds work fine.

To reduce IntelliSense errors:
1. Ensure "C++ Language Standard" is set to `/std:c++20` or `/std:c++latest` in project properties
2. Close Visual Studio and delete the `.vs` folder (hidden) in the project directory
3. Reopen Visual Studio and let IntelliSense rebuild

### SDK Not Found Errors
- Ensure you've dumped the SDK using Dumper-7
- Verify SDK files are in `plugin_manager_base/Plugin_Manager_Base_SDK/SDK/`
- Check that all required SDK files are present

### DLL Not Loading
- Ensure build order was followed correctly
- Check that `dwmapi.dll` is in the Borderlands 4 root directory (where Borderlands4.exe is)
- Verify `Plugin_Manager.dll` is in `OakGame\Binaries\Win64\Plugin_Manager\` subfolder
- Check the log file in `Plugin_Manager/settings/Plugin_Manager_GUI.log` for error messages

### Menu Not Appearing
- Press F1 to toggle the menu - sometimes needs pressing a few times
- Check that DirectX 12 hooks initialized (debug logs)
- Ensure no conflicting overlays are present

## License

This project is for educational and research purposes only.

## Credits

Built with:
- ImGui (https://github.com/ocornut/imgui)
- universal imgui hook (https://github.com/Sh0ckFR/Universal-Dear-ImGui-Hook)
- MinHook (https://github.com/TsudaKageyu/minhook)
- Dumper-7 (https://github.com/Encryqed/Dumper-7)
