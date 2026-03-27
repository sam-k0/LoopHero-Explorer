#include "ImGuiManager.h"
#include "imgui/imgui.h"
#include "imgui/imgui_impl_dx11.h"
#include "imgui/imgui_impl_win32.h"
#include <dxgi.h>
#include <d3d11.h>

extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// Custom WndProc to forward messages to ImGui
LRESULT CALLBACK ImGuiWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    if (ImGui_ImplWin32_WndProcHandler(hWnd, uMsg, wParam, lParam))
        return true; // ImGui handled it

    // Get the original procedure from ImGuiManager
    ImGuiManager& mgr = ImGuiManager::Get();
    return CallWindowProc(mgr.GetWndProc(), hWnd, uMsg, wParam, lParam);
}

bool ImGuiManager::Initialize(IDXGISwapChain* pSwapChain)
{
    if (m_bInitialized)
        return true;

    if (FAILED(pSwapChain->GetDevice(__uuidof(ID3D11Device), (void**)&m_pDevice)))
        return false;

    m_pDevice->GetImmediateContext(&m_pContext);

    DXGI_SWAP_CHAIN_DESC desc;
    pSwapChain->GetDesc(&desc);
    m_hWnd = desc.OutputWindow;

    // Hook WndProc before initializing ImGui
    m_pOriginalWndProc = (WNDPROC)SetWindowLongPtr(m_hWnd, GWLP_WNDPROC, (LONG_PTR)ImGuiWndProc);

    // Initialize ImGui context
    ImGui::CreateContext();
    ImGui_ImplWin32_Init(m_hWnd);
    ImGui_ImplDX11_Init(m_pDevice, m_pContext);

    // Create render target view
    ID3D11Texture2D* pBackBuffer = nullptr;
    pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)&pBackBuffer);

    if (pBackBuffer)
    {
        m_pDevice->CreateRenderTargetView(pBackBuffer, NULL, &m_pRTV);
        pBackBuffer->Release();
    }

    m_bInitialized = true;
    return true;
}

void ImGuiManager::Shutdown()
{
    if (m_pRTV)
    {
        m_pRTV->Release();
        m_pRTV = nullptr;
    }

    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    if (m_pContext)
    {
        m_pContext->Release();
        m_pContext = nullptr;
    }

    if (m_pDevice)
    {
        m_pDevice->Release();
        m_pDevice = nullptr;
    }

    // Restore original WndProc
    if (m_hWnd && m_pOriginalWndProc)
    {
        SetWindowLongPtr(m_hWnd, GWLP_WNDPROC, (LONG_PTR)m_pOriginalWndProc);
    }

    m_bInitialized = false;
}

ImGuiManager::~ImGuiManager()
{
    Shutdown();
}

void ImGuiManager::NewFrame()
{
    if (!m_bInitialized)
        return;

    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
}

void ImGuiManager::Render()
{
    if (!m_bInitialized)
        return;

    ImGui::Render();
    m_pContext->OMSetRenderTargets(1, &m_pRTV, NULL);
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
}
