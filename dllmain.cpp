// dllmain.cpp : Defines the entry point for the DLL application.
#include <Windows.h>

// YYTK is in this now
#include "MyPlugin.h"
#include "Assets.h"
#include "LHSprites.h"
#include "LHObjects.h"
#include "LHCore.h"
// Plugin functionality
#include <fstream>
#include <iterator>
#include <map>
#include <iostream>
#include <format>
#include <regex>
#include <deque>

#include "VariableNames.h"
#include "Helpers.h"

#include "imgui/imgui.h"
#include "imgui/imgui_impl_dx11.h"
#include "imgui/imgui_impl_win32.h"

#define _CRT_SECURE_NO_WARNINGS
#define FPSBUFSIZE 120
#define MAXCMDHIST 5

CallbackAttributes_t* frameCallbackAttr;
// imgui
bool g_ImGuiInitialized = false;
ID3D11Device* g_Device = nullptr;
ID3D11DeviceContext* g_Context = nullptr;
HWND g_hWnd = nullptr;
ID3D11RenderTargetView* g_RTV = nullptr;
WNDPROC g_OriginalWndProc = nullptr;
extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
// imgui states
bool g_showObjects = false;
bool g_showObjectsSafe = false;
bool g_recordCreateEvents = true;
bool g_showGlobals = false;
bool g_showDebugOverlay = false;
bool g_showFpsPlot = true;
bool g_showRunCmd = false;
bool g_filterShowParsedOnly = true;
bool g_logUncommonEvents = false;
bool g_showNearestObject = false;
// other
std::vector<int> g_InstanceIds;
std::map<int, std::vector<VarInfo>> g_InstanceVarInfo;
std::vector<VarInfo> g_GlobalVarInfo;
std::deque<std::string> g_commandHistory;

char g_commandBuffer[512] = "";
char g_globalVarNameFilter[256] = "";
char g_globalVarValueFilter[256] = "";
// Fps measurement
LARGE_INTEGER freq, last;
float fpsHistory[FPSBUFSIZE] = {};
int fpsOffset = 0;

#pragma region dumper functions


#pragma endregion

// Custom WndProc to forward messages to ImGui
LRESULT CALLBACK MyWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    if (ImGui_ImplWin32_WndProcHandler(hWnd, uMsg, wParam, lParam))
        return true; // ImGui handled it

    return CallWindowProc(g_OriginalWndProc, hWnd, uMsg, wParam, lParam);
}

