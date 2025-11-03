#include "PluginManager.h"
#include <filesystem>
#include <iostream>
#include <cstdio>
#include <stdarg.h>
#include <thread>
#include <chrono>
#include "../plugin_manager_base/Plugin_Manager_Base_SDK/SimpleHookManager.h"
#include "../plugin_manager_base/Plugin_Manager_Base_SDK/SDK/SDK.hpp"
#include "GUILogger.h"

namespace fs = std::filesystem;

PluginManager& PluginManager::Get() {
    static PluginManager instance;
    return instance;
}

PluginManager::PluginManager() : m_ImGuiContext(nullptr) {
}

PluginManager::~PluginManager() {
    Shutdown();
}

static PluginManager* g_PluginManagerInstance = nullptr;

static void StaticLog(const char* level, const char* message) {
    GUILogger::Get().Log("[%s] %s", level, message);
}

static void StaticLogInfo(const char* message) {
    GUILogger::Get().Log("[INFO] %s", message);
}

static void StaticLogWarning(const char* message) {
    GUILogger::Get().Log("[WARNING] %s", message);
}

static void StaticLogError(const char* message) {
    GUILogger::Get().Log("[ERROR] %s", message);
}

static ImGuiContext* StaticGetImGuiContext() {
    return g_PluginManagerInstance ? g_PluginManagerInstance->m_ImGuiContext : nullptr;
}

static void StaticExecuteOnGameThread(std::function<void()> func) {
    func();
}

static void StaticReloadPlugins() {
    if (g_PluginManagerInstance) {
        g_PluginManagerInstance->ReloadPlugins();
    }
}

static SDK::APlayerController* GetPlayerController() {
    SDK::UWorld* World = SDK::UWorld::GetWorld();
    StaticLogInfo("[SDK] GetPlayerController: Attempting to get world...");
    if (!World) {
        StaticLogError("[SDK] GetPlayerController: World is nullptr");
        return nullptr;
    }
    StaticLogInfo("[SDK] GetPlayerController: World found");

    if (!World->OwningGameInstance) {
        StaticLogError("[SDK] GetPlayerController: OwningGameInstance is nullptr");
        return nullptr;
    }
    StaticLogInfo("[SDK] GetPlayerController: OwningGameInstance found");

    if (World->OwningGameInstance->LocalPlayers.Num() <= 0) {
        StaticLogError("[SDK] GetPlayerController: No LocalPlayers");
        return nullptr;
    }
    StaticLogInfo("[SDK] GetPlayerController: LocalPlayers found");

    if (!World->OwningGameInstance->LocalPlayers[0]) {
        StaticLogError("[SDK] GetPlayerController: LocalPlayers[0] is nullptr");
        return nullptr;
    }
    StaticLogInfo("[SDK] GetPlayerController: LocalPlayers[0] found");

    SDK::APlayerController* PC = World->OwningGameInstance->LocalPlayers[0]->PlayerController;
    if (PC) {
        StaticLogInfo("[SDK] GetPlayerController: PlayerController found successfully");
        return PC;
    }
    StaticLogError("[SDK] GetPlayerController: PlayerController is nullptr");
    return nullptr;
}

static SDK::APlayerController* FindPlayerControllerViaGObjects() {
    for (int i = 0; i < SDK::UObject::GObjects->Num(); i++) {
        SDK::UObject* Obj = SDK::UObject::GObjects->GetByIndex(i);
        if (Obj && Obj->IsA(SDK::APlayerController::StaticClass())) {
            SDK::APlayerController* PC = static_cast<SDK::APlayerController*>(Obj);
            if (PC->PlayerCameraManager) return PC;
        }
    }
    return nullptr;
}

