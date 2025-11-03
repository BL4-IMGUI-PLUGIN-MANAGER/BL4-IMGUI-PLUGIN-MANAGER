#include "stdafx.h"
#include "Settings.h"
#include "GUILogger.h"
#include <filesystem>

bool Settings::Load(const std::string& configPath) {
    // Get the directory where the game exe is located
    std::string fullPath = configPath;
    if (configPath.find(':') == std::string::npos && configPath.find("\\\\") != 0) {
        // Relative path - make it relative to the game executable directory
        char exePath[MAX_PATH];
        GetModuleFileNameA(nullptr, exePath, MAX_PATH);
        std::filesystem::path exeDir = std::filesystem::path(exePath).parent_path();
        fullPath = (exeDir / configPath).string();
    }

    std::ifstream file(fullPath);
    if (!file.is_open()) {
        GUILogger::Get().Log("[Settings] Config file not found, creating default: %s\n", fullPath.c_str());
        return Save(fullPath);
    }

    std::string line;
    std::string currentSection;

    while (std::getline(file, line)) {
        line = Trim(line);

        if (line.empty() || line[0] == ';' || line[0] == '#') {
            continue;
        }

        if (line[0] == '[' && line.back() == ']') {
            currentSection = line.substr(1, line.length() - 2);
            continue;
        }

        size_t eqPos = line.find('=');
        if (eqPos == std::string::npos) {
            continue;
        }

        std::string key = Trim(line.substr(0, eqPos));
        std::string value = Trim(line.substr(eqPos + 1));

        if (currentSection == "GUI") {
            if (key == "ShowDebugWindow") {
                config.showDebugWindow = ParseBool(value);
            }
            else if (key == "MenuHotkey") {
                config.menuHotkey = ParseVirtualKey(value);
            }
        }
        else if (currentSection == "Logging") {
            if (key == "EnableLogging") {
                config.enableLogging = ParseBool(value);
            }
            else if (key == "LogFilePath") {
                config.logFilePath = value;
            }
        }
    }

    file.close();
    GUILogger::Get().Log("[Settings] Configuration loaded from %s\n", fullPath.c_str());
    return true;
}

bool Settings::Save(const std::string& configPath) {
    // Get the directory where the game exe is located
    std::string fullPath = configPath;
    if (configPath.find(':') == std::string::npos && configPath.find("\\\\") != 0) {
        // Relative path - make it relative to the game executable directory
        char exePath[MAX_PATH];
        GetModuleFileNameA(nullptr, exePath, MAX_PATH);
        std::filesystem::path exeDir = std::filesystem::path(exePath).parent_path();
        fullPath = (exeDir / configPath).string();
    }

    std::ofstream file(fullPath, std::ios::trunc);
    if (!file.is_open()) {
        GUILogger::Get().Log("[Settings] Failed to create config file: %s\n", fullPath.c_str());
        return false;
    }

    file << "; Plugin Manager GUI Configuration\n";
    file << "; This file is auto-generated\n\n";

    file << "[GUI]\n";
    file << "; Show debug log window on startup\n";
    file << "ShowDebugWindow=" << (config.showDebugWindow ? "true" : "false") << "\n\n";

    file << "; Hotkey to toggle plugin menu (VK_F1 = F1 key)\n";
    file << "MenuHotkey=" << VirtualKeyToString(config.menuHotkey) << "\n\n";

    file << "[Logging]\n";
    file << "; Enable logging to file\n";
    file << "EnableLogging=" << (config.enableLogging ? "true" : "false") << "\n\n";

    file << "; Path to log file\n";
    file << "LogFilePath=" << config.logFilePath << "\n";

    file.close();
    GUILogger::Get().Log("[Settings] Configuration saved to %s\n", fullPath.c_str());
    return true;
}

bool Settings::ParseBool(const std::string& value) {
    std::string lower = value;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    return lower == "true" || lower == "1" || lower == "yes" || lower == "on";
}

int Settings::ParseVirtualKey(const std::string& value) {
    if (value.find("VK_") == 0 || value.find("0x") == 0) {
        try {
            return std::stoi(value.substr(value.find("x") + 1), nullptr, 16);
        } catch (...) {
            return VK_F1;
        }
    }

    if (value == "F1") return VK_F1;
    if (value == "F2") return VK_F2;
    if (value == "F3") return VK_F3;
    if (value == "F4") return VK_F4;
    if (value == "F5") return VK_F5;
    if (value == "F6") return VK_F6;
    if (value == "F7") return VK_F7;
    if (value == "F8") return VK_F8;
    if (value == "F9") return VK_F9;
    if (value == "F10") return VK_F10;
    if (value == "F11") return VK_F11;
    if (value == "F12") return VK_F12;
    if (value == "INSERT") return VK_INSERT;
    if (value == "DELETE") return VK_DELETE;
    if (value == "HOME") return VK_HOME;
    if (value == "END") return VK_END;

    try {
        return std::stoi(value);
    } catch (...) {
        return VK_F1;
    }
}

std::string Settings::VirtualKeyToString(int vkey) {
    switch (vkey) {
        case VK_F1: return "F1";
        case VK_F2: return "F2";
        case VK_F3: return "F3";
        case VK_F4: return "F4";
        case VK_F5: return "F5";
        case VK_F6: return "F6";
        case VK_F7: return "F7";
        case VK_F8: return "F8";
        case VK_F9: return "F9";
        case VK_F10: return "F10";
        case VK_F11: return "F11";
        case VK_F12: return "F12";
        case VK_INSERT: return "INSERT";
        case VK_DELETE: return "DELETE";
        case VK_HOME: return "HOME";
        case VK_END: return "END";
        default: {
            std::stringstream ss;
            ss << "VK_0x" << std::hex << vkey;
            return ss.str();
        }
    }
}

std::string Settings::Trim(const std::string& str) {
    size_t first = str.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) {
        return "";
    }
    size_t last = str.find_last_not_of(" \t\r\n");
    return str.substr(first, (last - first + 1));
}
