#pragma once
#include "PluginAPI.h"
#include "SDK.h"
#include <imgui.h>
#include <sstream>
#include <atomic>
#include <thread>
#include <chrono>

namespace PluginHelpers {

    // Utility class for plugins to use ImGui safely
    class ImGuiManager {
    private:
        const PluginAPI::MasterAPI* m_api;

    public:
        ImGuiManager(const PluginAPI::MasterAPI* api) : m_api(api) {}

        // Get the shared ImGui context
        ImGuiContext* GetContext() const {
            return m_api->GetImGuiContext();
        }

        // Push ImGui context to make sure this plugin's UI uses the right context
        void PushContext() const {
            ImGui::SetCurrentContext(GetContext());
        }

        // Helper to log ImGui debug info
        void LogDebugInfo(const char* info) const {
            m_api->LogInfo(info);
        }
    };

    // Utility class for thread-safe game operations
    class GameThreadExecutor {
    private:
        const PluginAPI::MasterAPI* m_api;

    public:
        GameThreadExecutor(const PluginAPI::MasterAPI* api) : m_api(api) {}

        // Execute a function on the game thread
        template<typename Func>
        void Execute(Func func) const {
            m_api->ExecuteOnGameThread(func);
        }

        // Execute a lambda on game thread and wait for completion
        template<typename Func>
        void ExecuteSync(Func func) const {
            // Use atomic flag instead of stack reference to avoid use-after-free
            // The lambda captures a shared_ptr to the atomic, not a reference to stack memory
            auto completed = std::make_shared<std::atomic<bool>>(false);

            m_api->ExecuteOnGameThread([func, completed]() {
                func();
                completed->store(true, std::memory_order_release);
            });

            // Wait for completion with timeout to avoid infinite loops
            // Check every 1ms for up to 10 seconds
            for (int i = 0; i < 10000; ++i) {
                if (completed->load(std::memory_order_acquire)) {
                    break;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        }
    };

    // Utility class for logging with formatting
    class Logger {
    private:
        const PluginAPI::MasterAPI* m_api;
        std::string m_prefix;

    public:
        Logger(const PluginAPI::MasterAPI* api, const char* prefix = "")
            : m_api(api), m_prefix(prefix) {}

        // Log with format string
        void Info(const char* fmt, ...) const {
            char buffer[512];
            va_list args;
            va_start(args, fmt);
            vsnprintf_s(buffer, sizeof(buffer), _TRUNCATE, fmt, args);
            va_end(args);

            if (!m_prefix.empty()) {
                std::string msg = m_prefix + ": " + buffer;
                m_api->LogInfo(msg.c_str());
            } else {
                m_api->LogInfo(buffer);
            }
        }

        void Warning(const char* fmt, ...) const {
            char buffer[512];
            va_list args;
            va_start(args, fmt);
            vsnprintf_s(buffer, sizeof(buffer), _TRUNCATE, fmt, args);
            va_end(args);

            if (!m_prefix.empty()) {
                std::string msg = m_prefix + ": " + buffer;
                m_api->LogWarning(msg.c_str());
            } else {
                m_api->LogWarning(buffer);
            }
        }

        void Error(const char* fmt, ...) const {
            char buffer[512];
            va_list args;
            va_start(args, fmt);
            vsnprintf_s(buffer, sizeof(buffer), _TRUNCATE, fmt, args);
            va_end(args);

            if (!m_prefix.empty()) {
                std::string msg = m_prefix + ": " + buffer;
                m_api->LogError(msg.c_str());
            } else {
                m_api->LogError(buffer);
            }
        }
    };

    // Game helper utilities for SDK operations
    // NOTE: Plugins should implement SDK operations directly in ExecuteOnGameThread lambdas
    // where the SDK library is available. This namespace provides documentation and
    // utility type aliases for plugin developers.
    class GameHelpers {
    public:
        // Helper types for plugin use (when called from ExecuteOnGameThread)
        using PlayerController = PluginSDK::APlayerController;
        using Character = PluginSDK::AOakCharacter;
        using World = PluginSDK::UWorld;
        using HUD = PluginSDK::AHUD;
        using LocalPlayer = PluginSDK::ULocalPlayer;

        // Example code for plugins (implement in your ExecuteOnGameThread callbacks):
        //
        // Toggle Photo Mode:
        //  m_API->ExecuteOnGameThread([]() {
        //      auto World = SDK::UWorld::GetWorld();
        //      if (!World || !World->OwningGameInstance) return;
        //      auto PC = World->OwningGameInstance->LocalPlayers[0]->PlayerController;
        //      if (!PC) return;
        //      if (!PC->CheatManager) {
        //          auto CM_Class = SDK::UObject::FindClassFast("CheatManager");
        //          if (!CM_Class) return;
        //          PC->CheatManager = static_cast<SDK::UCheatManager*>(
        //              SDK::UGameplayStatics::SpawnObject(CM_Class, PC));
        //      }
        //      PC->Pause();
        //      Sleep(100);
        //      PC->CheatManager->ToggleDebugCamera();
        //  });
        //
        // Toggle HUD:
        //  m_API->ExecuteOnGameThread([]() {
        //      auto World = SDK::UWorld::GetWorld();
        //      if (!World || !World->OwningGameInstance) return;
        //      auto HUD = World->OwningGameInstance->LocalPlayers[0]->PlayerController->GetHUD();
        //      if (HUD) HUD->bShowHUD = !HUD->bShowHUD;
        //  });
    };

    // Base class for plugins - handles common functionality
    class BasePlugin : public PluginAPI::IPlugin {
    protected:
        const PluginAPI::MasterAPI* m_api;
        ImGuiManager m_imgui_manager;
        GameThreadExecutor m_game_thread;
        Logger m_logger;

    public:
        BasePlugin(const PluginAPI::MasterAPI* api, const char* name)
            : m_api(api), m_imgui_manager(api), m_game_thread(api), m_logger(api, name) {}

        virtual ~BasePlugin() = default;

        // Access to helper managers
        const ImGuiManager& GetImGuiManager() const { return m_imgui_manager; }
        const GameThreadExecutor& GetGameThread() const { return m_game_thread; }
        const Logger& GetLogger() const { return m_logger; }
        const PluginAPI::MasterAPI* GetMasterAPI() const { return m_api; }

        // Default implementations
        void OnFrame() override {}
        void RenderIndependent() override {}
        bool HasSubTabs() override { return false; }
        int GetSubTabCount() override { return 0; }
        const char* GetSubTabName(int index) override { return ""; }
        void RenderSubTab(int index) override {}
    };
}

// Convenient typedef
using PluginLogger = PluginHelpers::Logger;
using PluginBase = PluginHelpers::BasePlugin;