static void StaticTogglePhotoMode() {
    try {
        StaticLogInfo("[SDK] Photo Mode toggle requested - executing...");

        SDK::APlayerController* PC = GetPlayerController();
        if (!PC) PC = FindPlayerControllerViaGObjects();
        if (!PC) {
            StaticLogError("[SDK] ERROR: PlayerController not found");
            return;
        }

        if (!PC->CheatManager) {
            SDK::UClass* CM_Class = SDK::UObject::FindClassFast("CheatManager");
            if (CM_Class) {
                PC->CheatManager = static_cast<SDK::UCheatManager*>(SDK::UGameplayStatics::SpawnObject(CM_Class, PC));
            }
            if (!PC->CheatManager) {
                StaticLogError("[SDK] FATAL: Could not spawn CheatManager");
                return;
            }
        }

        PC->Pause();
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        PC->CheatManager->ToggleDebugCamera();
        StaticLogInfo("[SDK] Photo Mode toggled successfully");
    } catch (...) {
        StaticLogError("[SDK] Exception in TogglePhotoMode");
    }
}

static void StaticToggleHUD() {
    try {
        StaticLogInfo("[SDK] HUD toggle requested - executing...");

        SDK::APlayerController* PC = GetPlayerController();
        if (!PC) {
            StaticLogInfo("[SDK] Primary GetPlayerController failed, trying GObjects...");
            PC = FindPlayerControllerViaGObjects();
        }
        if (!PC) {
            StaticLogError("[SDK] ERROR: PlayerController not found via both methods");
            return;
        }

        StaticLogInfo("[SDK] Got PlayerController, attempting GetHUD()...");
        SDK::AHUD* HUD = PC->GetHUD();
        if (HUD) {
            StaticLogInfo("[SDK] HUD found, calling ShowHUD()...");
            HUD->ShowHUD();
            StaticLogInfo("[SDK] HUD toggled successfully");
        } else {
            StaticLogError("[SDK] ERROR: HUD not found (GetHUD returned nullptr)");
        }
    } catch (...) {
        StaticLogError("[SDK] Exception in ToggleHUD");
    }
}

static void StaticToggleDamageNumbers(bool enable) {
    try {
        char buffer[256];
        snprintf(buffer, sizeof(buffer), "[SDK] Damage numbers %s", enable ? "ENABLED" : "DISABLED");
        StaticLogInfo(buffer);
    } catch (...) {
        StaticLogError("[SDK] Exception in ToggleDamageNumbers");
    }
}

static void StaticTeleportToLocation(float x, float y, float z) {
    try {
        char buffer[256];
        snprintf(buffer, sizeof(buffer), "[SDK] Attempting teleport to (%.1f, %.1f, %.1f)", x, y, z);
        StaticLogInfo(buffer);

        SDK::APlayerController* PC = GetPlayerController();
        if (!PC) PC = FindPlayerControllerViaGObjects();
        if (!PC) {
            StaticLogError("[SDK] ERROR: PlayerController not found");
            return;
        }

        SDK::APawn* Pawn = PC->AcknowledgedPawn;
        if (Pawn) {
            SDK::FVector Location;
            Location.X = x;
            Location.Y = y;
            Location.Z = z;
            Pawn->K2_SetActorLocation(Location, false, nullptr, false);
            StaticLogInfo("[SDK] Teleport executed successfully");
        } else {
            StaticLogError("[SDK] ERROR: Pawn not found");
        }
    } catch (...) {
        StaticLogError("[SDK] Exception in TeleportToLocation");
    }
}

namespace menu {
    extern std::atomic<bool> isOpen;
}

static bool StaticIsMenuOpen() {
    return menu::isOpen.load(std::memory_order_relaxed);
}

void PluginManager::SetupMasterAPI() {
    m_MasterAPI.apiVersion = PluginAPI::API_VERSION;
    m_MasterAPI.Log = StaticLog;
    m_MasterAPI.LogInfo = StaticLogInfo;
    m_MasterAPI.LogWarning = StaticLogWarning;
    m_MasterAPI.LogError = StaticLogError;
    m_MasterAPI.GetImGuiContext = StaticGetImGuiContext;
    m_MasterAPI.ExecuteOnGameThread = StaticExecuteOnGameThread;
    m_MasterAPI.ReloadPlugins = StaticReloadPlugins;
    m_MasterAPI.IsMenuOpen = StaticIsMenuOpen;

    g_PluginManagerInstance = this;
}

