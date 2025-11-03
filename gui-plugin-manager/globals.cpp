#include "stdafx.h"

HMODULE g_mainModule = nullptr;

namespace globals {
    HMODULE mainModule = nullptr;
    HWND mainWindow = nullptr;
    int openMenuKey = VK_F1;
    Backend preferredBackend = Backend::DX12;
    bool enableDebugLog = false;
    Backend activeBackend = Backend::None;
}

namespace globals {
    void SetDebugLogging(bool enable) {
        enableDebugLog = enable;
    }

    void SetMainModule(HMODULE hModule) {
        mainModule = hModule;
        g_mainModule = hModule;
    }
}

static void LogGlobals() {
    DebugLog("[Globals] mainModule=%p, mainWindow=%p, openMenuKey=0x%X, activeBackend=%d, preferredBackend=%d\n",
        globals::mainModule, globals::mainWindow, globals::openMenuKey,
        static_cast<int>(globals::activeBackend), static_cast<int>(globals::preferredBackend));
}

struct GlobalsLogger {
    GlobalsLogger() {
        LogGlobals();
    }
};

static GlobalsLogger _globalsLogger;
