#pragma once

#include <Windows.h>
#include <d3d11.h>

// Forward declarations
struct ImGuiContext;

class ImGuiManager {
public:
    static ImGuiManager& Get() {
        static ImGuiManager instance;
        return instance;
    }

    // Initialize ImGui (called on first frame)
    bool Initialize(IDXGISwapChain* pSwapChain);

    // Shutdown ImGui
    void Shutdown();

    // Check if properly initialized
    bool IsInitialized() const { return m_bInitialized; }

    // Get graphics resources
    ID3D11Device* GetDevice() const { return m_pDevice; }
    ID3D11DeviceContext* GetContext() const { return m_pContext; }
    ID3D11RenderTargetView* GetRenderTargetView() const { return m_pRTV; }
    HWND GetWindowHandle() const { return m_hWnd; }
    WNDPROC GetWndProc() const { return m_pOriginalWndProc; }

    // New frame setup (call at start of frame)
    void NewFrame();

    // Rendering (call at end of frame)
    void Render();

    

private:
    ImGuiManager() = default;
    ~ImGuiManager();

    ImGuiManager(const ImGuiManager&) = delete;
    ImGuiManager& operator=(const ImGuiManager&) = delete;

    bool m_bInitialized = false;
    HWND m_hWnd = nullptr;
    ID3D11Device* m_pDevice = nullptr;
    ID3D11DeviceContext* m_pContext = nullptr;
    ID3D11RenderTargetView* m_pRTV = nullptr;
    WNDPROC m_pOriginalWndProc = nullptr;
};