void PluginManager::Initialize(ImGuiContext* imguiCtx) {
    m_ImGuiContext = imguiCtx;
    SetupMasterAPI();

    printf("[PluginManager] Initializing...\n");
    fflush(stdout);

    extern HMODULE g_mainModule;
    char dllPath[MAX_PATH];
    GetModuleFileNameA(g_mainModule, dllPath, MAX_PATH);
    std::string dllDir(dllPath);
    size_t lastSlash = dllDir.find_last_of("\\/");
    if (lastSlash != std::string::npos) {
        dllDir = dllDir.substr(0, lastSlash + 1);
    }

    std::string pluginsDir = dllDir + "Plugins\\";
    if (!fs::exists(pluginsDir)) {
        try {
            fs::create_directory(pluginsDir);
            printf("[PluginManager] Created Plugins directory: %s\n", pluginsDir.c_str());
        } catch (const std::exception& e) {
            printf("[PluginManager] Failed to create Plugins directory: %s\n", e.what());
        }
    }

    LoadPluginsFromDirectory(pluginsDir.c_str());
    printf("[PluginManager] Loaded %zu plugin(s)\n", m_Plugins.size());
    fflush(stdout);
}

void PluginManager::Shutdown() {
    printf("[PluginManager] Shutting down...\n");
    fflush(stdout);

    // Call OnUnload for cleanup, but don't actually unload DLLs to prevent crashes
    for (auto& plugin : m_Plugins) {
        if (plugin.instance) {
            printf("[PluginManager] Calling OnUnload for: %s\n", plugin.instance->GetName());
            fflush(stdout);
            plugin.instance->OnUnload();
        }
    }

    printf("[PluginManager] Cleaning up hook systems...\n");
    fflush(stdout);

    try {
    } catch (...) {
        printf("[PluginManager] Exception during EventDispatcher cleanup\n");
        fflush(stdout);
    }

    printf("[PluginManager] Shutdown complete.\n");
    fflush(stdout);
}

void PluginManager::LoadPluginsFromDirectory(const char* directory) {
    printf("[PluginManager] Scanning directory: %s\n", directory);
    fflush(stdout);

    if (!fs::exists(directory)) {
        printf("[PluginManager] Directory does not exist: %s\n", directory);
        fflush(stdout);
        return;
    }

    try {
        for (const auto& entry : fs::directory_iterator(directory)) {
            if (entry.is_regular_file() && entry.path().extension() == ".dll") {
                std::string dllPath = entry.path().string();
                printf("[PluginManager] Found plugin: %s\n", dllPath.c_str());
                fflush(stdout);
                LoadPlugin(dllPath.c_str());
            }
        }
    } catch (const std::exception& e) {
        printf("[PluginManager] Error scanning directory: %s\n", e.what());
        fflush(stdout);
    }
}

bool PluginManager::LoadPlugin(const char* dllPath) {
    printf("[PluginManager] Loading plugin: %s\n", dllPath);
    fflush(stdout);

    HMODULE hModule = LoadLibraryA(dllPath);
    if (!hModule) {
        printf("[PluginManager] Failed to load DLL: %s (Error: %lu)\n", dllPath, GetLastError());
        fflush(stdout);
        return false;
    }

    typedef PluginAPI::IPlugin* (*CreatePluginFn)(const PluginAPI::MasterAPI*);
    CreatePluginFn createPlugin = (CreatePluginFn)GetProcAddress(hModule, "CreatePlugin");

    if (!createPlugin) {
        printf("[PluginManager] DLL missing CreatePlugin export: %s\n", dllPath);
        fflush(stdout);
        FreeLibrary(hModule);
        return false;
    }

    PluginAPI::IPlugin* plugin = createPlugin(&m_MasterAPI);
    if (!plugin) {
        printf("[PluginManager] CreatePlugin returned nullptr: %s\n", dllPath);
        fflush(stdout);
        FreeLibrary(hModule);
        return false;
    }

    if (!plugin->OnLoad()) {
        printf("[PluginManager] Plugin OnLoad failed: %s\n", dllPath);
        fflush(stdout);
        delete plugin;
        FreeLibrary(hModule);
        return false;
    }

    LoadedPlugin loadedPlugin;
    loadedPlugin.moduleHandle = hModule;
    loadedPlugin.instance = plugin;
    loadedPlugin.dllPath = dllPath;
    loadedPlugin.isEnabled = true;

    m_Plugins.push_back(loadedPlugin);

    printf("[PluginManager] Successfully loaded: %s v%s by %s\n",
        plugin->GetName(), plugin->GetVersion(), plugin->GetAuthor());
    fflush(stdout);

    return true;
}

