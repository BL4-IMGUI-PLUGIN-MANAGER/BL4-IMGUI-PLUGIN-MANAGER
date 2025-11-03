#pragma once

#include <functional>
#include <string>
#include <vector>
#include <map>
#include <mutex>

namespace PluginAPI {

// Hook callback types
typedef std::function<void(void*, void*, void*)> HookCallback;

// Centralized hook registry that all plugins use
// This ensures all plugins share the same singleton HookSystem instance
class HookRegistry {
public:
    static HookRegistry& Get() {
        static HookRegistry instance;
        return instance;
    }

    // Register a hook on a class function
    // All callbacks go to the global HookSystem singleton in plugin_manager_base
    bool RegisterHook(
        const std::string& ClassName,
        const std::string& FunctionName,
        HookCallback PreCallback,
        HookCallback PostCallback = nullptr
    );

    // Unregister a hook
    bool UnregisterHook(
        const std::string& ClassName,
        const std::string& FunctionName
    );

private:
    HookRegistry() = default;
    ~HookRegistry() = default;

    HookRegistry(const HookRegistry&) = delete;
    HookRegistry& operator=(const HookRegistry&) = delete;

    mutable std::mutex m_Mutex;
};

}  // namespace PluginAPI
