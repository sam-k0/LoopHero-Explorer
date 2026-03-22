// dllmain.cpp : Defines the entry point for the DLL application.
#include <Windows.h>

// YYTK is in this now
#include "MyHelper.h"
#include "Assets.h"
#include "LHSprites.h"
#include "LHObjects.h"
// Plugin functionality
#include <fstream>
#include <iterator>
#include <map>
#include <iostream>

#include "imgui/imgui.h"
#include "imgui/imgui_impl_dx11.h"
#include "imgui/imgui_impl_win32.h"

#define _CRT_SECURE_NO_WARNINGS


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
bool g_recordCreateEvents = false;

// other
std::map<int, std::vector<std::string>> g_InstanceVars;
bool g_ShouldRefresh = false;

void mapToFile(std::map<int, std::string> arg, std::string fname, std::string enumName)
{
    std::ofstream outFile(fname);

    if (outFile.is_open()) {
        outFile << "#pragma once\n\n";
        outFile << "enum "<< enumName <<" {\n";

        for (const auto& entry : arg) {
            outFile << "    " << (entry.second) << " = " <<(entry.first) << ",\n";
        }

        outFile << "};\n";

        outFile.close();
        std::cout << fname <<" file generated successfully.\n";
    }
    else {
        std::cerr << "Unable to open file for writing.\n";
    }
}

void dumpObjectIDs()
{
    std::map<int, std::string> objMap;

    while (true)
    {
        YYRValue doesExist = Misc::CallBuiltin("object_exists", nullptr, nullptr, { static_cast<double>(objMap.size()) });
        Misc::Print(" Does exist: " + std::to_string(doesExist.As<double>()));
        if (static_cast<double>(doesExist) == 0.0)
        {
            Misc::Print("Done!");
            break;
        }
        YYRValue name = Misc::CallBuiltin("object_get_name", nullptr, nullptr, { static_cast<double>(objMap.size()) });
        // cast to string
        std::string namestr = static_cast<const char*>(name);
        Misc::Print(namestr);
        // add to map
        objMap.insert(std::pair<int, std::string>(objMap.size(), namestr));
    }

    mapToFile(objMap, "objEnum.txt", "LHObjectEnum");
}

void dumpSpriteIDs()
{
    // sprite ID ; sprite name
    std::map<int, std::string> spriteMap;
    // Loop through all sprites
    while(true)
    {
        YYRValue doesExist = Misc::CallBuiltin("sprite_exists", nullptr, nullptr, { static_cast<double>(spriteMap.size()) });
        Misc::Print(" Does exist: " + std::to_string(doesExist.As<double>()));
        if (static_cast<double>(doesExist) == 0.0)
        {
            Misc::Print("Done!");
            break;
        }
        YYRValue spriteName = Misc::CallBuiltin("sprite_get_name", nullptr, nullptr, { static_cast<double>(spriteMap.size()) });
        // cast to string
        std::string spriteNameStr = static_cast<const char*>(spriteName);
        Misc::Print(spriteNameStr);
        // add to map
        spriteMap.insert(std::pair<int, std::string>(spriteMap.size(), spriteNameStr));
    }

    Misc::Print("Map has " + std::to_string(spriteMap.size()));

    std::ofstream outFile("SpriteEnum.txt");

    if (outFile.is_open()) {
        outFile << "#pragma once\n\n";
        outFile << "enum SpriteEnum {\n";

        for (const auto& entry : spriteMap) {
            outFile << "    " << entry.second << " = " << entry.first << ",\n";
        }

        outFile << "};\n";

        outFile.close();
        std::cout << "SpriteEnum.txt file generated successfully.\n";
    }
    else {
        std::cerr << "Unable to open file for writing.\n";
    }

}

void enableDebug()
{
    Misc::CallBuiltin("show_debug_overlay", nullptr, nullptr, {1.0});
}

void arrayShit(int iid, std::string arrname)
{
    YYRValue type;
    YYRValue arr = Misc::CallBuiltin("variable_instance_get", nullptr, nullptr, { double(iid), arrname });
    CallBuiltin(type, "typeof", nullptr, nullptr, { arr });
    std::string typestr = std::string(static_cast<const char*>(type));
    if (typestr == "array")
    {
        Misc::PrintArray(arr);
    }
   
    
}

