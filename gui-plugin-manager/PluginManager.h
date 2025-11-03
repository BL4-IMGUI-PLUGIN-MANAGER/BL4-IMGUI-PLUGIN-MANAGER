#pragma once
#include "PluginLib/PluginAPI.h"
#include <vector>
#include <string>
#include <Windows.h>
#include "imgui/imgui.h"

class PluginManager {
private:
    struct LoadedPlugin {
        HMODULE moduleHandle;
        PluginAPI::IPlugin* instance;
        std::string dllPath;
        bool isEnabled;
    };

    std::vector<LoadedPlugin> m_Plugins;
    PluginAPI::MasterAPI m_MasterAPI;

public:
    ImGuiContext* m_ImGuiContext;

private:

    PluginManager();
    ~PluginManager();

    // Helper methods
    void SetupMasterAPI();

public:
    // Singleton
    static PluginManager& Get();

    // Plugin management
    void Initialize(ImGuiContext* imguiCtx);
    void Shutdown();
    void LoadPluginsFromDirectory(const char* directory);
    bool LoadPlugin(const char* dllPath);
    void ReloadPlugins();

    // Called every frame
    void UpdatePlugins();

    // UI Rendering
    void RenderPluginTabs();
    void RenderIndependentOverlays();

    // Getters
    size_t GetPluginCount() const { return m_Plugins.size(); }
    PluginAPI::IPlugin* GetPlugin(size_t index);
    const std::vector<LoadedPlugin>& GetPlugins() const { return m_Plugins; }

    // Master API
    const PluginAPI::MasterAPI* GetMasterAPI() const { return &m_MasterAPI; }
};
