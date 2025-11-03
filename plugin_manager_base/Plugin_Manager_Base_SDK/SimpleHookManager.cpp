#include "SimpleHookManager.h"
#include "SDK/SDK/Basic.hpp"
#include "SDK/SDK/CoreUObject_classes.hpp"
#include "MinHook.h"
#include <map>
#include <vector>
#include <mutex>
#include <iostream>
#include <cstdio>
#include <cstdarg>

namespace PluginAPI {

// ===== Global State (All protected by mutexes) =====

// Hook entry structure
struct HookEntry {
    std::string ClassName;
    std::string FunctionName;
    HookCallback Callback;
    void* OriginalFunction;
    void* TargetAddress;
};

// Global storage for callbacks (maps target address to callback function)
static std::map<void*, HookCallback> g_HookCallbacks;
static std::mutex g_HookCallbacksMutex;

// Global storage for hook entries
static std::vector<HookEntry> g_Hooks;
static std::map<std::string, size_t> g_HookMap;  // Maps "ClassName::FunctionName" to index
static std::mutex g_HooksMutex;

// MinHook initialization state
static bool g_MinHookInitialized = false;
static std::mutex g_MinHookMutex;

// Logging helper - prints with thread-safe buffering
static void LogInfo(const char* fmt, ...) {
    char buffer[2048];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);
    printf("[SimpleHookManager] %s\n", buffer);
    fflush(stdout);
}

static void LogWarning(const char* fmt, ...) {
    char buffer[2048];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);
    printf("[WARNING] [SimpleHookManager] %s\n", buffer);
    fflush(stdout);
}

static void LogError(const char* fmt, ...) {
    char buffer[2048];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);
    printf("[ERROR] [SimpleHookManager] %s\n", buffer);
    fflush(stdout);
}

// ===== MinHook Wrapper =====

// This function is called when a hooked function is invoked
// It looks up the registered callback and calls it
void MinHookWrapper(void* Obj, void* Stack, void* Result) {
    std::lock_guard<std::mutex> lock(g_HookCallbacksMutex);
    auto it = g_HookCallbacks.find(Obj);
    if (it != g_HookCallbacks.end() && it->second) {
        it->second();
    }
}

// ===== Public API =====

