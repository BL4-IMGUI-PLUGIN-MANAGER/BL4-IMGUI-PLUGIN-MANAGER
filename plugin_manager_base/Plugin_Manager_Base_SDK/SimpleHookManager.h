#pragma once
#include <functional>
#include <string>

namespace PluginAPI {

typedef void (*HookCallback)();

class SimpleHookManager {
public:
    static SimpleHookManager& Get() {
        static SimpleHookManager instance;
        return instance;
    }

    bool RegisterHook(const char* ClassName, const char* FunctionName, HookCallback Callback);
    bool UnregisterHook(const char* ClassName, const char* FunctionName);

private:
    SimpleHookManager() = default;
    ~SimpleHookManager() = default;
    SimpleHookManager(const SimpleHookManager&) = delete;
    SimpleHookManager& operator=(const SimpleHookManager&) = delete;
};

}  // namespace PluginAPI
