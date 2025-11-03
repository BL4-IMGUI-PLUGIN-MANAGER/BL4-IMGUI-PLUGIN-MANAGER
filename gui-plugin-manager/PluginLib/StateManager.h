#pragma once
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <variant>
#include <filesystem>
#include <fstream>
#include <sstream>
#include "../imgui/imgui.h"
#include "HotkeyManager.h"

namespace PluginAPI {

// Keybind structure to store key + modifiers
struct KeybindValue {
    int key = 0;
    bool ctrl = false;
    bool shift = false;
    bool alt = false;

    KeybindValue() = default;
    KeybindValue(int k, bool c = false, bool s = false, bool a = false)
        : key(k), ctrl(c), shift(s), alt(a) {}
};

// Supported state types
using StateValue = std::variant<bool, int, float, std::string, KeybindValue>;

// State entry with metadata
struct StateEntry {
    std::string widgetLabel;
    std::string widgetType;
    StateValue value;
    bool dirty = false;
};

class StateManager {
private:
    std::string m_PluginName;
    std::string m_ConfigPath;
    std::unordered_map<std::string, StateEntry> m_States;
    std::unordered_set<std::string> m_LoadedKeys; // Track which keys have been loaded
    bool m_AutoSave = true;
    bool m_AnyDirty = false; // Single flag instead of iterating map
    float m_LastSaveTime = 0.0f;
    float m_SaveInterval = 2.0f; // Save every 2 seconds if dirty

    // Generate unique key for widget (inline to avoid function call overhead)
    inline void GenerateKey(std::string& outKey, const char* label, const char* type) const {
        outKey.clear();
        outKey.reserve(strlen(type) + strlen(label) + 2);
        outKey.append(type);
        outKey.append("::");
        outKey.append(label);
    }

    // Parse INI file
    void LoadFromFile() {
        std::ifstream file(m_ConfigPath);
        if (!file.is_open()) {
            return; // File doesn't exist yet
        }

        std::string line;
        std::string currentSection;

        while (std::getline(file, line)) {
            // Trim whitespace
            line.erase(0, line.find_first_not_of(" \t\r\n"));
            line.erase(line.find_last_not_of(" \t\r\n") + 1);

            if (line.empty() || line[0] == ';' || line[0] == '#') {
                continue; // Skip empty lines and comments
            }

            // Section header
            if (line[0] == '[' && line.back() == ']') {
                currentSection = line.substr(1, line.length() - 2);
                continue;
            }

            // Key=Value pair
            size_t equalPos = line.find('=');
            if (equalPos != std::string::npos) {
                std::string key = line.substr(0, equalPos);
                std::string value = line.substr(equalPos + 1);

                // Trim key and value
                key.erase(key.find_last_not_of(" \t\r\n") + 1);
                value.erase(0, value.find_first_not_of(" \t\r\n"));

                // Parse based on section (with exception safety)
                try {
                    if (currentSection == "Checkboxes") {
                        bool boolVal = (value == "true" || value == "1");
                        m_States[key] = StateEntry{ key, "Checkbox", boolVal, false };
                    }
                    else if (currentSection == "Sliders_Int") {
                        int intVal = std::stoi(value);
                        m_States[key] = StateEntry{ key, "SliderInt", intVal, false };
                    }
                    else if (currentSection == "Sliders_Float") {
                        float floatVal = std::stof(value);
                        m_States[key] = StateEntry{ key, "SliderFloat", floatVal, false };
                    }
                    else if (currentSection == "Keybinds") {
                        // Parse format: "key,ctrl,shift,alt" e.g., "114,1,0,0" for Ctrl+F3
                        KeybindValue keybind;
                        size_t pos1 = value.find(',');
                        if (pos1 != std::string::npos) {
                            keybind.key = std::stoi(value.substr(0, pos1));
                            size_t pos2 = value.find(',', pos1 + 1);
                            if (pos2 != std::string::npos) {
                                keybind.ctrl = (value.substr(pos1 + 1, pos2 - pos1 - 1) == "1");
                                size_t pos3 = value.find(',', pos2 + 1);
                                if (pos3 != std::string::npos) {
                                    keybind.shift = (value.substr(pos2 + 1, pos3 - pos2 - 1) == "1");
                                    keybind.alt = (value.substr(pos3 + 1) == "1");
                                }
                            }
                        } else {
                            // Backward compatibility: just a key number
                            keybind.key = std::stoi(value);
                        }
                        m_States[key] = StateEntry{ key, "Keybind", keybind, false };
                    }
                } catch (const std::exception&) {
                    // Skip invalid entries - corrupted INI won't crash
                    continue;
                }
            }
        }

        file.close();
    }