bool SimpleHookManager::RegisterHook(const char* ClassName, const char* FunctionName, HookCallback Callback) {
    // Validate inputs
    if (!ClassName || !FunctionName || !Callback) {
        LogWarning("RegisterHook - FAILED: Invalid parameters (null pointer)");
        return false;
    }

    std::lock_guard<std::mutex> hooksLock(g_HooksMutex);

    // Check if already registered
    std::string key = std::string(ClassName) + "::" + FunctionName;
    if (g_HookMap.find(key) != g_HookMap.end()) {
        LogWarning("RegisterHook %s::%s - FAILED: Hook already registered", ClassName, FunctionName);
        return false;
    }

    // ===== STEP 1: Find the class =====
    LogInfo("RegisterHook %s::%s - Starting hook registration...", ClassName, FunctionName);
    LogInfo("RegisterHook %s::%s - [STEP 1/7] Finding class '%s'...", ClassName, FunctionName, ClassName);

    SDK::UClass* TargetClass = SDK::BasicFilesImpleUtils::FindClassByName(ClassName, false);
    if (!TargetClass) {
        LogError("RegisterHook %s::%s - [STEP 1/7] FAILED: Class '%s' not found",
                 ClassName, FunctionName, ClassName);
        return false;
    }

    LogInfo("RegisterHook %s::%s - [STEP 1/7] SUCCESS: Class found at %p",
            ClassName, FunctionName, (void*)TargetClass);

    // ===== STEP 2: Find the function =====
    LogInfo("RegisterHook %s::%s - [STEP 2/7] Finding function '%s' in class...",
            ClassName, FunctionName, FunctionName);

    SDK::UFunction* TargetFunction = TargetClass->GetFunction(ClassName, FunctionName);
    if (!TargetFunction) {
        LogError("RegisterHook %s::%s - [STEP 2/7] FAILED: Function '%s' not found in class",
                 ClassName, FunctionName, FunctionName);
        return false;
    }

    LogInfo("RegisterHook %s::%s - [STEP 2/7] SUCCESS: Function found at %p",
            ClassName, FunctionName, (void*)TargetFunction);

    // ===== STEP 3: Extract the ExecFunction pointer (native code address) =====
    LogInfo("RegisterHook %s::%s - [STEP 3/7] Extracting ExecFunction pointer from UFunction...",
            ClassName, FunctionName);

    SDK::UFunction::FNativeFuncPtr NativeFuncPtr = TargetFunction->ExecFunction;
    void* pTarget = reinterpret_cast<void*>(NativeFuncPtr);

    if (!pTarget) {
        LogError("RegisterHook %s::%s - [STEP 3/7] FAILED: ExecFunction pointer is NULL",
                 ClassName, FunctionName);
        return false;
    }

    LogInfo("RegisterHook %s::%s - [STEP 3/7] SUCCESS: Extracted ExecFunction pointer at %p",
            ClassName, FunctionName, pTarget);

    // ===== STEP 4: Initialize MinHook (thread-safe) =====
    {
        std::lock_guard<std::mutex> minHookLock(g_MinHookMutex);

        if (!g_MinHookInitialized) {
            LogInfo("RegisterHook %s::%s - [STEP 4/7] Initializing MinHook library...",
                    ClassName, FunctionName);

            MH_STATUS initStatus = MH_Initialize();
            // Both MH_OK (0) and MH_ERROR_ALREADY_INITIALIZED (1) are acceptable
            if (initStatus != MH_OK && initStatus != MH_ERROR_ALREADY_INITIALIZED) {
                LogError("RegisterHook %s::%s - [STEP 4/7] FAILED: MH_Initialize returned %d (only 0 and 1 are OK)",
                         ClassName, FunctionName, initStatus);
                return false;
            }

            if (initStatus == MH_OK) {
                LogInfo("RegisterHook %s::%s - [STEP 4/7] SUCCESS: MinHook initialized by this hook",
                        ClassName, FunctionName);
            } else {
                LogInfo("RegisterHook %s::%s - [STEP 4/7] INFO: MinHook was already initialized by another system (OK)",
                        ClassName, FunctionName);
            }
            g_MinHookInitialized = true;
        } else {
            LogInfo("RegisterHook %s::%s - [STEP 4/7] INFO: MinHook already initialized, skipping...",
                    ClassName, FunctionName);
        }
    }

    // ===== STEP 5: Create the MinHook hook =====
    LogInfo("RegisterHook %s::%s - [STEP 5/7] Creating MinHook at address %p...",
            ClassName, FunctionName, pTarget);

    void* pOriginal = nullptr;
    MH_STATUS createStatus = MH_CreateHook(pTarget, (LPVOID)&MinHookWrapper, &pOriginal);

    LogInfo("RegisterHook %s::%s - [STEP 5/7] MH_CreateHook returned: %d",
            ClassName, FunctionName, createStatus);
    LogInfo("        MinHook error codes: 0=OK, 1=ALREADY_HOOKED, 2=NOT_EXECUTABLE, 3=NOT_ENOUGH_MEMORY,");
    LogInfo("        4=UNSUPPORTED_FUNCTION, 5=POSITION_INDEPENDENT_CODE, 9=MEMORY_ALLOC");

    if (createStatus != MH_OK) {
        LogError("RegisterHook %s::%s - [STEP 5/7] FAILED: MH_CreateHook error %d at address %p",
                 ClassName, FunctionName, createStatus, pTarget);
        LogError("        This means the address is not hookable (JIT code, special memory, or PIE)");
        return false;
    }

    LogInfo("RegisterHook %s::%s - [STEP 5/7] SUCCESS: MinHook created at %p",
            ClassName, FunctionName, pTarget);

    // ===== STEP 6: Enable the MinHook hook =====
    LogInfo("RegisterHook %s::%s - [STEP 6/7] Enabling MinHook...", ClassName, FunctionName);

    MH_STATUS enableStatus = MH_EnableHook(pTarget);

    LogInfo("RegisterHook %s::%s - [STEP 6/7] MH_EnableHook returned: %d",
            ClassName, FunctionName, enableStatus);

    if (enableStatus != MH_OK) {
        LogWarning("RegisterHook %s::%s - [STEP 6/7] Enable failed (code %d), attempting recovery...",
                   ClassName, FunctionName, enableStatus);

        MH_DisableHook(pTarget);
        enableStatus = MH_EnableHook(pTarget);

        if (enableStatus != MH_OK) {
            LogError("RegisterHook %s::%s - [STEP 6/7] FAILED: Recovery attempt failed with code %d",
                     ClassName, FunctionName, enableStatus);
            return false;
        }

        LogInfo("RegisterHook %s::%s - [STEP 6/7] SUCCESS: Hook enabled after recovery",
                ClassName, FunctionName);
    } else {
        LogInfo("RegisterHook %s::%s - [STEP 6/7] SUCCESS: Hook enabled successfully",
                ClassName, FunctionName);
    }

    // ===== STEP 7: Store hook entry and callback (with callback mutex) =====
    LogInfo("RegisterHook %s::%s - [STEP 7/7] Storing hook entry and registering callback...",
            ClassName, FunctionName);

    HookEntry entry;
    entry.ClassName = ClassName;
    entry.FunctionName = FunctionName;
    entry.Callback = Callback;
    entry.OriginalFunction = pOriginal;
    entry.TargetAddress = pTarget;

    size_t index = g_Hooks.size();
    g_Hooks.push_back(entry);
    g_HookMap[key] = index;

    // Store callback for the MinHook wrapper
    {
        std::lock_guard<std::mutex> cbLock(g_HookCallbacksMutex);
        g_HookCallbacks[pTarget] = Callback;
    }

    // Log final success with all details
    LogInfo("RegisterHook %s::%s - ===== REGISTRATION SUCCESS! =====", ClassName, FunctionName);
    LogInfo("    UFunction object address: %p", (void*)TargetFunction);
    LogInfo("    ExecFunction (native code): %p", (void*)NativeFuncPtr);
    LogInfo("    Hook target address: %p", pTarget);
    LogInfo("    MinHook wrapper function: %p", (void*)&MinHookWrapper);
    LogInfo("    Original function saved: %p", pOriginal);
    LogInfo("    Callback registered: %p", (void*)Callback);

    return true;
}

