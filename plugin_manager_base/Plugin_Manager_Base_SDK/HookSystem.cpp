#include "HookSystem.h"
#include "SDK/SDK/Basic.hpp"
#include "SDK/SDK/CoreUObject_classes.hpp"
#include "SDK/SDK/Engine_classes.hpp"
#include "MinHook.h"
#include <iostream>
#include <sstream>

namespace PluginAPI {

// ===== Global State =====

// Original ProcessEvent function pointer
typedef void (*ProcessEventFnType)(SDK::UObject*, SDK::UFunction*, void*);
static ProcessEventFnType g_OriginalProcessEvent = nullptr;

// ===== Logging =====
// Note: std::cout is disabled - logging only goes to OutputDebugString for debugging

void HookSystem::LogInfo(const std::string& msg) {
    std::string fullMsg = "[HookSystem] " + msg + "\n";
    OutputDebugStringA(fullMsg.c_str());
}

void HookSystem::LogWarning(const std::string& msg) {
    std::string fullMsg = "[HookSystem WARNING] " + msg + "\n";
    OutputDebugStringA(fullMsg.c_str());
}

void HookSystem::LogError(const std::string& msg) {
    std::string fullMsg = "[HookSystem ERROR] " + msg + "\n";
    OutputDebugStringA(fullMsg.c_str());
}

// ===== ProcessEvent Hook Wrapper =====

// OPTIMIZATION: Cache FName/UClass pointers for frequently hooked functions to avoid string allocations
static SDK::UFunction* g_CachedMenuOpen = nullptr;
static SDK::UFunction* g_CachedMenuClose = nullptr;

// Hooked ProcessEvent that checks hash table and fires callbacks
void HookedProcessEvent(SDK::UObject* Object, SDK::UFunction* Function, void* Params)
{
    if (Object && Function)
    {
        // OPTIMIZATION: Get global pre-callbacks reference (avoid vector copy)
        const auto& GlobalPreCallbacks = HookSystem::Get().GetGlobalPreCallbacks();
        for (const auto& Callback : GlobalPreCallbacks)
        {
            if (Callback)
            {
                try
                {
                    Callback(Object, Function, Params);
                }
                catch (const std::exception& e)
                {
                    HookSystem::LogError(std::string("Exception in global pre-callback: ") + e.what());
                }
            }
        }

        // OPTIMIZATION: Get name strings (still need conversion for map keys, but minimize operations)
        std::string FunctionNameStr = Function ? Function->GetName() : "Unknown";
        std::string ClassNameStr = (Object && Object->Class) ? Object->Class->GetName() : "Unknown";
        const char* FunctionNameCStr = FunctionNameStr.c_str();
        const char* ClassNameCStr = ClassNameStr.c_str();

        // Fast path for frequently hooked functions (MenuOpen/MenuClose)
        bool bIsMenuFunction = false;
        if (FunctionNameCStr) {
            // Quick string comparison without allocation
            if ((FunctionNameCStr[0] == 'M' && FunctionNameCStr[1] == 'e' && FunctionNameCStr[2] == 'n' && FunctionNameCStr[3] == 'u') &&
                ((FunctionNameCStr[4] == 'O' && FunctionNameCStr[5] == 'p' && FunctionNameCStr[6] == 'e' && FunctionNameCStr[7] == 'n' && FunctionNameCStr[8] == '\0') ||
                 (FunctionNameCStr[4] == 'C' && FunctionNameCStr[5] == 'l' && FunctionNameCStr[6] == 'o' && FunctionNameCStr[7] == 's' && FunctionNameCStr[8] == 'e' && FunctionNameCStr[9] == '\0'))) {
                bIsMenuFunction = true;
            }
        }

        // Use already-converted strings for lookup
        const std::string& FunctionName = FunctionNameStr;
        const std::string& ClassName = ClassNameStr;

        // OPTIMIZATION: Use pointer references to avoid vector copies
        const auto& PreCallbacks = HookSystem::Get().GetPreCallbacksByClass(ClassName, FunctionName);
        const auto& PostCallbacks = HookSystem::Get().GetPostCallbacksByClass(ClassName, FunctionName);

        // Debug: Log MenuOpen/MenuClose lookups (reduced verbosity)
        if (bIsMenuFunction) {
            std::stringstream ss;
            ss << "[HookedProcessEvent] " << ClassName << "::" << FunctionName
               << " - Found " << PreCallbacks.size() << " pre-callbacks, "
               << PostCallbacks.size() << " post-callbacks\n";
            OutputDebugStringA(ss.str().c_str());
        }

        // Fire pre-callbacks
        for (const auto& Callback : PreCallbacks)
        {
            if (Callback)
            {
                try
                {
                    if (bIsMenuFunction) {
                        std::string msg = "[HookedProcessEvent] Invoking pre-callback for " + ClassName + "::" + FunctionName + "\n";
                        OutputDebugStringA(msg.c_str());
                    }
                    Callback(Object, Function, Params);
                    if (bIsMenuFunction) {
                        OutputDebugStringA("[HookedProcessEvent] Pre-callback completed\n");
                    }
                }
                catch (const std::exception& e)
                {
                    HookSystem::LogError(std::string("Exception in pre-callback: ") + e.what());
                }
            }
        }

        // Call original ProcessEvent
        if (g_OriginalProcessEvent)
        {
            g_OriginalProcessEvent(Object, Function, Params);
        }

        // Fire post-callbacks
        for (const auto& Callback : PostCallbacks)
        {
            if (Callback)
            {
                try
                {
                    Callback(Object, Function, Params);
                }
                catch (const std::exception& e)
                {
                    HookSystem::LogError(std::string("Exception in post-callback: ") + e.what());
                }
            }
        }

        // OPTIMIZATION: Get global post-callbacks reference (avoid vector copy)
        const auto& GlobalPostCallbacks = HookSystem::Get().GetGlobalPostCallbacks();
        for (const auto& Callback : GlobalPostCallbacks)
        {
            if (Callback)
            {
                try
                {
                    Callback(Object, Function, Params);
                }
                catch (const std::exception& e)
                {
                    HookSystem::LogError(std::string("Exception in global post-callback: ") + e.what());
                }
            }
        }
    }
    else if (g_OriginalProcessEvent)
    {
        // Fall through if no hooks
        g_OriginalProcessEvent(Object, Function, Params);
    }
}

// ===== Public API =====

bool HookSystem::InitializeProcessEventHook()
{
    std::lock_guard<std::mutex> lock(m_InitMutex);

    if (m_ProcessEventHookInitialized)
    {
        LogInfo("ProcessEvent hook already initialized");
        return true;
    }

    LogInfo("Initializing ProcessEvent hook...");

    // Initialize MinHook
    MH_STATUS initStatus = MH_Initialize();
    if (initStatus != MH_OK && initStatus != MH_ERROR_ALREADY_INITIALIZED)
    {
        LogError("MH_Initialize failed with code: " + std::to_string(initStatus));
        return false;
    }

    // Hook ProcessEvent
    uintptr_t base = (uintptr_t)GetModuleHandleA(nullptr);
    uintptr_t ProcessEventAddr = base + SDK::Offsets::ProcessEvent;

    LogInfo("ProcessEvent address: 0x" + std::to_string(ProcessEventAddr));

    if (MH_CreateHook(
        (LPVOID)ProcessEventAddr,
        (LPVOID)HookedProcessEvent,
        (LPVOID*)&g_OriginalProcessEvent) != MH_OK)
    {
        LogError("MH_CreateHook failed for ProcessEvent");
        return false;
    }

    if (MH_EnableHook((LPVOID)ProcessEventAddr) != MH_OK)
    {
        LogError("MH_EnableHook failed for ProcessEvent");
        return false;
    }

    LogInfo("ProcessEvent hook initialized successfully!");
    m_ProcessEventHookInitialized = true;
    return true;
}

bool HookSystem::RegisterHook(
    const std::string& ClassName,
    const std::string& FunctionName,
    PreHookCallback PreCallback,
    PostHookCallback PostCallback,
    bool bSilent)
{
    std::lock_guard<std::mutex> lock(m_HooksMutex);

    // DEBUG: Log callback status and instance address
    if (!bSilent) {
        std::stringstream ss;
        ss << "[RegisterHook ENTRY] Instance=0x" << std::hex << (uintptr_t)this << std::dec
           << ", ClassName=" << ClassName << ", FunctionName=" << FunctionName
           << ", PreCallback is " << (PreCallback ? "VALID" : "NULL")
           << ", PostCallback is " << (PostCallback ? "VALID" : "NULL") << "\n";
        OutputDebugStringA(ss.str().c_str());
    }

    // Validate inputs
    if (ClassName.empty() || FunctionName.empty())
    {
        if (!bSilent)
            LogError("ClassName or FunctionName is empty");
        return false;
    }

    if (!bSilent)
        LogInfo("Registering hook: " + ClassName + "::" + FunctionName);

    // Find the class (just to validate it exists)
    SDK::UClass* TargetClass = SDK::BasicFilesImpleUtils::FindClassByName(ClassName, false);
    if (!TargetClass)
    {
        if (!bSilent)
            LogError("Class not found: " + ClassName);
        return false;
    }

    if (!bSilent)
        LogInfo("  Found class at: 0x" + std::to_string((uintptr_t)TargetClass));

    // Find the function (just to validate it exists)
    SDK::UFunction* TargetFunction = TargetClass->GetFunction(ClassName.c_str(), FunctionName.c_str());
    if (!TargetFunction)
    {
        if (!bSilent)
            LogError("Function not found: " + FunctionName);
        return false;
    }

    if (!bSilent) {
        LogInfo("  Found function at: 0x" + std::to_string((uintptr_t)TargetFunction));
        std::string ActualFunctionName = TargetFunction->GetName();
        LogInfo("  Function GetName() returns: " + ActualFunctionName);
    }

    // Add callbacks to the nested dictionary (pure dictionary system)
    int preCallbackCount = 0;
    int postCallbackCount = 0;

    if (PreCallback) {
        // Debug logging removed - too verbose. Use debugger or OutputDebugString if needed.
        m_PreCallbacksByClass[ClassName][FunctionName].push_back(PreCallback);
        preCallbackCount = m_PreCallbacksByClass[ClassName][FunctionName].size();
    }

    if (PostCallback) {
        m_PostCallbacksByClass[ClassName][FunctionName].push_back(PostCallback);
        postCallbackCount = m_PostCallbacksByClass[ClassName][FunctionName].size();
    }

    if (!bSilent) {
        LogInfo("  Added to nested dictionary");
        if (PreCallback)
            LogInfo("  Pre-callbacks count: " + std::to_string(preCallbackCount));
        if (PostCallback)
            LogInfo("  Post-callbacks count: " + std::to_string(postCallbackCount));
    }

    if (!bSilent)
        LogInfo("Hook registered successfully: " + ClassName + "::" + FunctionName);
    return true;
}

bool HookSystem::RegisterHook(
    void* Object,
    const std::string& FunctionName,
    PreHookCallback PreCallback,
    PostHookCallback PostCallback)
{
    if (!Object)
    {
        LogError("RegisterHook(Object, Function) - Object is null");
        return false;
    }

    // Get the object's class name
    SDK::UObject* UObj = (SDK::UObject*)Object;
    std::string ClassName = UObj->Class ? UObj->Class->GetName() : "Unknown";

    LogInfo("Registering hook on object " + ClassName + "::" + FunctionName);

    // Delegate to class-based registration
    return RegisterHook(ClassName, FunctionName, PreCallback, PostCallback);
}

bool HookSystem::UnregisterHook(const std::string& ClassName, const std::string& FunctionName)
{
    std::lock_guard<std::mutex> lock(m_HooksMutex);

    // DEBUG: Log all unregistrations
    std::string msg = "[UnregisterHook CALLED] ClassName=" + ClassName + ", FunctionName=" + FunctionName + "\n";
    OutputDebugStringA(msg.c_str());

    // Validate inputs
    if (ClassName.empty() || FunctionName.empty())
    {
        LogError("ClassName or FunctionName is empty");
        return false;
    }

    bool bFound = false;

    // Remove from pre-callbacks dictionary
    auto classIt = m_PreCallbacksByClass.find(ClassName);
    if (classIt != m_PreCallbacksByClass.end()) {
        auto funcIt = classIt->second.find(FunctionName);
        if (funcIt != classIt->second.end()) {
            classIt->second.erase(funcIt);
            bFound = true;
        }
        // Clean up empty class entry
        if (classIt->second.empty()) {
            m_PreCallbacksByClass.erase(classIt);
        }
    }

    // Remove from post-callbacks dictionary
    auto postClassIt = m_PostCallbacksByClass.find(ClassName);
    if (postClassIt != m_PostCallbacksByClass.end()) {
        auto funcIt = postClassIt->second.find(FunctionName);
        if (funcIt != postClassIt->second.end()) {
            postClassIt->second.erase(funcIt);
            bFound = true;
        }
        // Clean up empty class entry
        if (postClassIt->second.empty()) {
            m_PostCallbacksByClass.erase(postClassIt);
        }
    }

    if (!bFound) {
        LogWarning("Hook not found: " + ClassName + "::" + FunctionName);
        return false;
    }

    LogInfo("Hook unregistered: " + ClassName + "::" + FunctionName);
    return true;
}

bool HookSystem::UnregisterHook(void* Object, const std::string& FunctionName)
{
    if (!Object)
    {
        LogError("UnregisterHook(Object, Function) - Object is null");
        return false;
    }

    SDK::UObject* UObj = (SDK::UObject*)Object;
    std::string ClassName = UObj->Class ? UObj->Class->GetName() : "Unknown";

    return UnregisterHook(ClassName, FunctionName);
}

std::vector<PreHookCallback> HookSystem::GetPreCallbacksByClass(const std::string& ClassName, const std::string& FunctionName) const
{
    std::lock_guard<std::mutex> lock(m_HooksMutex);

    // Debug logging removed - too verbose for production use
    auto classIt = m_PreCallbacksByClass.find(ClassName);
    if (classIt != m_PreCallbacksByClass.end())
    {
        auto funcIt = classIt->second.find(FunctionName);
        if (funcIt != classIt->second.end())
        {
            return funcIt->second;
        }
    }

    return std::vector<PreHookCallback>();
}

std::vector<PostHookCallback> HookSystem::GetPostCallbacksByClass(const std::string& ClassName, const std::string& FunctionName) const
{
    std::lock_guard<std::mutex> lock(m_HooksMutex);

    auto classIt = m_PostCallbacksByClass.find(ClassName);
    if (classIt != m_PostCallbacksByClass.end())
    {
        auto funcIt = classIt->second.find(FunctionName);
        if (funcIt != classIt->second.end())
        {
            return funcIt->second;
        }
    }

    return std::vector<PostHookCallback>();
}

void HookSystem::RegisterGlobalPreCallback(PreHookCallback Callback)
{
    std::lock_guard<std::mutex> lock(m_GlobalCallbacksMutex);
    m_GlobalPreCallbacks.push_back(Callback);
    LogInfo("Registered global pre-callback (total: " + std::to_string(m_GlobalPreCallbacks.size()) + ")");
}

void HookSystem::RegisterGlobalPostCallback(PostHookCallback Callback)
{
    std::lock_guard<std::mutex> lock(m_GlobalCallbacksMutex);
    m_GlobalPostCallbacks.push_back(Callback);
    LogInfo("Registered global post-callback (total: " + std::to_string(m_GlobalPostCallbacks.size()) + ")");
}

std::vector<PreHookCallback> HookSystem::GetGlobalPreCallbacks() const
{
    std::lock_guard<std::mutex> lock(m_GlobalCallbacksMutex);
    return m_GlobalPreCallbacks;
}

std::vector<PostHookCallback> HookSystem::GetGlobalPostCallbacks() const
{
    std::lock_guard<std::mutex> lock(m_GlobalCallbacksMutex);
    return m_GlobalPostCallbacks;
}

}  // namespace PluginAPI