void dumpVars()
{
    Misc::Print("Dumping shit:");
    YYRValue vars;
    //Misc::GetObjectInstanceVariables(vars, LHObjectEnum::o_camp_build_button);
    //Misc::PrintArray(vars, Color::CLR_BRIGHTPURPLE);

    YYRValue obj;
    Misc::GetFirstOfObject(obj, LHObjectEnum::o_camp_build_button);
    // show text
    YYRValue text;
    CallBuiltin(text, "variable_instance_get", nullptr, nullptr, { obj, "text" });

    Misc::Print(static_cast<const char*>(text), Color::CLR_GOLD);

    CallBuiltin(text, "variable_instance_set", nullptr, nullptr, { obj, "text", "ayo lmao" });
}

void printAllObjects()
{
    YYRValue allObjs;
    YYRValue iid;
    YYRValue arr;
    CallBuiltin(allObjs, "instance_number", nullptr, nullptr, { INSTANCE_ALL });
    // ic = instance_count(all)
    // instance_id_get(ic-1)
    for (int i = 0; i < ((int)allObjs) - 1; i++)
    {
        CallBuiltin(iid, "instance_id_get", nullptr, nullptr, { (double)i });
        Misc::GetInstanceVariables(arr, iid);
        Misc::Print("______________");
        Misc::PrintArray(arr);
    }
    
}


void getObjectAtMousePos()
{
    YYRValue mx = Misc::CallBuiltin("device_mouse_x", nullptr, nullptr, { 0.0 });
    YYRValue my = Misc::CallBuiltin("device_mouse_y", nullptr, nullptr, { 0.0 });
    
    YYRValue obj = Misc::CallBuiltin("instance_nearest", nullptr, nullptr, {mx, my, INSTANCE_ALL });

    YYRValue arr;
    Misc::GetInstanceVariables(arr, obj);
    Misc::Print("______________" + std::to_string((int)obj), Color::CLR_AQUA);
    Misc::PrintArrayInstanceVariables(arr,obj, Color::CLR_AQUA);
}

// Unload
YYTKStatus PluginUnload()
{
    PmRemoveCallback(callbackAttr);

    return YYTK_OK;
}

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


    if (!g_ImGuiInitialized)
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
    }
    if (g_ImGuiInitialized)
    {
        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        // UI elems
        ImGui::Begin("Loop Hero Explorer");
        ImGui::Text("ImGui injected successfully!");
        if (ImGui::Button("Object explorer"))
        {
            // This runs when the button is clicked
            Misc::Print("Show object explorer");
            g_showObjects = !g_showObjects;
            //ShowCursor(FALSE); 
        }
        if (ImGui::Button("Record create events"))
        {
            Misc::Print("Recording create events: "+std::to_string(g_recordCreateEvents));
        }
        if (ImGui::Button("Dump create events"))
        {
            Misc::Print("Dumping to file!");
            Misc::MapToFile(&g_createEvents);           
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
        if (ImGui::Button("Debug Mode"))
        {
            Misc::Print("Turning on Debug mode");
            enableDebug();
        }
        if (ImGui::Button("Dump obj var names"))
        {
            Misc::Print("dumping var names");
            printAllObjects();
        }
        if (ImGui::Button("Obj at cursor pos"))
        {
            Misc::Print("obj at pos");
            getObjectAtMousePos();
        }
        ImGui::End();

        if (g_showObjects)
        {
            ImGui::Begin("Object explorer");
            if (ImGui::Button("Refresh"))
            {
                g_ShouldRefresh = true;
            }

            ImGui::BeginChild("scroll_region", ImVec2(0, 0), true);

            YYRValue allObjs, iid, varr, len, item;

            // Refresh instance var cache
            if (g_ShouldRefresh)
            {
                g_InstanceVars.clear();

                YYRValue allObjs, iid, varr, len, item;

                CallBuiltin(allObjs, "instance_number", nullptr, nullptr, { INSTANCE_ALL });
                int count = (int)allObjs;

                for (int objIndex = 0; objIndex < count; objIndex++)
                {
                    CallBuiltin(iid, "instance_id_get", nullptr, nullptr, { (double)objIndex });

                    Misc::GetInstanceVariables(varr, iid);

                    CallBuiltin(len, "array_length_1d", nullptr, nullptr, { varr });
                    int varCount = (int)len;

                    std::vector<std::string> vars;

                    for (int varIndex = 0; varIndex < varCount; varIndex++)
                    {
                        CallBuiltin(item, "array_get", nullptr, nullptr, { varr, (double)varIndex });

                        const char* varName = static_cast<const char*>(item);
                        vars.push_back(varName);
                    }

                    g_InstanceVars[(int)iid] = vars;
                }

                g_ShouldRefresh = false;
            }
            // render cache
            for (const auto& [iid, vars] : g_InstanceVars)
            {
                std::string label = "Instance " + std::to_string(iid);

                if (ImGui::CollapsingHeader(label.c_str()))
                {
                    for (const auto& var : vars)
                    {
                        ImGui::Text("%s", var.c_str());
                    }
                }
            }

            ImGui::EndChild();
            ImGui::End();
        }
           

        ImGui::Render();
        g_Context->OMSetRenderTargets(1, &g_RTV, NULL);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
    }

    return YYTK_OK;
}


