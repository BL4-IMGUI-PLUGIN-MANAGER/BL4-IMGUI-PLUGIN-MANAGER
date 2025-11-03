#include <Windows.h>
#include <imgui.h>
#include "../gui-plugin-manager/PluginLib/PluginAPI.h"
#include "../gui-plugin-manager/PluginLib/PluginHelpers.h"
#include "../gui-plugin-manager/PluginLib/StateManager.h"
#include "../gui-plugin-manager/PluginLib/HotkeyManager.h"

// SDK includes for game function testing
#include "../plugin_manager_base/Plugin_Manager_Base_SDK/SDK/SDK/Basic.hpp"
#include "../plugin_manager_base/Plugin_Manager_Base_SDK/SDK/SDK/CoreUObject_classes.hpp"
#include "../plugin_manager_base/Plugin_Manager_Base_SDK/SDK/SDK/Engine_classes.hpp"

class TestPlugin : public PluginAPI::IPlugin {
private:
    const PluginAPI::MasterAPI* m_API = nullptr;
    const char* m_Name = "Test Plugin";
    const char* m_Version = "1.0.0";
    const char* m_Author = "Plugin Developer";
    const char* m_Description = "Example plugin demonstrating the new plugin system";

    PluginAPI::StateManager m_StateManager{"TestPlugin"};

    int m_ClickCount = 0;
    bool m_TestCheckbox = false;
    float m_TestSlider = 0.5f;
    bool m_ShowOverlay = false;
    bool m_ShowTestText = false;
    ImVec2 m_TestTextPos = ImVec2(200, 200);

    int m_TestKey = VK_F6;
    bool m_TestKeyCtrl = false;
    bool m_TestKeyShift = false;
    bool m_TestKeyAlt = false;
    bool m_TestKeyListening = false;


public:
    TestPlugin(const PluginAPI::MasterAPI* api) : m_API(api) {
    }

    const char* GetName() override {
        return m_Name;
    }

    const char* GetVersion() override {
        return m_Version;
    }

    const char* GetAuthor() override {
        return m_Author;
    }

    const char* GetDescription() override {
        return m_Description;
    }

    const char* GetTabName() override {
        return "Test Tab";
    }

    bool IsTabEnabled() override {
        return true;
    }

    bool OnLoad() override {
        m_API->LogInfo("[TestPlugin] Plugin loaded successfully!");
        return true;
    }

    void OnUnload() override {
        m_API->LogInfo("[TestPlugin] Plugin unloaded.");
    }

    void OnFrame() override {
        m_StateManager.Update();

        if (m_API->IsMenuOpen() || m_TestKeyListening) return;

        static bool wasTestKeyPressed = false;
        HotkeyManager::Hotkey testHotkey(m_TestKey, m_TestKeyCtrl, m_TestKeyShift, m_TestKeyAlt);
        bool isTestKeyPressed = HotkeyManager::IsHotkeyPressed(testHotkey);

        if (isTestKeyPressed && !wasTestKeyPressed) {
            char buffer[256];
            sprintf_s(buffer, "[TestPlugin] Custom hotkey pressed: %s", HotkeyManager::GetHotkeyString(m_TestKey, m_TestKeyCtrl, m_TestKeyShift, m_TestKeyAlt));
            m_API->LogInfo(buffer);
        }
        wasTestKeyPressed = isTestKeyPressed;
    }

