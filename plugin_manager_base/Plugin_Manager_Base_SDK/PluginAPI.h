#pragma once

#include "HookSystem.h"

namespace PluginAPI {

inline HookSystem& GetHookSystem() {
    return HookSystem::Get();
}

}  // namespace PluginAPI
#ifdef BUILDING_PLUGIN_LOADER_BASE
extern "C" __declspec(dllexport) bool __cdecl RegisterGlobalHook(
    const char* ClassName,
    const char* FunctionName,
    void* PreCallbackPtr,
    void* PostCallbackPtr
);
#else
extern "C" __declspec(dllimport) bool __cdecl RegisterGlobalHook(
    const char* ClassName,
    const char* FunctionName,
    void* PreCallbackPtr,
    void* PostCallbackPtr
);
#endif