bool SimpleHookManager::UnregisterHook(const char* ClassName, const char* FunctionName) {
    if (!ClassName || !FunctionName) {
        LogWarning("UnregisterHook - FAILED: Invalid parameters (null pointer)");
        return false;
    }

    std::lock_guard<std::mutex> hooksLock(g_HooksMutex);

    std::string key = std::string(ClassName) + "::" + FunctionName;
    auto it = g_HookMap.find(key);
    if (it == g_HookMap.end()) {
        LogWarning("UnregisterHook %s::%s - FAILED: Hook not found in registry", ClassName, FunctionName);
        return false;
    }

    size_t index = it->second;
    if (index >= g_Hooks.size()) {
        LogWarning("UnregisterHook %s::%s - FAILED: Invalid hook index in registry", ClassName, FunctionName);
        return false;
    }

    HookEntry& entry = g_Hooks[index];

    LogInfo("UnregisterHook %s::%s - Unregistering hook at address %p...",
            ClassName, FunctionName, entry.TargetAddress);

    // Disable the MinHook hook
    MH_DisableHook(entry.TargetAddress);
    LogInfo("UnregisterHook %s::%s - MinHook disabled", ClassName, FunctionName);

    // Remove callback (with callback mutex)
    {
        std::lock_guard<std::mutex> cbLock(g_HookCallbacksMutex);
        g_HookCallbacks.erase(entry.TargetAddress);
    }
    LogInfo("UnregisterHook %s::%s - Callback removed from registry", ClassName, FunctionName);

    // Remove from hooks vector
    g_Hooks.erase(g_Hooks.begin() + index);
    LogInfo("UnregisterHook %s::%s - Hook entry removed from vector", ClassName, FunctionName);

    // Update map indices (critical for maintaining correct indices after erase)
    g_HookMap.erase(it);
    for (auto& pair : g_HookMap) {
        if (pair.second > index) {
            pair.second--;
        }
    }

    LogInfo("UnregisterHook %s::%s - ===== UNREGISTRATION SUCCESS! =====", ClassName, FunctionName);

    return true;
}

}  // namespace PluginAPI
