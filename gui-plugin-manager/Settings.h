#pragma once

#include <string>
#include <fstream>
#include <sstream>
#include <algorithm>

class Settings {
public:
    struct Config {
        bool showDebugWindow = false;
        int menuHotkey = VK_F1;
        bool enableLogging = true;
        std::string logFilePath = "Plugin_Manager/settings/Plugin_Manager_GUI.log";
    };

    static Settings& Get() {
        static Settings instance;
        return instance;
    }

    bool Load(const std::string& configPath = "Plugin_Manager/settings/plugin_manager_config.ini");
    bool Save(const std::string& configPath = "Plugin_Manager/settings/plugin_manager_config.ini");

    const Config& GetConfig() const { return config; }
    void SetConfig(const Config& newConfig) { config = newConfig; }

    bool GetShowDebugWindow() const { return config.showDebugWindow; }
    void SetShowDebugWindow(bool show) { config.showDebugWindow = show; }

    int GetMenuHotkey() const { return config.menuHotkey; }
    void SetMenuHotkey(int key) { config.menuHotkey = key; }

    bool GetEnableLogging() const { return config.enableLogging; }
    void SetEnableLogging(bool enable) { config.enableLogging = enable; }

    std::string VirtualKeyToString(int vkey);

private:
    Settings() = default;
    ~Settings() = default;
    Settings(const Settings&) = delete;
    Settings& operator=(const Settings&) = delete;

    bool ParseBool(const std::string& value);
    int ParseVirtualKey(const std::string& value);
    std::string Trim(const std::string& str);

    Config config;
};