void PluginManager::ReloadPlugins() {
    printf("[PluginManager] Plugin reloading has been disabled to prevent crashes.\n");
    fflush(stdout);
    // Reloading plugins at runtime can cause crashes, so it's disabled
}

void PluginManager::UpdatePlugins() {
    for (auto& plugin : m_Plugins) {
        if (plugin.isEnabled) {
            plugin.instance->OnFrame();
        }
    }
}

void PluginManager::RenderIndependentOverlays() {
    for (auto& plugin : m_Plugins) {
        if (plugin.isEnabled) {
            plugin.instance->RenderIndependent();
        }
    }
}

void PluginManager::RenderPluginTabs() {
    if (m_Plugins.empty()) {
        ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "No plugins loaded");
        ImGui::Separator();
        ImGui::Text("Place plugin DLLs in the 'Plugins' folder");
        ImGui::Text("next to Plugin_Manager.dll and restart the game.");
        return;
    }

    if (ImGui::BeginTabBar("PluginTabs")) {
        for (auto& plugin : m_Plugins) {
            if (!plugin.isEnabled || !plugin.instance->IsTabEnabled()) {
                continue;
            }

            const char* tabName = plugin.instance->GetTabName();
            if (!tabName || strlen(tabName) == 0) {
                continue; // Skip plugins with invalid tab names
            }

            if (ImGui::BeginTabItem(tabName)) {
                // Mod Details section (collapsible)
                if (ImGui::CollapsingHeader("About This Mod", ImGuiTreeNodeFlags_DefaultOpen)) {
                    ImGui::Indent(10.0f);
                    ImGui::TextColored(ImVec4(0.3f, 0.9f, 1.0f, 1.0f), "%s", plugin.instance->GetName());
                    ImGui::Text("Version: %s", plugin.instance->GetVersion());
                    ImGui::Text("Author: %s", plugin.instance->GetAuthor());
                    if (plugin.instance->GetDescription() && strlen(plugin.instance->GetDescription()) > 0) {
                        ImGui::Spacing();
                        ImGui::TextWrapped("%s", plugin.instance->GetDescription());
                    }
                    ImGui::Unindent(10.0f);
                    ImGui::Spacing();
                }

                // Check if plugin has sub-tabs
                if (plugin.instance->HasSubTabs()) {
                    // Render sub-tabs
                    if (ImGui::BeginTabBar((std::string(tabName) + "_SubTabs").c_str())) {
                        for (int i = 0; i < plugin.instance->GetSubTabCount(); i++) {
                            const char* subTabName = plugin.instance->GetSubTabName(i);
                            if (ImGui::BeginTabItem(subTabName)) {
                                plugin.instance->RenderSubTab(i);
                                ImGui::EndTabItem();
                            }
                        }
                        ImGui::EndTabBar();
                    }
                } else {
                    // Render regular plugin UI
                    plugin.instance->RenderUI();
                }
                ImGui::EndTabItem();
            }
        }

        // Info tab
        if (ImGui::BeginTabItem("Plugin Info")) {
            ImGui::TextColored(ImVec4(0.5f, 0.9f, 1.0f, 1.0f), "Loaded Plugins:");
            ImGui::Separator();
            ImGui::Spacing();

            for (size_t i = 0; i < m_Plugins.size(); i++) {
                auto& p = m_Plugins[i];
                ImGui::PushID(static_cast<int>(i));

                ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "%s", p.instance->GetName());
                ImGui::Text("  Version: %s", p.instance->GetVersion());
                ImGui::Text("  Author: %s", p.instance->GetAuthor());
                ImGui::Text("  Description: %s", p.instance->GetDescription());
                ImGui::Checkbox("Enabled##checkbox", &p.isEnabled);
                ImGui::Separator();

                ImGui::PopID();
            }

            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }
}

PluginAPI::IPlugin* PluginManager::GetPlugin(size_t index) {
    if (index >= m_Plugins.size()) return nullptr;
    return m_Plugins[index].instance;
}
