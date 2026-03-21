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


std::vector<YYRValue> SwapCards;
CallbackAttributes_t* frameCallbackAttr;
// imgui
bool g_ImGuiInitialized = false;
ID3D11Device* g_Device = nullptr;
ID3D11DeviceContext* g_Context = nullptr;
HWND g_hWnd = nullptr;
ID3D11RenderTargetView* g_RTV = nullptr;


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

YYTKStatus FrameCallback(YYTKEventBase* pEvent, void* optArgument)
{
   
    /*if (pEvent->GetEventType() == EVT_ENDSCENE)
    {
        YYTKEndSceneEvent* pEndSceneEvent = (YYTKEndSceneEvent*)pEvent;
        auto& args = pEndSceneEvent->Arguments();

        // Get the D3D9 device (first argument in the tuple)
        IDirect3DDevice9* pD3D9 = std::get<0>(args);

        // Now you can use pD3D9!
    }
    else*/ if (pEvent->GetEventType() == EVT_PRESENT)
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

            // Your UI
            ImGui::Begin("Loop Hero Mod");
            ImGui::Text("ImGui injected successfully!");
            ImGui::End();

            ImGui::Render();
            g_Context->OMSetRenderTargets(1, &g_RTV, NULL);
            ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
        }
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
    
    if (Misc::StringHasSubstr(codeObj->i_pName, "gml_Room_rm_game_Create"))
    {
        SwapCards.clear();
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
        PmCreateCallback(pluginAttributes, frameCallbackAttr, FrameCallback, static_cast<EventType>(EVT_PRESENT /* | EVT_ENDSCENE*/), nullptr);
    }

    // Initialize the plugin, set callbacks inside the PluginObject.
    // Set-up buffers.
    return YYTK_OK; // Successful PluginEntry.
}

DWORD WINAPI Menu(HINSTANCE hModule)
{
    while (true)
    {
        Sleep(50);
        if (GetAsyncKeyState(VK_NUMPAD0))
        {
            Misc::Print("Dumping to file!");
            Misc::VectorToFile(&obj_create_events);
            Sleep(300);
        }
        if (GetAsyncKeyState(VK_NUMPAD1))
        {
            Misc::Print("Resetting event vector");
            obj_create_events.clear();
            Sleep(300);
        }
        if (GetAsyncKeyState(VK_NUMPAD2))
        {
            Misc::Print("Dumping sprite names with ID");
            dumpSpriteIDs();
            Sleep(300);
        }
        if (GetAsyncKeyState(VK_NUMPAD3))
        {
            Misc::Print("Dumping object names with ID");
            dumpObjectIDs();
            Sleep(300);
        }
        if (GetAsyncKeyState(VK_NUMPAD4))
        {
            Misc::Print("Turning on Debug mode");
            enableDebug();
            Sleep(300);
        }
        if (GetAsyncKeyState(VK_NUMPAD6))
        {
            Misc::Print("dumping var names");
            printAllObjects();
            Sleep(300);
        }
        if (GetAsyncKeyState(VK_NUMPAD7))
        {
            Misc::Print("obj at pos");
            getObjectAtMousePos();
            Sleep(300);
        }
    }
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
        CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)Menu, NULL, 0, NULL); // For the input
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}