    void RenderUI() override {
        ImGuiContext* ctx = m_API->GetImGuiContext();
        if (!ctx) {
            return;
        }
        ImGui::SetCurrentContext(ctx);

        ImGui::TextColored(ImVec4(0.0f, 1.0f, 1.0f, 1.0f), "=== Test Plugin UI ===");
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        ImGui::Text("This is an example plugin demonstrating the plugin system.");
        ImGui::Spacing();

        if (ImGui::Button("Click Me!", ImVec2(200, 40))) {
            m_ClickCount++;
            char buffer[256];
            sprintf_s(buffer, "[TestPlugin] Button clicked %d times!", m_ClickCount);
            m_API->LogInfo(buffer);
        }

        ImGui::SameLine();
        ImGui::Text("Clicks: %d", m_ClickCount);
        ImGui::Spacing();

        if (m_StateManager.Checkbox("Test Checkbox", &m_TestCheckbox)) {
            m_API->LogInfo(m_TestCheckbox ? "[TestPlugin] Checkbox enabled" : "[TestPlugin] Checkbox disabled");
        }
        ImGui::Spacing();

        if (m_StateManager.SliderFloat("Test Slider", &m_TestSlider, 0.0f, 1.0f)) {
            char buffer[256];
            sprintf_s(buffer, "[TestPlugin] Slider value: %.2f", m_TestSlider);
            m_API->LogInfo(buffer);
        }
        ImGui::Spacing();

        ImGui::Separator();
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Press F5 anywhere in the game to test hotkeys");

        ImGui::Spacing();
        ImGui::Separator();

        ImGui::TextColored(ImVec4(0.0f, 1.0f, 1.0f, 1.0f), "Independent Overlay:");
        if (m_StateManager.Checkbox("Show Always-Visible Overlay", &m_ShowOverlay)) {
            m_API->LogInfo(m_ShowOverlay ? "[TestPlugin] Overlay enabled" : "[TestPlugin] Overlay disabled");
        }
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "This overlay will be visible even when menu is closed");

        ImGui::Spacing();

        if (ImGui::Button("Toggle Test Text", ImVec2(200, 30))) {
            m_ShowTestText = !m_ShowTestText;
            m_API->LogInfo(m_ShowTestText ? "[TestPlugin] Test text enabled" : "[TestPlugin] Test text disabled");
        }
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Movable when menu open, uninteractable when closed");

        ImGui::Spacing();
        ImGui::Separator();

        ImGui::TextColored(ImVec4(0.0f, 1.0f, 1.0f, 1.0f), "Test Keybind:");
        m_StateManager.Keybind("Custom Hotkey", &m_TestKey, &m_TestKeyCtrl, &m_TestKeyShift, &m_TestKeyAlt, &m_TestKeyListening);
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Press the hotkey outside the menu to see console message");

        ImGui::Spacing();
        ImGui::Separator();

        ImGui::TextColored(ImVec4(0.0f, 1.0f, 1.0f, 1.0f), "Plugin Features:");
        ImGui::Text("Checkbox State: %s", m_TestCheckbox ? "ON" : "OFF");
        ImGui::Text("Slider Value: %.2f", m_TestSlider);
        ImGui::Spacing();

        ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "Note: SDK features require SDK integration");
        ImGui::Text("Enable SDK includes in the source to use SDK functionality");

    }

    void RenderIndependent() override {
        ImGuiContext* ctx = m_API->GetImGuiContext();
        if (!ctx) {
            return;
        }
        ImGui::SetCurrentContext(ctx);

        ImGuiIO& io = ImGui::GetIO();
        bool menuIsOpen = io.MouseDrawCursor;

        if (m_ShowOverlay) {
            ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav;
            ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_Always);
            ImGui::SetNextWindowBgAlpha(0.35f);

            if (ImGui::Begin("TestPlugin Overlay", nullptr, window_flags)) {
                ImGui::Text("TestPlugin Independent Overlay");
                ImGui::Separator();
                ImGui::Text("This text is always visible!");
                ImGui::Text("Button clicks: %d", m_ClickCount);
                ImGui::Separator();
                ImGui::Text("This overlay shows even when the menu is closed!");
            }
            ImGui::End();
        }

        if (m_ShowTestText) {
            ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings;

            if (!menuIsOpen) {
                window_flags |= ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav;
            }

            if (menuIsOpen) {
                ImGui::SetNextWindowPos(m_TestTextPos, ImGuiCond_FirstUseEver);
            } else {
                ImGui::SetNextWindowPos(m_TestTextPos, ImGuiCond_Always);
            }

            ImGui::SetNextWindowBgAlpha(0.5f);

            if (ImGui::Begin("TestText", nullptr, window_flags)) {
                ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "Test");
                ImGui::Separator();
                ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), menuIsOpen ? "Movable" : "Locked");

                if (menuIsOpen) {
                    m_TestTextPos = ImGui::GetWindowPos();
                }
            }
            ImGui::End();
        }
    }
};

PLUGIN_EXPORT PluginAPI::IPlugin* CreatePlugin(const PluginAPI::MasterAPI* api) {
    return new TestPlugin(api);
}

BOOL WINAPI DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    return TRUE;
}
