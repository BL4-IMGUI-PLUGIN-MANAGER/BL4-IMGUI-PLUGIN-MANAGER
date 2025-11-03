#pragma once
#include <functional>
#include <string>
#include <vector>
#include "SDK.h"

// Forward declarations
struct ImGuiContext;

namespace PluginAPI {

    // Version for API compatibility checking
    constexpr int API_VERSION = 1;

    // Plugin interface that all plugins must implement
    class IPlugin {
    public:
        virtual ~IPlugin() = default;

        // Plugin metadata
        virtual const char* GetName() = 0;
        virtual const char* GetVersion() = 0;
        virtual const char* GetAuthor() = 0;
        virtual const char* GetDescription() = 0;

        // Lifecycle
        virtual bool OnLoad() = 0;              // Called when plugin is loaded
        virtual void OnUnload() = 0;            // Called when plugin is unloaded
        virtual void OnFrame() = 0;             // Called every frame (for hotkeys, etc)

        // UI Rendering - called when plugin's tab is active
        virtual void RenderUI() = 0;

        // Independent rendering - called every frame regardless of menu state
        // Use this for always-visible overlays
        virtual void RenderIndependent() {}

        // Tab customization
        virtual const char* GetTabName() = 0;   // Name shown on the tab
        virtual bool IsTabEnabled() = 0;        // Can disable tab dynamically

        // Sub-tabs support (optional - plugins can override to add sub-tabs within their main tab)
        virtual bool HasSubTabs() { return false; }
        virtual int GetSubTabCount() { return 0; }
        virtual const char* GetSubTabName(int index) { return ""; }
        virtual void RenderSubTab(int index) {}
    };

    // Utility functions provided by the Plugin Loader DLL
    struct MasterAPI {
        // Logging
        void (*Log)(const char* level, const char* message);
        void (*LogInfo)(const char* message);
        void (*LogWarning)(const char* message);
        void (*LogError)(const char* message);

        // ImGui context (shared between master and plugins)
        ImGuiContext* (*GetImGuiContext)();

        // Game thread execution (for SDK calls and game-safe operations)
        void (*ExecuteOnGameThread)(std::function<void()> func);

        // Plugin management
        void (*ReloadPlugins)();

        // Menu state
        bool (*IsMenuOpen)();

        // SDK Operations (implemented in Plugin_Manager.dll where SDK is available)
        // These automatically execute on game thread
        void (*TogglePhotoMode)();                              // Pause game and toggle debug camera
        void (*ToggleHUD)();                                    // Toggle HUD visibility
        void (*ToggleDamageNumbers)(bool enable);               // Enable/disable damage numbers display
        void (*TeleportToLocation)(float x, float y, float z);  // Teleport player to location

        // Version
        int apiVersion;
    };

    // Export function that plugins must implement
    // extern "C" __declspec(dllexport) IPlugin* CreatePlugin(const MasterAPI* api);
}

// Macro to simplify plugin creation
#define PLUGIN_EXPORT extern "C" __declspec(dllexport)

// Helper macro for logging
#define PLUGIN_LOG(api, msg) (api)->LogInfo(msg)
#define PLUGIN_LOG_ERROR(api, msg) (api)->LogError(msg)
#define PLUGIN_LOG_WARNING(api, msg) (api)->LogWarning(msg)
