#include "stdafx.h"
#include "PluginManager.h"
#include "PluginLib/PluginAPI.h"

namespace menu {
    std::atomic<bool> isOpen{false};
    static bool pluginManagerInitialized = false;
    static bool lastMenuState = false;
    static bool showDebugWindow = false;
    static bool showSettingsWindow = false;

    void Init() {
        bool isMenuOpen = isOpen.load(std::memory_order_relaxed);

        // Removed recursive logging - this was being called every frame
        // DebugLog("[menu] Rendering menu. isOpen=%d\n", (int)isMenuOpen);

        ImGuiIO& io = ImGui::GetIO();
        io.MouseDrawCursor = isMenuOpen;

        if (!pluginManagerInitialized) {
            pluginManagerInitialized = true;
            PluginManager::Get().Initialize(ImGui::GetCurrentContext());
            DebugLog("[menu] PluginManager initialized.\n");
            showDebugWindow = false; // Default off
        }

        // Only log when menu state changes (not every frame)
        if (lastMenuState != isMenuOpen) {
            DebugLog("[menu] Menu state changed. isOpen=%d\n", (int)isMenuOpen);
        }
        lastMenuState = isMenuOpen;

        PluginManager::Get().UpdatePlugins();
        PluginManager::Get().RenderIndependentOverlays();

        if (showDebugWindow) {
            ImGui::SetNextWindowSize(ImVec2(700, 400), ImGuiCond_FirstUseEver);
            ImGui::Begin("Debug Log", &showDebugWindow);

            if (ImGui::Button("Clear Log")) {
                GUILogger::Get().ClearLogBuffer();
            }

            ImGui::Separator();
            ImGui::BeginChild("LogScrolling", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);

            const auto& logBuffer = GUILogger::Get().GetLogBuffer();
            for (const auto& entry : logBuffer) {
                ImGui::TextUnformatted(("[" + entry.timestamp + "] " + entry.message).c_str());
            }

            if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
                ImGui::SetScrollHereY(1.0f);
            }

            ImGui::EndChild();
            ImGui::End();
        }

        if (showSettingsWindow) {
            ImGui::SetNextWindowSize(ImVec2(500, 200), ImGuiCond_FirstUseEver);
            ImGui::Begin("Settings", &showSettingsWindow);

            if (ImGui::Checkbox("Show Debug Window", &showDebugWindow)) {
                DebugLog("[Settings] Debug window: %s\n", showDebugWindow ? "enabled" : "disabled");
            }

            ImGui::Separator();
            ImGui::Text("Hotkeys:");
            ImGui::Text("Menu Toggle: F1 (hardcoded)");

            ImGui::End();
        }

        if (!isMenuOpen) {
            return;
        }

        static bool styled = false;
        if (!styled) {
            ImGui::StyleColorsDark();
            ImVec4* colors = ImGui::GetStyle().Colors;
            colors[ImGuiCol_WindowBg] = ImVec4(0, 0, 0, 0.8f);
            colors[ImGuiCol_Header] = ImVec4(0.2f, 0.2f, 0.2f, 0.8f);
            colors[ImGuiCol_HeaderHovered] = ImVec4(0.3f, 0.3f, 0.3f, 0.8f);
            colors[ImGuiCol_Button] = ImVec4(0.26f, 0.59f, 0.98f, 0.4f);
            colors[ImGuiCol_ButtonHovered] = ImVec4(0.26f, 0.59f, 0.98f, 1.0f);
            colors[ImGuiCol_ButtonActive] = ImVec4(0.06f, 0.53f, 0.98f, 1.0f);
            styled = true;
            DebugLog("[menu] Style applied.\n");
        }

        ImGuiWindowFlags flags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_MenuBar;
        ImGui::SetNextWindowSize(ImVec2(800, 600), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowPos(ImVec2(25, 25), ImGuiCond_FirstUseEver);

        ImGui::Begin("Plugin Manager", &isMenuOpen, flags);

        if (ImGui::BeginMenuBar()) {
            if (ImGui::BeginMenu("View")) {
                if (ImGui::MenuItem("Debug Log", nullptr, showDebugWindow)) {
                    showDebugWindow = !showDebugWindow;
                }
                if (ImGui::MenuItem("Settings", nullptr, showSettingsWindow)) {
                    showSettingsWindow = !showSettingsWindow;
                }
                ImGui::EndMenu();
            }
            ImGui::EndMenuBar();
        }

        PluginManager::Get().RenderPluginTabs();

        ImGui::End();

        isOpen.store(isMenuOpen, std::memory_order_relaxed);
    }
}
