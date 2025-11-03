#include "stdafx.h"
#include "PluginLib/HotkeyManager.h"

namespace hooks { void Remove(); }

namespace d3d12hook {
    PresentD3D12            oPresentD3D12 = nullptr;
    Present1Fn              oPresent1D3D12 = nullptr;
    ExecuteCommandListsFn   oExecuteCommandListsD3D12 = nullptr;
    ResizeBuffersFn         oResizeBuffersD3D12 = nullptr;

    // Global DirectX 12 resources (accessed only from DX12 rendering thread)
    // NOTE: These are NOT protected by locks because they are only accessed from
    // the DX12 rendering thread. The DirectX execution model guarantees single-threaded
    // access to rendering calls. This is safe because:
    // 1. hookPresentD3D12/Present1D3D12 are called on DX12 thread during Present()
    // 2. hookExecuteCommandLists is called on DX12 thread during work submission
    // 3. hookResizeBuffers is called on DX12 thread during window resize
    // 4. Initialization order: ExecuteCommandLists sets gCommandQueue before Present uses it

    static ID3D12Device* gDevice = nullptr;
    static ID3D12CommandQueue* gCommandQueue = nullptr;
    static ID3D12DescriptorHeap* gHeapRTV = nullptr;
    static ID3D12DescriptorHeap* gHeapSRV = nullptr;
    static ID3D12GraphicsCommandList* gCommandList = nullptr;
    static ID3D12Fence* gOverlayFence = nullptr;
    static HANDLE                   gFenceEvent = nullptr;
    static UINT64                  gOverlayFenceValue = 0;
    static uintx_t                 gBufferCount = 0;

    struct FrameContext {
        ID3D12CommandAllocator* allocator;
        ID3D12Resource* renderTarget;
        D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle;
    };
    static FrameContext* gFrameContexts = nullptr;
    static bool                   gInitialized = false;
    static bool                   gShutdown = false;
    static bool                   gAfterFirstPresent = false;

    // F1 hotkey lock state
    static auto initTime = std::chrono::high_resolution_clock::now();
    static auto f1UnlockTime = std::chrono::high_resolution_clock::now() + std::chrono::seconds(5);
    static bool f1LockActive = true;
    static auto notificationStartTime = std::chrono::high_resolution_clock::now();
    static bool notificationActive = false;
    static const int F1_LOCK_DURATION_MS = 5000;
    static const int NOTIFICATION_DISPLAY_DURATION_MS = 5000;

    void release();

    // Utility to log HRESULTs
    inline void LogHRESULT(const char* label, HRESULT hr) {
        DebugLog("[d3d12hook] %s: hr=0x%08X\n", label, hr);
    }

    // Helper to safely cleanup after initialization failure
    static void CleanupPartialInit() {
        DebugLog("[d3d12hook] Cleaning up partial initialization.\n");

        if (gCommandList) {
            gCommandList->Release();
            gCommandList = nullptr;
        }
        if (gHeapRTV) {
            gHeapRTV->Release();
            gHeapRTV = nullptr;
        }
        if (gHeapSRV) {
            gHeapSRV->Release();
            gHeapSRV = nullptr;
        }
        if (gOverlayFence) {
            gOverlayFence->Release();
            gOverlayFence = nullptr;
        }
        if (gFenceEvent) {
            CloseHandle(gFenceEvent);
            gFenceEvent = nullptr;
        }
        if (gFrameContexts) {
            for (UINT i = 0; i < gBufferCount; ++i) {
                if (gFrameContexts[i].allocator) {
                    gFrameContexts[i].allocator->Release();
                    gFrameContexts[i].allocator = nullptr;
                }
                if (gFrameContexts[i].renderTarget) {
                    gFrameContexts[i].renderTarget->Release();
                    gFrameContexts[i].renderTarget = nullptr;
                }
            }
            delete[] gFrameContexts;
            gFrameContexts = nullptr;
        }
        gBufferCount = 0;
        gOverlayFenceValue = 0;
    }

