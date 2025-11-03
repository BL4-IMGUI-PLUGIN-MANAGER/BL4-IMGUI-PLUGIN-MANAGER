#include "PluginAPI.h"
#include "PluginHelpers.h"
#include "HookRegistry.h"
#include <iostream>
#include <sstream>
#include "../../plugin_manager_base/Plugin_Manager_Base_SDK/HookSystem.h"

namespace PluginAPI {

void* g_GlobalHookSystem = nullptr;

}

extern "C" void InitializeGlobalHookSystem(void* hookSystem) {
    PluginAPI::g_GlobalHookSystem = hookSystem;
    std::stringstream ss;
    ss << "[PluginLib] Global HookSystem initialized at 0x" << std::hex << (uintptr_t)hookSystem << std::dec;
    OutputDebugStringA(ss.str().c_str());
}

namespace PluginAPI {

bool HookRegistry::RegisterHook(
    const std::string& ClassName,
    const std::string& FunctionName,
    HookCallback PreCallback,
    HookCallback PostCallback)
{
    std::lock_guard<std::mutex> lock(m_Mutex);

    if (!g_GlobalHookSystem) {
        OutputDebugStringA("[HookRegistry] ERROR: HookSystem not initialized yet!\n");
        return false;
    }

    std::stringstream ss;
    ss << "[HookRegistry] Forwarding RegisterHook to global HookSystem: "
       << ClassName << "::" << FunctionName << "\n";
    OutputDebugStringA(ss.str().c_str());

    HookSystem* hookSys = reinterpret_cast<HookSystem*>(g_GlobalHookSystem);
    return hookSys->RegisterHook(ClassName, FunctionName, PreCallback, PostCallback);
}

bool HookRegistry::UnregisterHook(
    const std::string& ClassName,
    const std::string& FunctionName)
{
    std::lock_guard<std::mutex> lock(m_Mutex);

    if (!g_GlobalHookSystem) {
        return false;
    }

    HookSystem* hookSys = reinterpret_cast<HookSystem*>(g_GlobalHookSystem);
    return hookSys->UnregisterHook(ClassName, FunctionName);
}

}  // namespace PluginAPI
