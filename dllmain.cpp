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
bool g_showObjectExplorer = false;
bool g_recordCreateEvents = true;
bool g_showGlobals = false;
bool g_showDebugOverlay = false;
bool g_showFpsPlot = true;
bool g_showRunCmd = false;
bool g_filterShowParsedOnly = true;
bool g_logUncommonEvents = false;
bool g_showNearestObject = false;
bool g_doFilterEventLogging = false;
bool g_showEventLogFilterDlg = false;
// other
std::map<int,InstanceInfo> g_InstanceInfos;
std::vector<VarInfo> g_GlobalVarInfo;
std::deque<std::string> g_commandHistory;
// Run command
char g_commandBuffer[256] = "";
// Filtering global vars
char g_globalVarNameFilter[256] = ""; 
char g_globalVarValueFilter[256] = "";
// Filtering instance and instance vars
char g_instanceNameFilter[256] = "";
char g_instanceVarNameFilter[256] = "";
char g_instanceVarValueFilter[256] = "";
// Event filter
char g_eventLogFilter[256] = "";
// Fps measurement
LARGE_INTEGER freq, last;
float fpsHistory[FPSBUFSIZE] = {};
int fpsOffset = 0;


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
            ImGui::SameLine();
            if (ImGui::Button("Object explorer"))
            {
                Misc::Print("Show object explorer");
                g_showObjectExplorer = !g_showObjectExplorer;
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
            ImGui::SameLine();
            if (ImGui::Button("Dump Globals"))
            {
                FetchInstanceVarsDumpFile(INSTANCE_GLOBAL, false,true, "dump_globals.txt");
            }
            if (ImGui::Button("Dump create events"))
            {
                Misc::Print("Dumping to file!");
                Misc::MapToFileA(&g_createEvents);
            }
            ImGui::SameLine();
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

        if (ImGui::CollapsingHeader("Logging"))
        {
            if (ImGui::Button("Filter & Log Events"))
            {
                g_showEventLogFilterDlg = !g_showEventLogFilterDlg;
            }
            ImGui::SameLine();
            ImGui::Text((g_showEventLogFilterDlg == true) ? "Shown" : "Hidden");

            ImGui::Text("Logs events but Step & Draw");
            ImGui::SameLine();
            if (ImGui::Button("Log Events"))
            {
                g_logUncommonEvents = !g_logUncommonEvents;
            }
           
        }

        if (ImGui::CollapsingHeader("Other"))
        {   
            if (ImGui::Button("Toggle Debug Overlay"))
            {
                g_showDebugOverlay = !g_showDebugOverlay;
                Binds::CallBuiltin("show_debug_overlay", nullptr, nullptr, { double(g_showDebugOverlay) });
            }
            ImGui::SameLine();
            ImGui::Text((g_showDebugOverlay == true) ? "On" : "Off");
            
            if (ImGui::Button("Record Create Events"))
            {
                g_recordCreateEvents = !g_recordCreateEvents;
                Misc::Print("Recording create events: " + std::to_string(g_recordCreateEvents));
            }
            ImGui::SameLine();
            ImGui::Text((g_recordCreateEvents == true) ? "On" : "Off");

            if (ImGui::Button("Run Command"))
            {
                g_showRunCmd = !g_showRunCmd;
            }
            ImGui::SameLine();
            if (ImGui::Button("Show Fps"))
            {
                g_showFpsPlot = !g_showFpsPlot;
            }
            if (ImGui::Button("Cursor Object Info"))
            {
                g_showNearestObject = !g_showNearestObject;
            }
            if (ImGui::Button("Hide Native Cursor"))
            {
                ShowCursor(FALSE);
            }
            
           
        }

        
        ImGui::End();
#pragma endregion
       
        if (g_showObjectExplorer)
        {
            ImGui::Begin("Object explorer");
            // refresh instance enumeration
            if (ImGui::Button("Refresh"))
            {
                g_InstanceInfos.clear();

                YYRValue allObjs, i_id;

                CallBuiltin(allObjs, "instance_number", nullptr, nullptr, { INSTANCE_ALL });
                int count = (int)allObjs;
                // Loop vars
                int obj_index; // Object Index per inst
                std::string obj_name; // Object name per inst
                // Fetch all instance info except variables
                for (int inst_iter = 0; inst_iter < count; inst_iter++)
                {
                    CallBuiltin(i_id, "instance_id_get", nullptr, nullptr, { (double)inst_iter });
                    
                    obj_index = (int)Binds::CallBuiltinA("variable_instance_get", { double(i_id), "object_index" });
                    obj_name = LHObjects::GetObjectName(obj_index);

                    InstanceInfo i_info = InstanceInfo();
                    i_info.instanceid = i_id;
                    i_info.name = obj_name;
                    i_info.objectindex = obj_index;
                    i_info.variables = std::nullopt;

                    g_InstanceInfos.insert(std::make_pair(i_id, i_info));
                }
            }
            // Filtering
            if (ImGui::CollapsingHeader("Filters"))
            {
                // Filter var name
                ImGui::InputText("Filter Instance", g_instanceNameFilter, sizeof(g_instanceNameFilter));
                ImGui::SameLine();
                if (ImGui::Button("Apply##Instance")) // apply filter by deleting unmatching entries from vector
                {
                    std::string filter = g_instanceNameFilter;
                    for (auto it = g_InstanceInfos.begin(); it != g_InstanceInfos.end();)
                    {
                        if (!Misc::StringHasSubstr(it->second.name, filter))
                        {
                            it = g_InstanceInfos.erase(it); // returns next iterator
                        }
                        else
                        {
                            ++it;
                        }
                    }
                }
                // Filter variable names
                ImGui::InputText("Filter Variable", g_instanceVarNameFilter, sizeof(g_instanceVarNameFilter));
                ImGui::SameLine();
                if (ImGui::Button("Apply##Variable")) // apply filter by deleting unmatching entries from vector
                {
                    //Load all variables for all instances that arent loaded yet
                    std::string filter = g_instanceVarNameFilter;
                    for (auto it = g_InstanceInfos.begin(); it != g_InstanceInfos.end(); )
                    {
                        bool exists = true;
                        auto& info = it->second;

                        if (!info.variables) // refetch if none
                        {
                            info.variables = FetchInstanceVariablesSafe(it->first, exists);
                        }
                       
                        // If instance doesnt exist anymore, continue
                        if (!exists)
                        {
                            it = g_InstanceInfos.erase(it);
                            continue;
                        }

                        // If it exists, filter variables by filter
                        if (auto& vars = info.variables)
                        {
                            vars->erase(
                                std::remove_if(vars->begin(), vars->end(),
                                    [&](const VarInfo& var)
                                    {
                                        return !Misc::StringHasSubstr(var.name, filter);
                                    }),
                                vars->end()
                            );
                        }

                        ++it; // Increase the it for instance infos
                    }
                }

                // Filter variable values
                ImGui::InputText("Filter Value", g_instanceVarValueFilter, sizeof(g_instanceVarValueFilter));
                ImGui::SameLine();
                if (ImGui::Button("Apply##Value")) // apply filter by deleting unmatching entries from vector
                {
                    //Load all variables for all instances that arent loaded yet
                    std::string filter = g_instanceVarValueFilter;
                    for (auto it = g_InstanceInfos.begin(); it != g_InstanceInfos.end(); )
                    {
                        bool exists = true;
                        auto& info = it->second;

                        if (!info.variables) // refetch if none
                        {
                            info.variables = FetchInstanceVariablesSafe(it->first, exists);
                        }

                        // If instance doesnt exist anymore, continue
                        if (!exists)
                        {
                            it = g_InstanceInfos.erase(it);
                            continue;
                        }

                        // If it exists, filter variables by filter
                        if (auto& vars = info.variables)
                        {
                            vars->erase(
                                std::remove_if(vars->begin(), vars->end(),
                                    [&](const VarInfo& var)
                                    {
                                        return !Misc::StringHasSubstr(var.value, filter);
                                    }),
                                vars->end()
                            );
                        }

                        ++it; // Increase the it for instance infos
                    }
                }
            }
            
            //Variable display
            ImGui::BeginChild("scroll_region", ImVec2(0, 0), true);
            for (auto& instance_info : g_InstanceInfos)
            {
                std::string label = std::format("{}:{}({})",
                    std::to_string(instance_info.second.instanceid),
                    instance_info.second.name,
                    std::to_string(instance_info.second.objectindex));

                if (ImGui::CollapsingHeader(label.c_str()))
                {
                    bool exists = true;
                    if (ImGui::Button(("Refresh##" + std::to_string(instance_info.second.instanceid)).c_str()))
                    {
                        Misc::Print(std::format("Getting vars for instance {}", instance_info.second.instanceid));
                        instance_info.second.variables = FetchInstanceVariablesSafe(instance_info.first, exists);
                    }
                    if (exists)
                    {
                        if (bool(Binds::CallBuiltinA("instance_exists", { double(instance_info.first) })))
                        {
                            if (auto& vars = instance_info.second.variables)
                            {
                                for (const auto& var : *vars)
                                {
                                    if (var.value != "<unknown>" || !g_filterShowParsedOnly)
                                    {
                                        ImVec4 col = ImVec4(255, 255, 255, 255);//white default
                                        if (var.type != "number" && var.type != "string" && var.type != "bool")
                                        {
                                            col = ImVec4(255, 0, 0, 255); // red on unparsable
                                        }

                                        ImGui::TextColored(col, "%s (%s): %s",
                                            var.name.c_str(),
                                            var.type.c_str(),
                                            var.value.c_str());
                                    }
                                }
                            }
                            else
                            {
                                ImGui::TextColored({ 255,0,0,255 }, "Variable container is empty");
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
            ImGui::Begin("Cursor Object Info");

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

        if (g_showEventLogFilterDlg)
        {
            ImGui::Begin("Log Filtered Events");

            if (ImGui::Button("Toggle Logging"))
            {
                g_doFilterEventLogging = !g_doFilterEventLogging;
            }
            ImGui::SameLine();
            ImGui::Text((g_doFilterEventLogging == true) ? "On" : "Off");

            ImGui::InputText("Filter", g_eventLogFilter, sizeof(g_eventLogFilter));

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
    CCode* code = std::get<2>(codeEvent->Arguments());
   
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
            g_createEvents.insert(std::make_pair(selfInst->i_id, codeObj->i_pName));
        }
    }

    if (g_logUncommonEvents)
    {
        if (!Misc::StringHasSubstr(codeObj->i_pName, "Draw") && !Misc::StringHasSubstr(codeObj->i_pName, "Step"))
        {
            Misc::Print(std::format("{} / {} ({})", codeObj->i_pName, code->GetText(), code->i_CodeIndex));
        }
    }

    if (g_doFilterEventLogging)
    {
        if (Misc::StringHasSubstr(codeObj->i_pName, std::string(g_eventLogFilter)))
        {
            Misc::Print(std::format("Filtered Evt: {}", codeObj->i_pName));
        }
    }

    /*if (strcmp(codeObj->i_pName, "gml_Object_o_camp_statistik_Create_0") == 0)
    {
        CInstance* pInstance = (CInstance*)selfInst;
        Misc::Print(std::format("m_CreateCounter: {}", pInstance->m_CreateCounter));
        Misc::Print(std::format("m_Instflags: {}", pInstance->m_Instflags));
        Misc::Print(std::format("Unknown1: {}", pInstance->m_pUnknown1));
        Misc::Print(std::format("Unknown2: {}", pInstance->m_pUnknown2));
        Misc::Print(std::format("i_id: {}", pInstance->i_id));
        Misc::Print(std::format("i_objectindex: {}", pInstance->i_objectindex));
        Misc::Print(std::format("i_spriteindex: {}", pInstance->i_spriteindex));

        Misc::Print(std::format("i_sequencePos: {}", pInstance->i_sequencePos));
        Misc::Print(std::format("i_lastSequencePos: {}", pInstance->i_lastSequencePos));
        Misc::Print(std::format("i_sequenceDir: {}", pInstance->i_sequenceDir));

        Misc::Print(std::format("i_imageindex: {}", pInstance->i_imageindex));
        Misc::Print(std::format("i_imagespeed: {}", pInstance->i_imagespeed));
        Misc::Print(std::format("i_imagescalex: {}", pInstance->i_imagescalex));
        Misc::Print(std::format("i_imagescaley: {}", pInstance->i_imagescaley));
        Misc::Print(std::format("i_imageangle: {}", pInstance->i_imageangle));
        Misc::Print(std::format("i_imagealpha: {}", pInstance->i_imagealpha));
        Misc::Print(std::format("i_imageblend: {}", pInstance->i_imageblend));

        Misc::Print(std::format("i_x: {}", pInstance->i_x));
        Misc::Print(std::format("i_y: {}", pInstance->i_y));
        Misc::Print(std::format("i_xstart: {}", pInstance->i_xstart));
        Misc::Print(std::format("i_ystart: {}", pInstance->i_ystart));
        Misc::Print(std::format("i_xprevious: {}", pInstance->i_xprevious));
        Misc::Print(std::format("i_yprevious: {}", pInstance->i_yprevious));

        Misc::Print(std::format("i_direction: {}", pInstance->i_direction));
        Misc::Print(std::format("i_speed: {}", pInstance->i_speed));
        Misc::Print(std::format("i_friction: {}", pInstance->i_friction));
        Misc::Print(std::format("i_gravitydir: {}", pInstance->i_gravitydir));
        Misc::Print(std::format("i_gravity: {}", pInstance->i_gravity));
        Misc::Print(std::format("i_hspeed: {}", pInstance->i_hspeed));
        Misc::Print(std::format("i_vspeed: {}", pInstance->i_vspeed));

        Misc::Print(std::format(
            "i_bbox: [{}, {}, {}, {}]",
            pInstance->i_bbox[0],
            pInstance->i_bbox[1],
            pInstance->i_bbox[2],
            pInstance->i_bbox[3]
        ));

        Misc::Print(std::format(
            "i_timer: [{}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}]",
            pInstance->i_timer[0], pInstance->i_timer[1], pInstance->i_timer[2],
            pInstance->i_timer[3], pInstance->i_timer[4], pInstance->i_timer[5],
            pInstance->i_timer[6], pInstance->i_timer[7], pInstance->i_timer[8],
            pInstance->i_timer[9], pInstance->i_timer[10], pInstance->i_timer[11]
        ));

        Misc::Print(std::format("m_nLayerID: {}", pInstance->m_nLayerID));
        Misc::Print(std::format("i_maskindex: {}", pInstance->i_maskindex));
        Misc::Print(std::format("m_nMouseOver: {}", pInstance->m_nMouseOver));

        Misc::Print(std::format("i_depth: {}", pInstance->i_depth));
        Misc::Print(std::format("i_currentdepth: {}", pInstance->i_currentdepth));
        Misc::Print(std::format("i_lastImageNumber: {}", pInstance->i_lastImageNumber));

        Misc::Print(std::format("m_collisionTestNumber: {}", pInstance->m_collisionTestNumber));
    }*/

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
    HANDLE t = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)LHCore::ResolveCore, (LPVOID)pack, 0, NULL);// Wait for LHCC
    if (t != 0)CloseHandle(t); 

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