YYTKStatus FrameCallback(YYTKEventBase* pEvent, void* optArgument)
{
    YYTKPresentEvent* pPresentEvent = (YYTKPresentEvent*)pEvent;
    auto& args = pPresentEvent->Arguments();

    // Get the arguments (SwapChain, Sync, Flags)
    IDXGISwapChain* pSwapChain = std::get<0>(args);
    UINT Sync = std::get<1>(args);
    UINT Flags = std::get<2>(args);


    if (!g_ImGuiInitialized) // imgui not inited yet
    {
        if (SUCCEEDED(pSwapChain->GetDevice(__uuidof(ID3D11Device), (void**)&g_Device)))
        {
            g_Device->GetImmediateContext(&g_Context);

            DXGI_SWAP_CHAIN_DESC desc;
            pSwapChain->GetDesc(&desc);
            g_hWnd = desc.OutputWindow;

            // Hook WndProc before initializing ImGui
            g_OriginalWndProc = (WNDPROC)SetWindowLongPtr(g_hWnd, GWLP_WNDPROC, (LONG_PTR)MyWndProc);
                
            // 
            ImGui::CreateContext();
            ImGui_ImplWin32_Init(g_hWnd);
            ImGui_ImplDX11_Init(g_Device, g_Context);

            ID3D11Texture2D* pBackBuffer = nullptr;
            pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)&pBackBuffer);

            if (pBackBuffer)
            {
                g_Device->CreateRenderTargetView(pBackBuffer, NULL, &g_RTV);
                pBackBuffer->Release();
            }

            g_ImGuiInitialized = true;
            Misc::Print("Initialized imgui!");
                
        }
        // fps profiler init
        QueryPerformanceFrequency(&freq);
        QueryPerformanceCounter(&last);
    }

    // update fps measurement
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    double frameDelta = double(now.QuadPart - last.QuadPart) / freq.QuadPart;
    last = now;
    float fps = (frameDelta > 0.0) ? (1.0f / (float)frameDelta) : 0.0f;
    fpsHistory[fpsOffset] = fps;
    fpsOffset = (fpsOffset + 1) % FPSBUFSIZE;


    if (g_ImGuiInitialized)
    {
        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        // UI elems
        ImGui::Begin("Loop Hero Explorer");
#pragma region Top Level Buttons
        

        if (ImGui::CollapsingHeader("Variables"))
        {
            ImGui::Text(std::format("Only show parsed variables: {}", g_filterShowParsedOnly).c_str());
            ImGui::SameLine();
            if (ImGui::Button("Toggle"))
            {
                g_filterShowParsedOnly = !g_filterShowParsedOnly;
            }
            /*
            if (ImGui::Button("Object explorer"))
            {
                Misc::Print("Show object explorer");
                g_showObjects = !g_showObjects;
            }
            ImGui::SameLine();*/
            if (ImGui::Button("Safe Object explorer"))
            {
                Misc::Print("Show safe global explorer");
                g_showObjectsSafe = !g_showObjectsSafe;
            }
            if (ImGui::Button("Global Variable Explorer"))
            {
                Misc::Print("Show global explorer");
                g_showGlobals = !g_showGlobals;
            }
        }
        

// DUMPING section
        if (ImGui::CollapsingHeader("Dumping"))
        {
            if (ImGui::Button("Dump Objects"))
            {
                FetchInstanceVarsDumpFile(INSTANCE_ALL, true, true, "dump_objects.txt");
            }
            if (ImGui::Button("Dump Globals"))
            {
                FetchInstanceVarsDumpFile(INSTANCE_GLOBAL, false,true, "dump_globals.txt");
            }
            if (ImGui::Button("Dump create events"))
            {
                Misc::Print("Dumping to file!");
                Misc::MapToFileA(&g_createEvents);
            }
            if (ImGui::Button("Reset event storage"))
            {
                Misc::Print("Resetting event vector");
                g_createEvents.clear();
            }
            if (ImGui::Button("Dump sprite names with ID"))
            {
                Misc::Print("Dumping sprite names with ID");
                dumpSpriteIDs();
            }
            if (ImGui::Button("Dump object names with ID"))
            {
                Misc::Print("Dumping object names with ID");
                dumpObjectIDs();
            }
        }

        if (ImGui::CollapsingHeader("Other"))
        {
            if (ImGui::Button("Hide Native Cursor"))
            {
                ShowCursor(FALSE);
            }
            ImGui::SameLine();
            if (ImGui::Button("Toggle Debug Overlay"))
            {
                g_showDebugOverlay = !g_showDebugOverlay;
                Binds::CallBuiltin("show_debug_overlay", nullptr, nullptr, { double(g_showDebugOverlay) });
            }
            
            if (ImGui::Button("Record Create Events"))
            {
                g_recordCreateEvents = !g_recordCreateEvents;
                Misc::Print("Recording create events: " + std::to_string(g_recordCreateEvents));
            }
            if (ImGui::Button("Run Command"))
            {
                g_showRunCmd = !g_showRunCmd;
            }
            ImGui::SameLine();
            if (ImGui::Button("Show Fps"))
            {
                g_showFpsPlot = !g_showFpsPlot;
            }
            ImGui::SameLine();
            if (ImGui::Button("Log Events"))
            {
                g_logUncommonEvents = !g_logUncommonEvents;
            }
            if (ImGui::Button("Cursor Object Info"))
            {
                g_showNearestObject = !g_showNearestObject;
            }
        }

        
        ImGui::End();
#pragma endregion
       
        if (g_showObjectsSafe)
        {
            ImGui::Begin("Safe Object explorer");
            // refresh instance enumeration
            if (ImGui::Button("Refresh"))
            {
                g_InstanceIds.clear();

                YYRValue allObjs, iid, varr, len, item;

                CallBuiltin(allObjs, "instance_number", nullptr, nullptr, { INSTANCE_ALL });
                int count = (int)allObjs;

                for (int objIndex = 0; objIndex < count; objIndex++)
                {
                    CallBuiltin(iid, "instance_id_get", nullptr, nullptr, { (double)objIndex });

                    g_InstanceIds.push_back(int(iid));
                }
            }

            //Variable display
            ImGui::BeginChild("scroll_region", ImVec2(0, 0), true);
            for (const auto& iid : g_InstanceIds)
            {
                int objid = (int)Binds::CallBuiltinA("variable_instance_get", { double(iid), "object_index" });
                std::string obj_name = LHObjects::GetObjectName(objid);
                std::string label = std::format("{}:{}({})", std::to_string(iid), obj_name,objid);

                if (ImGui::CollapsingHeader(label.c_str()))
                {
                    bool exists = true;
                    if (ImGui::Button(("Refresh##" + std::to_string(iid)).c_str()))
                    {
                        Misc::Print(std::format("Getting vars for instance {}", iid));
                        g_InstanceVarInfo[iid] = FetchInstanceVariablesSafe(iid, exists);
                    }
                    if (exists)
                    {
                        if (bool(Binds::CallBuiltinA("instance_exists", { double(iid) })))
                        {
                            for (const auto& var : g_InstanceVarInfo[iid])
                            {
                                if (var.value != "<unknown>" || !g_filterShowParsedOnly)
                                {
                                    ImVec4 col = ImVec4(255, 255, 255, 255);//white default
                                    if (var.type != "number" && var.type != "string" && var.type != "bool")
                                    {
                                        col = ImVec4(255, 0, 0, 255); // red on unparsable
                                    }
                                    
                                    ImGui::TextColored(col,"%s (%s): %s",
                                        var.name.c_str(),
                                        var.type.c_str(),
                                        var.value.c_str());
                                }
                            }
                        }
                        else
                        {
                            ImGui::TextColored({ 255,0,0,255 }, "Does not exist anymore");
                        }
                    }
                    else
                    {
                        ImGui::TextColored({ 255,0,0,255 }, "Not indexed!");
                    }
                    
                }
            }

            ImGui::EndChild();
            ImGui::End();
        }
           
        if (g_showGlobals)
        {
            ImGui::Begin("Global Variables");
            
            if (ImGui::Button("Refresh"))
            {
                g_GlobalVarInfo.clear();
                bool _;
                g_GlobalVarInfo = FetchInstanceVariablesSafe(INSTANCE_GLOBAL, _);
            }
            if (ImGui::CollapsingHeader("Filters"))
            {
                // Filter var name
                ImGui::InputText("Filter Name", g_globalVarNameFilter, sizeof(g_globalVarNameFilter));
                ImGui::SameLine();
                if (ImGui::Button("Apply##Name")) // apply filter by deleting unmatching entries from vector
                {
                    std::string filter = g_globalVarNameFilter;
                    std::vector<VarInfo>::iterator it = g_GlobalVarInfo.begin();

                    while (it != g_GlobalVarInfo.end())
                    {
                        if (!Misc::StringHasSubstr(it->name, filter))
                        {
                            it = g_GlobalVarInfo.erase(it);
                        }
                        else ++it;
                    }
                }
                // Filter val
                ImGui::InputText("Filter Value", g_globalVarValueFilter, sizeof(g_globalVarValueFilter));
                ImGui::SameLine();
                if (ImGui::Button("Apply##Value")) // apply filter by deleting unmatching entries from vector
                {
                    std::string filter = g_globalVarValueFilter;
                    std::vector<VarInfo>::iterator it = g_GlobalVarInfo.begin();

                    while (it != g_GlobalVarInfo.end())
                    {
                        if (!Misc::StringHasSubstr(it->value, filter))
                        {
                            it = g_GlobalVarInfo.erase(it);
                        }
                        else ++it;
                    }
                }
            }
            //Variable display
            ImGui::BeginChild("scroll_region", ImVec2(0, 0), true);
          
            for (const auto& var : g_GlobalVarInfo)
            {
                if (var.value == "<unset>")
                {
                    ImGui::TextColored({ 255.0, 0., 0., 255.0 }, std::format("{} ({}): {}", var.name, var.type, var.value).c_str());
                }
                else if (var.value != "<unknown>" || !g_filterShowParsedOnly)
                {

                    ImGui::Text("%s (%s): %s",
                        var.name.c_str(),
                        var.type.c_str(),
                        var.value.c_str());
                }

            }
            
            ImGui::EndChild();
            ImGui::End();
        }

        if (g_showFpsPlot)
        {
            float minFPS = FLT_MAX;
            float maxFPS = 0.0f;

            for (int i = 0; i < FPSBUFSIZE; i++)
            {
                float v = fpsHistory[i];

                if (v > 0.0f) // ignore uninitialized values
                {
                    if (v < minFPS) minFPS = v;
                    if (v > maxFPS) maxFPS = v;
                }
            }

            ImGui::Begin("Fps Monitor");
            ImGui::PlotLines(
                "FPS",
                fpsHistory,
                120,
                fpsOffset,          // offset for circular buffer
                nullptr,            // overlay text
                0.0f,               // min
                144.0f,             // max (adjust to your monitor)
                ImVec2(0, 80)       // size
            );
            ImGui::Text(("Fps: "+std::to_string(fps)).c_str());
            ImGui::SameLine();
            ImGui::Text(("ft(ms): " + std::to_string(frameDelta * 1000.0f)).c_str());
            ImGui::Text(("High: " + std::to_string(maxFPS)).c_str());
            ImGui::SameLine();
            ImGui::Text(("Low: " + std::to_string(minFPS)).c_str());

            ImGui::End();
        }
        
        if (g_showRunCmd)
        {
            ImGui::Begin("Run Command");

            ImGui::InputText("cmd", g_commandBuffer, sizeof(g_commandBuffer));

            if (ImGui::Button("Run"))
            {
                std::string cmd = g_commandBuffer;

                if (RunCommand(cmd))
                {
                    // Avoid duplicate of last command
                    if (g_commandHistory.empty() || g_commandHistory.back() != cmd)
                    {
                        g_commandHistory.push_back(cmd);

                        if (g_commandHistory.size() > MAXCMDHIST)
                            g_commandHistory.pop_front();
                    }
                }
            }

            ImGui::Separator();
            ImGui::Text("History:");

            for (int i = (int)g_commandHistory.size() - 1; i >= 0; --i)
            {
                const std::string& cmd = g_commandHistory[i];

                if (ImGui::Button(cmd.c_str()))
                {
                    // Copy into input field
                    strncpy(g_commandBuffer, cmd.c_str(), sizeof(g_commandBuffer));
                    g_commandBuffer[sizeof(g_commandBuffer) - 1] = '\0'; // safety
                }
            }

            ImGui::End();
        }

        if (g_showNearestObject)
        {
            ImGui::Begin("Cursor object info");

            YYRValue mx = Binds::CallBuiltin("device_mouse_x", nullptr, nullptr, { 0.0 });
            YYRValue my = Binds::CallBuiltin("device_mouse_y", nullptr, nullptr, { 0.0 });
            YYRValue obj = Binds::CallBuiltin("instance_nearest", nullptr, nullptr, { mx, my, INSTANCE_ALL });
            YYRValue oi = Binds::CallBuiltinA("variable_instance_get", { obj, "object_index" });
            
            double objIndex = static_cast<double>(oi);
            double instid = static_cast<double>(obj);
            ImGui::Text("ObjectName: %s", LHObjects::GetObjectName(objIndex));
            ImGui::Text("ObjectIndex: %d", objIndex);
            ImGui::Text("InstanceID: %d", instid);
            ImGui::End();
        }

        ImGui::Render();
        g_Context->OMSetRenderTargets(1, &g_RTV, NULL);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
    }

    return YYTK_OK;
}


