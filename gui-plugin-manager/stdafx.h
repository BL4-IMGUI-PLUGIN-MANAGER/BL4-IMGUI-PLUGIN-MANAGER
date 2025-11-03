#pragma once

#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3d12.lib")

#include <windows.h>
#include <vector>
#include <atomic>
#include <chrono>
#include <cstring>
#include <cstdio>
#include <cstdint>
#include <cstddef>
#include <cstdarg>

#include <dxgi.h>
#include <dxgi1_4.h>
#include <d3d12.h>

#include <wrl/client.h>

#if defined _M_X64
typedef uint64_t uintx_t;
#elif defined _M_IX86
typedef uint32_t uintx_t;
#endif

#include "imgui/imgui.h"
#include "imgui/imgui_impl_win32.h"
#include "imgui/imgui_impl_dx12.h"

#include "minhook/include/MinHook.h"

#include "namespaces.h"

class GUILogger;

// Helper macro for debug logging via DebugView and file
inline void DebugLog(const char* fmt, ...);

#include "GUILogger.h"
#include "Settings.h"

inline void DebugLog(const char* fmt, ...) {
    if (!globals::enableDebugLog) {
        return;
    }
    char buf[512];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    GUILogger::Get().Log("%s", buf);
}
