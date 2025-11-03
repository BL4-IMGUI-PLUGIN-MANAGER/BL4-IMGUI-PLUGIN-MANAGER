#include "stdafx.h"

namespace mousehooks { void Init(); void Remove(); }

using IsInitFn = bool (*)();

static bool WaitForInitialization(IsInitFn fn, int attempts = 50, int sleepMs = 100)
{
    for (int i = 0; i < attempts; ++i)
    {
        if (fn())
            return true;
        Sleep(sleepMs);
    }
    return false;
}

static bool TryInitBackend(globals::Backend backend)
{
    switch (backend)
    {
    case globals::Backend::DX12:
        if (GetModuleHandleA("d3d12.dll") || GetModuleHandleA("dxgi.dll"))
        {
            DebugLog("[DllMain] Attempting DX12 initialization.\n");
            hooks::Init();

            // Don't wait for ImGui to initialize - it will init on first Present call
            // Just verify hooks were installed successfully
            DebugLog("[DllMain] DX12 hooks installed. ImGui will initialize on first frame.\n");
            globals::activeBackend = globals::Backend::DX12;
            return true;
        }
        break;
    default:
        break;
    }
    return false;
}

static bool TryInitializeFrom(globals::Backend start)
{
    if (TryInitBackend(globals::Backend::DX12))
        return true;

    DebugLog("[DllMain] No backend initialized.\n");
    return false;
}

using LoadLibraryA_t = HMODULE(WINAPI*)(LPCSTR);
using LoadLibraryW_t = HMODULE(WINAPI*)(LPCWSTR);
static LoadLibraryA_t oLoadLibraryA = nullptr;
static LoadLibraryW_t oLoadLibraryW = nullptr;

static void InitForModule(const char* name)
{
    if (!name)
        return;

    const char* base = strrchr(name, '\\');
    base = base ? base + 1 : name;

    globals::Backend detected = globals::Backend::None;
    if (_stricmp(base, "d3d12.dll") == 0 || _stricmp(base, "dxgi.dll") == 0) {
        detected = globals::Backend::DX12;
    }
    else {
        return;
    }

    if (globals::preferredBackend != globals::Backend::None && detected != globals::preferredBackend)
        return;

    if (globals::activeBackend == globals::Backend::DX12)
        return;

    TryInitBackend(globals::Backend::DX12);
}

static HMODULE WINAPI hookLoadLibraryA(LPCSTR lpLibFileName)
{
    HMODULE mod = oLoadLibraryA(lpLibFileName);
    if (mod)
        InitForModule(lpLibFileName);
    return mod;
}

static HMODULE WINAPI hookLoadLibraryW(LPCWSTR lpLibFileName)
{
    HMODULE mod = oLoadLibraryW(lpLibFileName);
    if (mod && lpLibFileName)
    {
        char name[MAX_PATH];
        WideCharToMultiByte(CP_ACP, 0, lpLibFileName, -1, name, MAX_PATH, nullptr, nullptr);
        InitForModule(name);
    }
    return mod;
}

static DWORD WINAPI onAttach(LPVOID lpParameter)
{
    // Initialize logger with default path
    GUILogger::Get().Initialize("Plugin_Manager/Plugin_Manager_GUI.log");
    globals::SetDebugLogging(true);

    // Use F1 as default menu hotkey
    globals::openMenuKey = VK_F1;

    DebugLog("[DllMain] onAttach starting.\n");
    DebugLog("[DllMain] Menu hotkey: VK 0x%X\n", globals::openMenuKey);

    {
        MH_STATUS mhStatus = MH_Initialize();
        if (mhStatus != MH_OK) {
            DebugLog("[DllMain] MinHook initialization failed: %s\n",
                MH_StatusToString(mhStatus));
            return 1;
        }
        DebugLog("[DllMain] MinHook initialized.\n");
    }

    TryInitBackend(globals::Backend::DX12);

    HMODULE k32 = GetModuleHandleA("kernel32.dll");
    if (k32) {
        LPVOID addrA = GetProcAddress(k32, "LoadLibraryA");
        LPVOID addrW = GetProcAddress(k32, "LoadLibraryW");
        if (addrA) {
            MH_CreateHook(addrA, reinterpret_cast<LPVOID>(hookLoadLibraryA), reinterpret_cast<LPVOID*>(&oLoadLibraryA));
            MH_EnableHook(addrA);
            DebugLog("[DllMain] Hooked LoadLibraryA@%p\n", addrA);
        }
        if (addrW) {
            MH_CreateHook(addrW, reinterpret_cast<LPVOID>(hookLoadLibraryW), reinterpret_cast<LPVOID*>(&oLoadLibraryW));
            MH_EnableHook(addrW);
            DebugLog("[DllMain] Hooked LoadLibraryW@%p\n", addrW);
        }
    }

    mousehooks::Init();

    DebugLog("[DllMain] Hook initialization completed.\n");
    return 0;
}

BOOL WINAPI DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
    switch (ul_reason_for_call) {
    case DLL_PROCESS_ATTACH:
        DebugLog("[DllMain] DLL_PROCESS_ATTACH: hModule=%p\n", hModule);
        globals::SetMainModule(hModule);
        // Create a thread for hook setup to avoid blocking loading
        {
            HANDLE thread = CreateThread(
                nullptr, 0,
                onAttach,
                nullptr,
                0,
                nullptr
            );
            if (thread) CloseHandle(thread);
            else DebugLog("[DllMain] Failed to create hook thread: %d\n", GetLastError());
        }
        break;

    case DLL_PROCESS_DETACH:
        DebugLog("[DllMain] DLL_PROCESS_DETACH. Releasing hooks and uninitializing MinHook.\n");
        if (globals::activeBackend == globals::Backend::DX12) {
            d3d12hook::release();
        }
        mousehooks::Remove();
        MH_DisableHook(MH_ALL_HOOKS);
        MH_RemoveHook(MH_ALL_HOOKS);
        MH_Uninitialize();
        GUILogger::Get().Shutdown();
        break;
    }
    return TRUE;
}