    // Write INI file
    void SaveToFile() {
        // Create settings directory if it doesn't exist
        std::filesystem::path settingsDir = std::filesystem::path(m_ConfigPath).parent_path();
        if (!std::filesystem::exists(settingsDir)) {
            std::filesystem::create_directories(settingsDir);
        }

        std::ofstream file(m_ConfigPath);
        if (!file.is_open()) {
            return;
        }

        // Group by type
        std::unordered_map<std::string, std::vector<std::string>> sections;
        for (const auto& [key, entry] : m_States) {
            std::string section;
            if (entry.widgetType == "Checkbox") {
                section = "Checkboxes";
            }
            else if (entry.widgetType == "SliderInt") {
                section = "Sliders_Int";
            }
            else if (entry.widgetType == "SliderFloat") {
                section = "Sliders_Float";
            }
            else if (entry.widgetType == "Keybind") {
                section = "Keybinds";
            }
            else {
                section = "Other";
            }

            sections[section].push_back(key);
        }

        // Write sections
        for (const auto& [section, keys] : sections) {
            file << "[" << section << "]\n";

            for (const auto& key : keys) {
                const auto& entry = m_States[key];
                file << key << "=";

                if (std::holds_alternative<bool>(entry.value)) {
                    file << (std::get<bool>(entry.value) ? "true" : "false");
                }
                else if (std::holds_alternative<int>(entry.value)) {
                    file << std::get<int>(entry.value);
                }
                else if (std::holds_alternative<float>(entry.value)) {
                    file << std::get<float>(entry.value);
                }
                else if (std::holds_alternative<std::string>(entry.value)) {
                    file << std::get<std::string>(entry.value);
                }
                else if (std::holds_alternative<KeybindValue>(entry.value)) {
                    const auto& kb = std::get<KeybindValue>(entry.value);
                    file << kb.key << "," << (kb.ctrl ? "1" : "0") << ","
                         << (kb.shift ? "1" : "0") << "," << (kb.alt ? "1" : "0");
                }

                file << "\n";
            }

            file << "\n";
        }

        file.close();

        // Clear dirty flags
        for (auto& [key, entry] : m_States) {
            entry.dirty = false;
        }
    }

public:
    StateManager(const std::string& pluginName) : m_PluginName(pluginName) {
        // Construct config path: Plugin_Manager/settings/PluginName.ini
        m_ConfigPath = "Plugin_Manager/settings/" + pluginName + ".ini";

        // Reserve capacity to avoid rehashing (estimate ~20 widgets per plugin)
        m_States.reserve(20);
        m_LoadedKeys.reserve(20);

        LoadFromFile();
    }

    ~StateManager() {
        if (m_AutoSave) {
            SaveToFile();
        }
    }

    // Update function - call every frame (optimized)
    void Update() {
        if (!m_AutoSave || !m_AnyDirty) return;

        float currentTime = ImGui::GetTime();
        if (currentTime - m_LastSaveTime > m_SaveInterval) {
            SaveToFile();
            m_LastSaveTime = currentTime;
            m_AnyDirty = false;
        }
    }

    // Wrapper for ImGui::Checkbox with state tracking (optimized)
    bool Checkbox(const char* label, bool* v) {
        // Use thread_local to reuse string buffer and avoid per-frame allocations
        thread_local std::string key;
        GenerateKey(key, label, "Checkbox");

        // Load saved state on first use only (tracked by m_LoadedKeys set)
        if (m_LoadedKeys.find(key) == m_LoadedKeys.end()) {
            auto it = m_States.find(key);
            if (it != m_States.end() && std::holds_alternative<bool>(it->second.value)) {
                *v = std::get<bool>(it->second.value);
            }
            m_LoadedKeys.insert(key); // Mark as loaded
        }

        bool changed = ImGui::Checkbox(label, v);

        if (changed) {
            m_States[key] = StateEntry{ label, "Checkbox", *v, true };
            m_AnyDirty = true;
        }

        return changed;
    }

    // Wrapper for ImGui::SliderFloat (optimized)
    bool SliderFloat(const char* label, float* v, float v_min, float v_max, const char* format = "%.3f", ImGuiSliderFlags flags = 0) {
        thread_local std::string key;
        GenerateKey(key, label, "SliderFloat");

        if (m_LoadedKeys.find(key) == m_LoadedKeys.end()) {
            auto it = m_States.find(key);
            if (it != m_States.end() && std::holds_alternative<float>(it->second.value)) {
                *v = std::get<float>(it->second.value);
            }
            m_LoadedKeys.insert(key);
        }

        bool changed = ImGui::SliderFloat(label, v, v_min, v_max, format, flags);

        if (changed) {
            m_States[key] = StateEntry{ label, "SliderFloat", *v, true };
            m_AnyDirty = true;
        }

        return changed;
    }

