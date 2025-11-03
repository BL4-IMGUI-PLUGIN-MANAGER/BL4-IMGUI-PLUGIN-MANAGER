#pragma once
#include <functional>
#include <string>
#include <vector>
#include <map>
#include <mutex>

namespace PluginAPI {

typedef std::function<void(void*, void*, void*)> PreHookCallback;
typedef std::function<void(void*, void*, void*)> PostHookCallback;

class HookSystem {
public:
    static HookSystem& Get() {
        static HookSystem instance;
        return instance;
    }

    static uintptr_t GetInstanceAddress() {
        return (uintptr_t)&Get();
    }

    bool RegisterHook(
        const std::string& ClassName,
        const std::string& FunctionName,
        PreHookCallback PreCallback = nullptr,
        PostHookCallback PostCallback = nullptr,
        bool bSilent = false
    );

    bool RegisterHook(
        void* Object,
        const std::string& FunctionName,
        PreHookCallback PreCallback = nullptr,
        PostHookCallback PostCallback = nullptr
    );

    bool UnregisterHook(const std::string& ClassName, const std::string& FunctionName);
    bool UnregisterHook(void* Object, const std::string& FunctionName);
    bool InitializeProcessEventHook();
    std::vector<PreHookCallback> GetPreCallbacksByClass(const std::string& ClassName, const std::string& FunctionName) const;
    std::vector<PostHookCallback> GetPostCallbacksByClass(const std::string& ClassName, const std::string& FunctionName) const;
    void RegisterGlobalPreCallback(PreHookCallback Callback);
    void RegisterGlobalPostCallback(PostHookCallback Callback);
    std::vector<PreHookCallback> GetGlobalPreCallbacks() const;
    std::vector<PostHookCallback> GetGlobalPostCallbacks() const;
    static void LogInfo(const std::string& msg);
    static void LogWarning(const std::string& msg);
    static void LogError(const std::string& msg);

private:
    HookSystem() = default;
    ~HookSystem() = default;

    HookSystem(const HookSystem&) = delete;
    HookSystem& operator=(const HookSystem&) = delete;

    std::map<std::string, std::map<std::string, std::vector<PreHookCallback>>> m_PreCallbacksByClass;
    std::map<std::string, std::map<std::string, std::vector<PostHookCallback>>> m_PostCallbacksByClass;
    mutable std::mutex m_HooksMutex;
    std::vector<PreHookCallback> m_GlobalPreCallbacks;
    std::vector<PostHookCallback> m_GlobalPostCallbacks;
    mutable std::mutex m_GlobalCallbacksMutex;
    bool m_ProcessEventHookInitialized = false;
    std::mutex m_InitMutex;
};

}  // namespace PluginAPI