YYTKStatus ExecuteCodeCallback(YYTKCodeEvent* codeEvent, void*)
{
    CCode* codeObj = std::get<CCode*>(codeEvent->Arguments());
    CInstance* selfInst = std::get<0>(codeEvent->Arguments());
    CInstance* otherInst = std::get<1>(codeEvent->Arguments());

    // If we have invalid data???
    if (!codeObj)
        return YYTK_INVALIDARG;

    if (!codeObj->i_pName)
        return YYTK_INVALIDARG;
    
    if (g_recordCreateEvents)
    {
        if (Misc::StringHasSubstr(codeObj->i_pName, "Create") && Misc::StringHasSubstr(codeObj->i_pName, "gml_Object"))
        {
            g_createEvents.insert(std::make_pair(selfInst->i_spriteindex, codeObj->i_pName));
        }
    }
    
    /*
    if (Misc::StringHasSubstr(codeObj->i_pName, "o_menu_Draw_0"))
    {
      
    }*/

  
    /*if (Misc::StringHasSubstr(codeObj->i_pName, "gml_Object") && (Misc::StringHasSubstr(codeObj->i_pName, "create") || Misc::StringHasSubstr(codeObj->i_pName, "Create")))
    {
        
        PrintMessage(CLR_DEFAULT, "%s SpriteID: %d", codeObj->i_pName, selfInst->i_spriteindex);
 
    }*/
    


    return YYTK_OK;
}


// Entry
DllExport YYTKStatus PluginEntry(
    YYTKPlugin* PluginObject // A pointer to the dedicated plugin object
)
{
    Misc::Print("Dumper is loaded!", CLR_RED);
    gThisPlugin = PluginObject;
    gThisPlugin->PluginUnload = PluginUnload;

    PluginAttributes_t* pluginAttributes = nullptr;
    if (PmGetPluginAttributes(gThisPlugin, pluginAttributes) == YYTK_OK)
    {
        PmCreateCallback(pluginAttributes, callbackAttr, reinterpret_cast<FNEventHandler>(ExecuteCodeCallback), EVT_CODE_EXECUTE, nullptr);
        PmCreateCallback(pluginAttributes, frameCallbackAttr, FrameCallback, static_cast<EventType>(EVT_PRESENT), nullptr);
    }

    // Initialize the plugin, set callbacks inside the PluginObject.
    // Set-up buffers.
    return YYTK_OK; // Successful PluginEntry.
}
/*
DWORD WINAPI Menu(HINSTANCE hModule)
{
    while (true)
    {
        Sleep(50);
        if (GetAsyncKeyState(VK_NUMPAD0))
        {
            Misc::Print("Dumping to file!");
            Misc::VectorToFile(&obj_create_events);
            
        }
        if (GetAsyncKeyState(VK_NUMPAD1))
        {
            Misc::Print("Resetting event vector");
            obj_create_events.clear();
            
        }
        if (GetAsyncKeyState(VK_NUMPAD2))
        {
            Misc::Print("Dumping sprite names with ID");
            dumpSpriteIDs();
            
        }
        if (GetAsyncKeyState(VK_NUMPAD3))
        {
            Misc::Print("Dumping object names with ID");
            dumpObjectIDs();
            
        }
        if (GetAsyncKeyState(VK_NUMPAD4))
        {
            Misc::Print("Turning on Debug mode");
            enableDebug();
            
        }
        if (GetAsyncKeyState(VK_NUMPAD6))
        {
            Misc::Print("dumping var names");
            printAllObjects();
            
        }
        if (GetAsyncKeyState(VK_NUMPAD7))
        {
            Misc::Print("obj at pos");
            getObjectAtMousePos();
            
        }
    }
}
*/
BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
                     )
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
        DllHandle = hModule;
        //CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)Menu, NULL, 0, NULL); // For the input
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}