    // Wrapper for ImGui::SliderInt (optimized)
    bool SliderInt(const char* label, int* v, int v_min, int v_max, const char* format = "%d", ImGuiSliderFlags flags = 0) {
        thread_local std::string key;
        GenerateKey(key, label, "SliderInt");

        if (m_LoadedKeys.find(key) == m_LoadedKeys.end()) {
            auto it = m_States.find(key);
            if (it != m_States.end() && std::holds_alternative<int>(it->second.value)) {
                *v = std::get<int>(it->second.value);
            }
            m_LoadedKeys.insert(key);
        }

        bool changed = ImGui::SliderInt(label, v, v_min, v_max, format, flags);

        if (changed) {
            m_States[key] = StateEntry{ label, "SliderInt", *v, true };
            m_AnyDirty = true;
        }

        return changed;
    }

    // Wrapper for ImGui::DragFloat (optimized)
    bool DragFloat(const char* label, float* v, float v_speed = 1.0f, float v_min = 0.0f, float v_max = 0.0f, const char* format = "%.3f", ImGuiSliderFlags flags = 0) {
        thread_local std::string key;
        GenerateKey(key, label, "DragFloat");

        if (m_LoadedKeys.find(key) == m_LoadedKeys.end()) {
            auto it = m_States.find(key);
            if (it != m_States.end() && std::holds_alternative<float>(it->second.value)) {
                *v = std::get<float>(it->second.value);
            }
            m_LoadedKeys.insert(key);
        }

        bool changed = ImGui::DragFloat(label, v, v_speed, v_min, v_max, format, flags);

        if (changed) {
            m_States[key] = StateEntry{ label, "DragFloat", *v, true };
            m_AnyDirty = true;
        }

        return changed;
    }

    // Wrapper for ImGui::DragInt (optimized)
    bool DragInt(const char* label, int* v, float v_speed = 1.0f, int v_min = 0, int v_max = 0, const char* format = "%d", ImGuiSliderFlags flags = 0) {
        thread_local std::string key;
        GenerateKey(key, label, "DragInt");

        if (m_LoadedKeys.find(key) == m_LoadedKeys.end()) {
            auto it = m_States.find(key);
            if (it != m_States.end() && std::holds_alternative<int>(it->second.value)) {
                *v = std::get<int>(it->second.value);
            }
            m_LoadedKeys.insert(key);
        }

        bool changed = ImGui::DragInt(label, v, v_speed, v_min, v_max, format, flags);

        if (changed) {
            m_States[key] = StateEntry{ label, "DragInt", *v, true };
            m_AnyDirty = true;
        }

        return changed;
    }

    // Wrapper for Keybind button with listening state (optimized)
    bool Keybind(const char* label, int* key, bool* ctrl, bool* shift, bool* alt, bool* listening) {
        thread_local std::string stateKey;
        GenerateKey(stateKey, label, "Keybind");

        // Load saved state on first use
        if (m_LoadedKeys.find(stateKey) == m_LoadedKeys.end()) {
            auto it = m_States.find(stateKey);
            if (it != m_States.end() && std::holds_alternative<KeybindValue>(it->second.value)) {
                const auto& kb = std::get<KeybindValue>(it->second.value);
                *key = kb.key;
                *ctrl = kb.ctrl;
                *shift = kb.shift;
                *alt = kb.alt;
            }
            m_LoadedKeys.insert(stateKey);
        }

        bool changed = false;

        // Render the keybind UI
        ImGui::PushID(label);

        // Label
        ImGui::Text("%s", label);

        // Modifier checkboxes
        if (ImGui::Checkbox("Ctrl", ctrl)) changed = true;
        ImGui::SameLine();
        if (ImGui::Checkbox("Shift", shift)) changed = true;
        ImGui::SameLine();
        if (ImGui::Checkbox("Alt", alt)) changed = true;

        // Key button
        std::string buttonText = *listening ? "Press any key..." : HotkeyManager::GetHotkeyString(*key, *ctrl, *shift, *alt);
        if (ImGui::Button(buttonText.c_str(), ImVec2(200, 30))) {
            *listening = true;
        }

        // Listen for key press
        if (*listening) {
            int newKey;
            bool newCtrl, newShift, newAlt;
            if (HotkeyManager::ListenForKeyPress(newKey, newCtrl, newShift, newAlt)) {
                *key = newKey;
                *ctrl = newCtrl;
                *shift = newShift;
                *alt = newAlt;
                *listening = false;
                changed = true;
            }
        }

        ImGui::PopID();

        // Save if changed
        if (changed) {
            KeybindValue kb(*key, *ctrl, *shift, *alt);
            m_States[stateKey] = StateEntry{ label, "Keybind", kb, true };
            m_AnyDirty = true;
        }

        return changed;
    }

    // Manual save
    void Save() {
        SaveToFile();
    }

    // Manual load
    void Load() {
        LoadFromFile();
    }

    // Enable/disable auto-save
    void SetAutoSave(bool enabled) {
        m_AutoSave = enabled;
    }
};

} // namespace PluginAPI