    long __fastcall hookPresentD3D12(IDXGISwapChain3* pSwapChain, UINT SyncInterval, UINT Flags) {
        // Check if F1 lock period has expired
        auto now = std::chrono::high_resolution_clock::now();
        if (f1LockActive && now >= f1UnlockTime) {
            f1LockActive = false;
            DebugLog("[d3d12hook] F1 hotkey unlocked.\n");
        }

        // Check if notification should be displayed
        if (notificationActive && (now - notificationStartTime) >= std::chrono::milliseconds(F1_LOCK_DURATION_MS)) {
            notificationActive = true;  // Keep showing for 5 seconds from unlock
        }
        if (notificationActive && (now - notificationStartTime) >= std::chrono::milliseconds(F1_LOCK_DURATION_MS + NOTIFICATION_DISPLAY_DURATION_MS)) {
            notificationActive = false;
            DebugLog("[d3d12hook] Plugin Loader Ready notification expired.\n");
        }

        // Create hotkey object
        HotkeyManager::Hotkey menuKey(globals::openMenuKey, false, false, false);

        // Edge detection for menu key to prevent stuttering
        static bool wasMenuKeyPressed = false;
        bool isMenuKeyPressed = !f1LockActive && HotkeyManager::IsHotkeyPressed(menuKey);
        if (isMenuKeyPressed && !wasMenuKeyPressed) {
            menu::isOpen = !menu::isOpen;
            DebugLog("[d3d12hook] Toggle menu: isOpen=%d\n", (bool)menu::isOpen);
        }
        wasMenuKeyPressed = isMenuKeyPressed;

        gAfterFirstPresent = true;
        if (!gCommandQueue) {
            DebugLog("[d3d12hook] CommandQueue not yet captured, skipping frame\n");
            if (!gDevice) {
                pSwapChain->GetDevice(__uuidof(ID3D12Device), (void**)&gDevice);
            }
            return oPresentD3D12(pSwapChain, SyncInterval, Flags);
        }

        if (!gInitialized) {
            DebugLog("[d3d12hook] Initializing ImGui on first Present.\n");
            // Start F1 lock timer and notification
            initTime = std::chrono::high_resolution_clock::now();
            f1UnlockTime = initTime + std::chrono::milliseconds(F1_LOCK_DURATION_MS);
            f1LockActive = true;
            notificationActive = true;
            notificationStartTime = initTime;
            DebugLog("[d3d12hook] ImGui initialized.\n");
            if (FAILED(pSwapChain->GetDevice(__uuidof(ID3D12Device), (void**)&gDevice))) {
                LogHRESULT("GetDevice", E_FAIL);
                return oPresentD3D12(pSwapChain, SyncInterval, Flags);
            }

            // Swap Chain description
            DXGI_SWAP_CHAIN_DESC desc = {};
            pSwapChain->GetDesc(&desc);
            gBufferCount = desc.BufferCount;
            DebugLog("[d3d12hook] BufferCount=%u\n", gBufferCount);

            // Create descriptor heaps
            D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
            heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
            heapDesc.NumDescriptors = gBufferCount;
            if (FAILED(gDevice->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&gHeapRTV)))) {
                LogHRESULT("CreateDescriptorHeap RTV", E_FAIL);
                CleanupPartialInit();
                return oPresentD3D12(pSwapChain, SyncInterval, Flags);
            }

            heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
            heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
            if (FAILED(gDevice->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&gHeapSRV)))) {
                LogHRESULT("CreateDescriptorHeap SRV", E_FAIL);
                CleanupPartialInit();
                return oPresentD3D12(pSwapChain, SyncInterval, Flags);
            }

            // Allocate frame contexts
            gFrameContexts = new FrameContext[gBufferCount];
            ZeroMemory(gFrameContexts, sizeof(FrameContext) * gBufferCount);

            // Create command allocator for each frame
            for (UINT i = 0; i < gBufferCount; ++i) {
                if (FAILED(gDevice->CreateCommandAllocator(
                        D3D12_COMMAND_LIST_TYPE_DIRECT,
                        IID_PPV_ARGS(&gFrameContexts[i].allocator)))) {
                    LogHRESULT("CreateCommandAllocator", E_FAIL);
                    CleanupPartialInit();
                    return oPresentD3D12(pSwapChain, SyncInterval, Flags);
                }
            }

            // Create RTVs for each back buffer
            UINT rtvSize = gDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
            auto rtvHandle = gHeapRTV->GetCPUDescriptorHandleForHeapStart();
            for (UINT i = 0; i < gBufferCount; ++i) {
                ID3D12Resource* back;
                pSwapChain->GetBuffer(i, IID_PPV_ARGS(&back));
                gDevice->CreateRenderTargetView(back, nullptr, rtvHandle);
                gFrameContexts[i].renderTarget = back;
                gFrameContexts[i].rtvHandle = rtvHandle;
                rtvHandle.ptr += rtvSize;
            }

            // ImGui setup
            ImGui::CreateContext();
            ImGuiIO& io = ImGui::GetIO(); (void)io;
            io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
            ImGui::StyleColorsDark();
            ImGui_ImplWin32_Init(desc.OutputWindow);
            ImGui_ImplDX12_Init(gDevice, gBufferCount,
                desc.BufferDesc.Format,
                gHeapSRV,
                gHeapSRV->GetCPUDescriptorHandleForHeapStart(),
                gHeapSRV->GetGPUDescriptorHandleForHeapStart());
            DebugLog("[d3d12hook] ImGui initialized.\n");

            inputhook::Init(desc.OutputWindow);

            if (!gOverlayFence) {
                if (FAILED(gDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&gOverlayFence)))) {
                    LogHRESULT("CreateFence", E_FAIL);
                    CleanupPartialInit();
                    return oPresentD3D12(pSwapChain, SyncInterval, Flags);
                }
            }

            if (!gFenceEvent) {
                gFenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
                if (!gFenceEvent) {
                    DebugLog("[d3d12hook] Failed to create fence event: %lu\n", GetLastError());
                    CleanupPartialInit();
                    return oPresentD3D12(pSwapChain, SyncInterval, Flags);
                }
            }

            // Hook CommandQueue and Fence are already captured by minhook
            gInitialized = true;
        }

        if (!gShutdown) {
            // Render ImGui
            ImGui_ImplDX12_NewFrame();
            ImGui_ImplWin32_NewFrame();
            ImGui::NewFrame();

            // Always call menu::Init() to ensure plugins are loaded
            // The menu will only render UI if menu::isOpen is true
            menu::Init();

            UINT frameIdx = pSwapChain->GetCurrentBackBufferIndex();
            FrameContext& ctx = gFrameContexts[frameIdx];

            // Wait for the GPU to finish with the previous frame
            bool canRender = true;
            if (!gOverlayFence || !gFenceEvent) {
                // Missing synchronization objects, skip waiting
            } else if (gOverlayFence->GetCompletedValue() < gOverlayFenceValue) {
                HRESULT hr = gOverlayFence->SetEventOnCompletion(gOverlayFenceValue, gFenceEvent);
                if (SUCCEEDED(hr)) {
                    const DWORD waitTimeoutMs = 2000; // Extended timeout
                    DWORD waitRes = WaitForSingleObject(gFenceEvent, waitTimeoutMs);
                    if (waitRes == WAIT_TIMEOUT) {
                        DebugLog("[d3d12hook] WaitForSingleObject timeout\n");
                        canRender = false;
                    } else if (waitRes != WAIT_OBJECT_0) {
                        DebugLog("[d3d12hook] WaitForSingleObject failed: %lu\n", GetLastError());
                        canRender = false;
                    }
                } else {
                    LogHRESULT("SetEventOnCompletion", hr);
                    canRender = false;
                }
            }

            if (!canRender) {
                DebugLog("[d3d12hook] Skipping ImGui render for this frame\n");
                ImGui::EndFrame();
                return oPresentD3D12(pSwapChain, SyncInterval, Flags);
            }

            // Render "Plugin Loader Ready" notification
            if (notificationActive) {
                ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
                                        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing |
                                        ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoInputs;
                ImVec2 screenSize = ImGui::GetIO().DisplaySize;
                ImVec2 bottomRightPos(screenSize.x - 300 - 20, screenSize.y - 80 - 20);
                ImGui::SetNextWindowPos(bottomRightPos, ImGuiCond_Always);
                ImGui::SetNextWindowBgAlpha(0.8f);
                if (ImGui::Begin("PluginLoaderReady", nullptr, flags)) {
                    ImGui::TextColored(ImVec4(0.2f, 0.8f, 0.2f, 1.0f), "Plugin Loader Ready");
                }
                ImGui::End();
            }

            // Reset allocator and command list using frame-specific allocator
            HRESULT hr = ctx.allocator->Reset();
            if (FAILED(hr)) {
                LogHRESULT("CommandAllocator->Reset", hr);
                return oPresentD3D12(pSwapChain, SyncInterval, Flags);
            }

            if (!gCommandList) {
                hr = gDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT,
                    ctx.allocator, nullptr, IID_PPV_ARGS(&gCommandList));
                if (FAILED(hr)) {
                    LogHRESULT("CreateCommandList", hr);
                    return oPresentD3D12(pSwapChain, SyncInterval, Flags);
                }
                gCommandList->Close();
            }
            hr = gCommandList->Reset(ctx.allocator, nullptr);
            if (FAILED(hr)) {
                LogHRESULT("CommandList->Reset", hr);
                return oPresentD3D12(pSwapChain, SyncInterval, Flags);
            }

            // Transition to render target
            D3D12_RESOURCE_BARRIER barrier = {};
            barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            barrier.Transition.pResource = ctx.renderTarget;
            barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
            barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
            gCommandList->ResourceBarrier(1, &barrier);

            gCommandList->OMSetRenderTargets(1, &ctx.rtvHandle, FALSE, nullptr);
            ID3D12DescriptorHeap* heaps[] = { gHeapSRV };
            gCommandList->SetDescriptorHeaps(1, heaps);

            ImGui::Render();
            ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), gCommandList);

            // Transition back to present
            barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
            barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
            gCommandList->ResourceBarrier(1, &barrier);
            gCommandList->Close();

            // Execute
            if (!gCommandQueue) {
                DebugLog("[d3d12hook] CommandQueue not set, skipping ExecuteCommandLists.\n");
            }
            else {
                oExecuteCommandListsD3D12(gCommandQueue, 1, reinterpret_cast<ID3D12CommandList* const*>(&gCommandList));
                if (gOverlayFence) {
                    // Call Signal directly on the command queue to synchronize the internal overlay.
                    HRESULT hr = gCommandQueue->Signal(gOverlayFence, ++gOverlayFenceValue);
                    if (FAILED(hr)) {
                        LogHRESULT("Signal", hr);
                        if (gDevice) {
                            HRESULT reason = gDevice->GetDeviceRemovedReason();
                            DebugLog("[d3d12hook] DeviceRemovedReason=0x%08X\n", reason);
                            if (reason != S_OK) {
                                DebugLog("[d3d12hook] Device lost. Releasing resources.\n");
                                release();
                            }
                        }
                    }
                }
            }
        }

        return oPresentD3D12(pSwapChain, SyncInterval, Flags);
    }

    long __fastcall hookPresent1D3D12(IDXGISwapChain3* pSwapChain, UINT SyncInterval, UINT Flags, const DXGI_PRESENT_PARAMETERS* pParams) {
        if (GetAsyncKeyState(globals::openMenuKey) & 1) {
            menu::isOpen = !menu::isOpen;
            DebugLog("[d3d12hook] Toggle menu: isOpen=%d\n", (bool)menu::isOpen);
        }

        gAfterFirstPresent = true;
        if (!gCommandQueue) {
            DebugLog("[d3d12hook] CommandQueue not yet captured, skipping frame\n");
            if (!gDevice) {
                pSwapChain->GetDevice(__uuidof(ID3D12Device), (void**)&gDevice);
            }
            return oPresent1D3D12(pSwapChain, SyncInterval, Flags, pParams);
        }

        if (!gInitialized) {
            DebugLog("[d3d12hook] Initializing ImGui on first Present1.\n");
            if (FAILED(pSwapChain->GetDevice(__uuidof(ID3D12Device), (void**)&gDevice))) {
                LogHRESULT("GetDevice", E_FAIL);
                return oPresent1D3D12(pSwapChain, SyncInterval, Flags, pParams);
            }

            // Swap Chain description
            DXGI_SWAP_CHAIN_DESC desc = {};
            pSwapChain->GetDesc(&desc);
            gBufferCount = desc.BufferCount;
            DebugLog("[d3d12hook] BufferCount=%u\n", gBufferCount);

            // Create descriptor heaps
            D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
            heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
            heapDesc.NumDescriptors = gBufferCount;
            if (FAILED(gDevice->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&gHeapRTV)))) {
                LogHRESULT("CreateDescriptorHeap RTV", E_FAIL);
                CleanupPartialInit();
                return oPresent1D3D12(pSwapChain, SyncInterval, Flags, pParams);
            }

            heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
            heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
            if (FAILED(gDevice->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&gHeapSRV)))) {
                LogHRESULT("CreateDescriptorHeap SRV", E_FAIL);
                CleanupPartialInit();
                return oPresent1D3D12(pSwapChain, SyncInterval, Flags, pParams);
            }

            // Allocate frame contexts
            gFrameContexts = new FrameContext[gBufferCount];
            ZeroMemory(gFrameContexts, sizeof(FrameContext) * gBufferCount);

            // Create command allocator for each frame
            for (UINT i = 0; i < gBufferCount; ++i) {
                if (FAILED(gDevice->CreateCommandAllocator(
                        D3D12_COMMAND_LIST_TYPE_DIRECT,
                        IID_PPV_ARGS(&gFrameContexts[i].allocator)))) {
                    LogHRESULT("CreateCommandAllocator", E_FAIL);
                    CleanupPartialInit();
                    return oPresent1D3D12(pSwapChain, SyncInterval, Flags, pParams);
                }
            }

            // Create RTVs for each back buffer
            UINT rtvSize = gDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
            auto rtvHandle = gHeapRTV->GetCPUDescriptorHandleForHeapStart();
            for (UINT i = 0; i < gBufferCount; ++i) {
                ID3D12Resource* back;
                pSwapChain->GetBuffer(i, IID_PPV_ARGS(&back));
                gDevice->CreateRenderTargetView(back, nullptr, rtvHandle);
                gFrameContexts[i].renderTarget = back;
                gFrameContexts[i].rtvHandle = rtvHandle;
                rtvHandle.ptr += rtvSize;
            }

            // ImGui setup
            ImGui::CreateContext();
            ImGuiIO& io = ImGui::GetIO(); (void)io;
            io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
            ImGui::StyleColorsDark();
            ImGui_ImplWin32_Init(desc.OutputWindow);
            ImGui_ImplDX12_Init(gDevice, gBufferCount,
                desc.BufferDesc.Format,
                gHeapSRV,
                gHeapSRV->GetCPUDescriptorHandleForHeapStart(),
                gHeapSRV->GetGPUDescriptorHandleForHeapStart());
            DebugLog("[d3d12hook] ImGui initialized.\n");

            inputhook::Init(desc.OutputWindow);

            if (!gOverlayFence) {
                if (FAILED(gDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&gOverlayFence)))) {
                    LogHRESULT("CreateFence", E_FAIL);
                    CleanupPartialInit();
                    return oPresent1D3D12(pSwapChain, SyncInterval, Flags, pParams);
                }
            }

            if (!gFenceEvent) {
                gFenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
                if (!gFenceEvent) {
                    DebugLog("[d3d12hook] Failed to create fence event: %lu\n", GetLastError());
                    CleanupPartialInit();
                    return oPresent1D3D12(pSwapChain, SyncInterval, Flags, pParams);
                }
            }

            // Hook CommandQueue and Fence are already captured by minhook
            gInitialized = true;
        }

        if (!gShutdown) {
            // Render ImGui
            ImGui_ImplDX12_NewFrame();
            ImGui_ImplWin32_NewFrame();
            ImGui::NewFrame();

            // Always call menu::Init() to ensure plugins are loaded
            // The menu will only render UI if menu::isOpen is true
            menu::Init();

            UINT frameIdx = pSwapChain->GetCurrentBackBufferIndex();
            FrameContext& ctx = gFrameContexts[frameIdx];

            // Wait for the GPU to finish with the previous frame
            bool canRender = true;
            if (!gOverlayFence || !gFenceEvent) {
                // Missing synchronization objects, skip waiting
            } else if (gOverlayFence->GetCompletedValue() < gOverlayFenceValue) {
                HRESULT hr = gOverlayFence->SetEventOnCompletion(gOverlayFenceValue, gFenceEvent);
                if (SUCCEEDED(hr)) {
                    const DWORD waitTimeoutMs = 2000; // Extended timeout
                    DWORD waitRes = WaitForSingleObject(gFenceEvent, waitTimeoutMs);
                    if (waitRes == WAIT_TIMEOUT) {
                        DebugLog("[d3d12hook] WaitForSingleObject timeout\n");
                        canRender = false;
                    } else if (waitRes != WAIT_OBJECT_0) {
                        DebugLog("[d3d12hook] WaitForSingleObject failed: %lu\n", GetLastError());
                        canRender = false;
                    }
                } else {
                    LogHRESULT("SetEventOnCompletion", hr);
                    canRender = false;
                }
            }

            if (!canRender) {
                DebugLog("[d3d12hook] Skipping ImGui render for this frame\n");
                ImGui::EndFrame();
                return oPresent1D3D12(pSwapChain, SyncInterval, Flags, pParams);
            }

            // Render "Plugin Loader Ready" notification
            if (notificationActive) {
                ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
                                        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing |
                                        ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoInputs;
                ImVec2 screenSize = ImGui::GetIO().DisplaySize;
                ImVec2 bottomRightPos(screenSize.x - 300 - 20, screenSize.y - 80 - 20);
                ImGui::SetNextWindowPos(bottomRightPos, ImGuiCond_Always);
                ImGui::SetNextWindowBgAlpha(0.8f);
                if (ImGui::Begin("PluginLoaderReady", nullptr, flags)) {
                    ImGui::TextColored(ImVec4(0.2f, 0.8f, 0.2f, 1.0f), "Plugin Loader Ready");
                }
                ImGui::End();
            }

            // Reset allocator and command list using frame-specific allocator
            HRESULT hr = ctx.allocator->Reset();
            if (FAILED(hr)) {
                LogHRESULT("CommandAllocator->Reset", hr);
                return oPresent1D3D12(pSwapChain, SyncInterval, Flags, pParams);
            }

            if (!gCommandList) {
                hr = gDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT,
                    ctx.allocator, nullptr, IID_PPV_ARGS(&gCommandList));
                if (FAILED(hr)) {
                    LogHRESULT("CreateCommandList", hr);
                    return oPresent1D3D12(pSwapChain, SyncInterval, Flags, pParams);
                }
                gCommandList->Close();
            }
            hr = gCommandList->Reset(ctx.allocator, nullptr);
            if (FAILED(hr)) {
                LogHRESULT("CommandList->Reset", hr);
                return oPresent1D3D12(pSwapChain, SyncInterval, Flags, pParams);
            }

            // Transition to render target
            D3D12_RESOURCE_BARRIER barrier = {};
            barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            barrier.Transition.pResource = ctx.renderTarget;
            barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
            barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
            gCommandList->ResourceBarrier(1, &barrier);

            gCommandList->OMSetRenderTargets(1, &ctx.rtvHandle, FALSE, nullptr);
            ID3D12DescriptorHeap* heaps[] = { gHeapSRV };
            gCommandList->SetDescriptorHeaps(1, heaps);

            ImGui::Render();
            ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), gCommandList);

            // Transition back to present
            barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
            barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
            gCommandList->ResourceBarrier(1, &barrier);
            gCommandList->Close();

            // Execute
            if (!gCommandQueue) {
                DebugLog("[d3d12hook] CommandQueue not set, skipping ExecuteCommandLists.\n");
            }
            else {
                oExecuteCommandListsD3D12(gCommandQueue, 1, reinterpret_cast<ID3D12CommandList* const*>(&gCommandList));
                if (gOverlayFence) {
                    // Call Signal directly on the command queue to synchronize the internal overlay.
                    HRESULT hr = gCommandQueue->Signal(gOverlayFence, ++gOverlayFenceValue);
                    if (FAILED(hr)) {
                        LogHRESULT("Signal", hr);
                        if (gDevice) {
                            HRESULT reason = gDevice->GetDeviceRemovedReason();
                            DebugLog("[d3d12hook] DeviceRemovedReason=0x%08X\n", reason);
                            if (reason != S_OK) {
                                DebugLog("[d3d12hook] Device lost. Releasing resources.\n");
                                release();
                            }
                        }
                    }
                }
            }
        }

        return oPresent1D3D12(pSwapChain, SyncInterval, Flags, pParams);
    }

    void STDMETHODCALLTYPE hookExecuteCommandListsD3D12(
        ID3D12CommandQueue* _this,
        UINT                          NumCommandLists,
        ID3D12CommandList* const* ppCommandLists) {
        if (!gCommandQueue && gAfterFirstPresent) {
            ID3D12Device* queueDevice = nullptr;
            if (SUCCEEDED(_this->GetDevice(__uuidof(ID3D12Device), (void**)&queueDevice))) {
                if (!gDevice && queueDevice) {
                    // GetDevice increments ref count, so we take ownership
                    gDevice = queueDevice;
                    queueDevice = nullptr;  // Mark as taken
                }

                if (queueDevice == gDevice) {
                    D3D12_COMMAND_QUEUE_DESC desc = _this->GetDesc();
                    DebugLog("[d3d12hook] CommandQueue type=%u\n", desc.Type);
                    if (desc.Type == D3D12_COMMAND_LIST_TYPE_DIRECT) {
                        _this->AddRef();
                        gCommandQueue = _this;
                        DebugLog("[d3d12hook] Captured CommandQueue=%p\n", _this);
                    }
                    else {
                        DebugLog("[d3d12hook] Skipping capture: non-direct queue\n");
                    }
                }
                else if (gDevice && queueDevice != gDevice) {
                    DebugLog("[d3d12hook] Skipping capture: CommandQueue uses different device (%p != %p)\n", queueDevice, gDevice);
                }

                // Always release the reference from GetDevice if we don't own it
                if (queueDevice) {
                    queueDevice->Release();
                }
            }
        }
        gAfterFirstPresent = false;
        oExecuteCommandListsD3D12(_this, NumCommandLists, ppCommandLists);
    }

    HRESULT STDMETHODCALLTYPE hookResizeBuffersD3D12(
        IDXGISwapChain3* pSwapChain,
        UINT BufferCount,
        UINT Width,
        UINT Height,
        DXGI_FORMAT NewFormat,
        UINT SwapChainFlags)
    {
        DebugLog("[d3d12hook] ResizeBuffers called: %ux%u Buffers=%u\n",
            Width, Height, BufferCount);

        if (gInitialized)
        {
            DebugLog("[d3d12hook] Releasing resources for resize\n");

            ImGui_ImplDX12_Shutdown();
            ImGui_ImplWin32_Shutdown();
            ImGui::DestroyContext();
            inputhook::Remove(globals::mainWindow);

            if (gCommandList)
            {
                gCommandList->Release();
                gCommandList = nullptr;
            }
            if (gHeapRTV)
            {
                gHeapRTV->Release();
                gHeapRTV = nullptr;
            }
            if (gHeapSRV)
            {
                gHeapSRV->Release();
                gHeapSRV = nullptr;
            }

            for (UINT i = 0; i < gBufferCount; ++i)
            {
                if (gFrameContexts[i].renderTarget)
                {
                    gFrameContexts[i].renderTarget->Release();
                    gFrameContexts[i].renderTarget = nullptr;
                }
                if (gFrameContexts[i].allocator)
                {
                    gFrameContexts[i].allocator->Release();
                    gFrameContexts[i].allocator = nullptr;
                }
            }

            delete[] gFrameContexts;
            gFrameContexts = nullptr;
            gBufferCount = 0;

            gInitialized = false;
        }

        return oResizeBuffersD3D12(
            pSwapChain, BufferCount, Width, Height, NewFormat, SwapChainFlags);
    }

    void release() {
        DebugLog("[d3d12hook] Releasing resources and hooks.\n");
        gShutdown = true;
        if (globals::mainWindow) {
            inputhook::Remove(globals::mainWindow);
        }

        // Shutdown ImGui before releasing any D3D resources
        if (gInitialized && ImGui::GetCurrentContext())
        {
            ImGui_ImplDX12_Shutdown();
            ImGui_ImplWin32_Shutdown();
            ImGui::DestroyContext();
            gInitialized = false;
        }

        // Release all DirectX resources with proper null checks and null assignment
        if (gCommandList) {
            gCommandList->Release();
            gCommandList = nullptr;
        }
        if (gHeapRTV) {
            gHeapRTV->Release();
            gHeapRTV = nullptr;
        }
        if (gHeapSRV) {
            gHeapSRV->Release();
            gHeapSRV = nullptr;
        }

        // Release frame contexts
        if (gFrameContexts) {
            for (UINT i = 0; i < gBufferCount; ++i) {
                if (gFrameContexts[i].renderTarget) {
                    gFrameContexts[i].renderTarget->Release();
                    gFrameContexts[i].renderTarget = nullptr;
                }
                if (gFrameContexts[i].allocator) {
                    gFrameContexts[i].allocator->Release();
                    gFrameContexts[i].allocator = nullptr;
                }
            }
            delete[] gFrameContexts;
            gFrameContexts = nullptr;
        }

        if (gOverlayFence) {
            gOverlayFence->Release();
            gOverlayFence = nullptr;
        }

        if (gFenceEvent) {
            CloseHandle(gFenceEvent);
            gFenceEvent = nullptr;
        }

        if (gCommandQueue) {
            gCommandQueue->Release();
            gCommandQueue = nullptr;
        }

        if (gDevice) {
            gDevice->Release();
            gDevice = nullptr;
        }

        gBufferCount = 0;
        gOverlayFenceValue = 0;

        // Disable hooks installed for D3D12
        hooks::Remove();
    }

    bool IsInitialized()
    {
        return gInitialized;
    }
}
