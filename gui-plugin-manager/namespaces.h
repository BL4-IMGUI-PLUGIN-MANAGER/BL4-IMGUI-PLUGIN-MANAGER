#pragma once

namespace globals {
        extern HMODULE mainModule;
        extern HWND mainWindow;
        extern int openMenuKey;

        // Rendering backend currently in use
        enum class Backend {
                None,
                DX12
        };
        extern Backend activeBackend;
        // Preferred backend to hook. None means auto with fallback order
        extern Backend preferredBackend;
        extern bool enableDebugLog;
        void SetDebugLogging(bool enable);
        void SetMainModule(HMODULE hModule);
}

namespace hooks {
        extern void Init();
}

namespace inputhook {
        extern void Init(HWND hWindow);
        extern void Remove(HWND hWindow);
        static LRESULT APIENTRY WndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
}

namespace mousehooks {
        void Init();
        void Remove();
}

namespace d3d12hook {
        typedef HRESULT(STDMETHODCALLTYPE* PresentD3D12)(
                IDXGISwapChain3 * pSwapChain, UINT SyncInterval, UINT Flags);
        typedef HRESULT(STDMETHODCALLTYPE* Present1Fn)(
                IDXGISwapChain3 * pSwapChain, UINT SyncInterval, UINT Flags, const DXGI_PRESENT_PARAMETERS* pParams);
        extern PresentD3D12 oPresentD3D12;
        extern Present1Fn   oPresent1D3D12;

	typedef void(STDMETHODCALLTYPE* ExecuteCommandListsFn)(
		ID3D12CommandQueue * _this, UINT NumCommandLists, ID3D12CommandList* const* ppCommandLists);
        extern ExecuteCommandListsFn oExecuteCommandListsD3D12;

        typedef HRESULT(STDMETHODCALLTYPE* ResizeBuffersFn)(
                IDXGISwapChain3* pSwapChain,
                UINT BufferCount,
                UINT Width,
                UINT Height,
                DXGI_FORMAT NewFormat,
                UINT SwapChainFlags);
        extern ResizeBuffersFn oResizeBuffersD3D12;

        extern HRESULT STDMETHODCALLTYPE hookResizeBuffersD3D12(
                IDXGISwapChain3* pSwapChain,
                UINT BufferCount,
                UINT Width,
                UINT Height,
                DXGI_FORMAT NewFormat,
                UINT SwapChainFlags);

        extern long __fastcall hookPresentD3D12(IDXGISwapChain3* pSwapChain, UINT SyncInterval, UINT Flags);
        extern long __fastcall hookPresent1D3D12(IDXGISwapChain3* pSwapChain, UINT SyncInterval, UINT Flags, const DXGI_PRESENT_PARAMETERS* pParams);
        extern void STDMETHODCALLTYPE hookExecuteCommandListsD3D12(
                ID3D12CommandQueue* _this,
                UINT                          NumCommandLists,
                ID3D12CommandList* const* ppCommandLists);

        extern void release();
        bool IsInitialized();
}

// DX12 is the only supported rendering backend

namespace menu {
        extern std::atomic<bool> isOpen;
        extern void Init();
}
