
#include "PluginCore.h"
#include "ImGuiManager.h"
#include "UIWindows.h"
#include "MyPlugin.h"
#include "LHCore.h"
#include <Windows.h>
#include <chrono>

YYTKPlugin* PluginCore::m_pPlugin = nullptr;
CallbackAttributes_t* g_frameCallbackAttr = nullptr;

// Forward declare since ExecuteCodeCallback is used by InstallPatches
int PluginCore::OnCodeExecute(YYTKCodeEvent* codeEvent, void*)
{
    CCode* codeObj = std::get<CCode*>(codeEvent->Arguments());
    CInstance* selfInst = std::get<0>(codeEvent->Arguments());
    CInstance* otherInst = std::get<1>(codeEvent->Arguments());

    // If we have invalid data
    if (!codeObj)
        return YYTK_INVALIDARG;

    if (!codeObj->i_pName)
        return YYTK_INVALIDARG;

    // Associate create events with instances
    if (UIWindows::ShouldRecordCreateEvents())
    {
        if (Misc::StringHasSubstr(codeObj->i_pName, "Create") && Misc::StringHasSubstr(codeObj->i_pName, "gml_Object"))
        {
            g_createEvents.insert(std::make_pair(selfInst->i_spriteindex, codeObj->i_pName));
        }
    }

    if (UIWindows::ShouldLogUncommonEvents())
    {
        if (!Misc::StringHasSubstr(codeObj->i_pName, "Draw") && !Misc::StringHasSubstr(codeObj->i_pName, "Step"))
        {
            Misc::Print(codeObj->i_pName);
        }
    }

    return YYTK_OK;
}

void PluginCore::InstallPatches()
{
    if (LHCore::pInstallPrePatch != nullptr)
    {
        LHCore::pInstallPrePatch(OnCodeExecute);
        Misc::Print("Installed patch method(s)", CLR_GREEN);
    }
}

YYTKStatus PluginCore::OnFrameRender(YYTKEventBase* pEvent, void* optArgument)
{
    YYTKPresentEvent* pPresentEvent = (YYTKPresentEvent*)pEvent;
    auto& args = pPresentEvent->Arguments();

    // Get the arguments (SwapChain, Sync, Flags)
    IDXGISwapChain* pSwapChain = std::get<0>(args);

    // Initialize ImGui on first frame
    if (!ImGuiManager::Get().IsInitialized())
    {
        if (ImGuiManager::Get().Initialize(pSwapChain))
        {
            Misc::Print("Initialized ImGui!");
            UIWindows::Initialize();
        }

        // Initialize FPS profiler
        static LARGE_INTEGER freq, last;
        QueryPerformanceFrequency(&freq);
        QueryPerformanceCounter(&last);
    }

    // Update FPS measurement
    static LARGE_INTEGER freq, last;
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    if (freq.QuadPart == 0) QueryPerformanceFrequency(&freq);
    
    double frameDelta = (freq.QuadPart > 0) ? double(now.QuadPart - last.QuadPart) / freq.QuadPart : 0.0;
    last = now;
    float fps = (frameDelta > 0.0) ? (1.0f / (float)frameDelta) : 0.0f;

    UIWindows::UpdateFrameMetrics(fps, frameDelta);

    // Render ImGui
    if (ImGuiManager::Get().IsInitialized())
    {
        ImGuiManager::Get().NewFrame();
        UIWindows::Render();
        ImGuiManager::Get().Render();
    }

    return YYTK_OK;
}

YYTKStatus PluginCore::Unload()
{
    if (m_pPlugin)
    {
        LHCore::pUnregisterModule(gPluginName);
    }
    return YYTK_OK;
}

YYTKStatus PluginCore::Initialize(YYTKPlugin* pluginObject)
{
    m_pPlugin = pluginObject;
    gThisPlugin = pluginObject;
    pluginObject->PluginUnload = Unload;

    // Create thread to resolve core
    LHCore::CoreReadyPack* pack = new LHCore::CoreReadyPack(pluginObject, InstallPatches);
    CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)LHCore::ResolveCore, (LPVOID)pack, 0, NULL);

    // Register frame callback
    PluginAttributes_t* pluginAttributes = nullptr;
    if (PmGetPluginAttributes(pluginObject, pluginAttributes) == YYTK_OK)
    {
        PmCreateCallback(pluginAttributes, g_frameCallbackAttr, OnFrameRender, static_cast<EventType>(EVT_PRESENT), nullptr);
    }

    return YYTK_OK;
}