int ExecuteCodeCallback(YYTKCodeEvent* codeEvent, void*)
{
    CCode* codeObj = std::get<CCode*>(codeEvent->Arguments());
    CInstance* selfInst = std::get<0>(codeEvent->Arguments());
    CInstance* otherInst = std::get<1>(codeEvent->Arguments());

    // If we have invalid data???
    if (!codeObj)
        return YYTK_INVALIDARG;

    if (!codeObj->i_pName)
        return YYTK_INVALIDARG;
    
    // associate create events with instances
    if (g_recordCreateEvents)
    {
        if (Misc::StringHasSubstr(codeObj->i_pName, "Create") && Misc::StringHasSubstr(codeObj->i_pName, "gml_Object"))
        {
            g_createEvents.insert(std::make_pair(selfInst->i_spriteindex, codeObj->i_pName));
        }
    }

    if (g_logUncommonEvents)
    {
        if (!Misc::StringHasSubstr(codeObj->i_pName, "Draw") && !Misc::StringHasSubstr(codeObj->i_pName, "Step"))
        {
            Misc::Print(codeObj->i_pName);
        }
    }
    

    return YYTK_OK;
}


// Entry
void InstallPatches()
{
    if (LHCore::pInstallPrePatch != nullptr)
    {
        LHCore::pInstallPrePatch(ExecuteCodeCallback);
        Misc::Print("Installed patch method(s)", CLR_GREEN);
    }
}

YYTKStatus PluginUnload()
{
    LHCore::pUnregisterModule(gPluginName);
    return YYTK_OK;
}

DllExport YYTKStatus PluginEntry(
    YYTKPlugin* PluginObject // A pointer to the dedicated plugin object
)
{
    LHCore::CoreReadyPack* pack = new LHCore::CoreReadyPack(PluginObject, InstallPatches);
    gThisPlugin = PluginObject;
    PluginObject->PluginUnload = PluginUnload;
    CloseHandle(CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)LHCore::ResolveCore, (LPVOID)pack, 0, NULL)); // Wait for LHCC

    PluginAttributes_t* pluginAttributes = nullptr;
    if (PmGetPluginAttributes(gThisPlugin, pluginAttributes) == YYTK_OK)
    {
        PmCreateCallback(pluginAttributes, frameCallbackAttr, FrameCallback, static_cast<EventType>(EVT_PRESENT), nullptr);
    }
    return YYTK_OK; // Successful PluginEntry.
}



BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
                     )
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
        DllHandle = hModule;
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}

