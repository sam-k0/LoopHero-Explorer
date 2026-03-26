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

#include "ObjectMaps.h"
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
// other
std::vector<int> g_InstanceIds;
std::map<int, std::vector<VarInfo>> g_InstanceVarInfo;
std::vector<VarInfo> g_GlobalVarInfo;
std::deque<std::string> g_commandHistory;

char g_commandBuffer[512] = "";
char g_globalVarNameFilter[256] = "";
// Fps measurement
LARGE_INTEGER freq, last;
float fpsHistory[FPSBUFSIZE] = {};
int fpsOffset = 0;

#pragma region dumper functions

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
        YYRValue doesExist = Binds::CallBuiltin("object_exists", nullptr, nullptr, { static_cast<double>(objMap.size()) });
        Misc::Print(" Does exist: " + std::to_string(doesExist.As<double>()));
        if (static_cast<double>(doesExist) == 0.0)
        {
            Misc::Print("Done!");
            break;
        }
        YYRValue name = Binds::CallBuiltin("object_get_name", nullptr, nullptr, { static_cast<double>(objMap.size()) });
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
        YYRValue doesExist = Binds::CallBuiltin("sprite_exists", nullptr, nullptr, { static_cast<double>(spriteMap.size()) });
        Misc::Print(" Does exist: " + std::to_string(doesExist.As<double>()));
        if (static_cast<double>(doesExist) == 0.0)
        {
            Misc::Print("Done!");
            break;
        }
        YYRValue spriteName = Binds::CallBuiltin("sprite_get_name", nullptr, nullptr, { static_cast<double>(spriteMap.size()) });
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

void toggleDebug()
{
    Binds::CallBuiltin("show_debug_overlay", nullptr, nullptr, {double(g_showDebugOverlay)});
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
        Binds::GetInstanceVariables(arr, iid);
        Misc::Print("______________");
        Misc::PrintArray(arr);
    }
    
}

void getObjectAtMousePos()
{
    YYRValue mx = Binds::CallBuiltin("device_mouse_x", nullptr, nullptr, { 0.0 });
    YYRValue my = Binds::CallBuiltin("device_mouse_y", nullptr, nullptr, { 0.0 });
    
    YYRValue obj = Binds::CallBuiltin("instance_nearest", nullptr, nullptr, {mx, my, INSTANCE_ALL });

    YYRValue arr;
    Binds::GetInstanceVariables(arr, obj);
    Misc::Print("______________" + std::to_string((int)obj), Color::CLR_AQUA);
    Misc::PrintArrayInstanceVariables(arr,obj);

    YYRValue oi = Binds::CallBuiltinA("variable_instance_get", { obj, "object_index" });
    Misc::Print(DCS(oi));

}

std::vector<VarInfo> FetchInstanceVariables(double inst)
{
    std::vector<VarInfo> result;

    YYRValue varr = Binds::CallBuiltinA("variable_instance_get_names", { inst });    
    Misc::Print(varr.As<double>());
    YYRValue len = Binds::CallBuiltinA("array_length_1d", { varr });

    int count = len.As<double>();

    for (int i = 0; i < count; i++)
    {
        YYRValue item, content, type;

        CallBuiltin(item, "array_get", nullptr, nullptr, { varr, (double)i });
        std::string varName = DCS(item);

        CallBuiltin(content, "variable_instance_get", nullptr, nullptr, { inst, varName.c_str() });
        CallBuiltin(type, "typeof", nullptr, nullptr, { content });

        std::string typeStr = DCS(type);
        std::string valueStr;

        if (typeStr == "number")
            valueStr = std::to_string(double(content));
        else if (typeStr == "bool")
            valueStr = bool(content) ? "true" : "false";
        else if (typeStr == "string")
            valueStr = DCS(content);
        else
            valueStr = "<unknown>";

        result.push_back({ varName, typeStr, valueStr });
    }

    return result; // fully safe snapshot
}

// exists will be false if not yet saved
std::vector<VarInfo> FetchInstanceVariablesSafe(double inst, bool &exists)
{
    std::vector<VarInfo> result;
    int oid = INSTANCE_GLOBAL; // global inst doesnt have object index, so use as default
    // Try to find inst in map
    if (inst != INSTANCE_GLOBAL)
    {
         oid = Binds::CallBuiltinA("variable_instance_get", { inst, "object_index" });

        if (!g_ObjectVarNames.contains((int)oid))
        {
            Misc::Print(std::format("Instance id {} ({}) not found in map!", inst, oid), CLR_RED);
            exists = false;
            return result;
        }
    }
    
    exists = true;
    for (const auto &it : g_ObjectVarNames.at((int)oid))
    {
        YYRValue item, content, type, isset;
        
        // Check if the variable exists already (may be inited later, would be violation)
        if (inst != INSTANCE_GLOBAL)
        {
            CallBuiltin(isset, "variable_instance_exists", nullptr, nullptr, { inst, it });
        }
        else // use this for globals, other doesnt work for some reason
        {
            CallBuiltin(isset, "variable_global_exists", nullptr, nullptr, { it });
        }
        
        if (bool(isset)) // exists
        {
            CallBuiltin(content, "variable_instance_get", nullptr, nullptr, { inst,it });
            CallBuiltin(type, "typeof", nullptr, nullptr, { content });

            std::string typeStr = DCS(type);
            std::string valueStr;

            if (typeStr == "number")
                valueStr = std::to_string(double(content));
            else if (typeStr == "bool")
                valueStr = bool(content) ? "true" : "false";
            else if (typeStr == "string")
                valueStr = DCS(content);
            else
                valueStr = "<unknown>";

            result.push_back({ it, typeStr, valueStr });
        }
        else
        {
            result.push_back({ it, "<?type>", "<unset>" });
        }

       
    }
    return result;
}

void FetchInstanceVarsDumpFile(double index,bool isobject,bool dumpparsable, const char* filename)
{

    YYRValue allObjs, iid;
    std::vector<int> outIDs;
    if (isobject)
    {
        Misc::Print("Treating as object index, fetching all instances of type first...");
        CallBuiltin(allObjs, "instance_number", nullptr, nullptr, { index });
        int count = (int)allObjs;

        for (int i = 0; i < count; i++)
        {
            CallBuiltin(iid, "instance_id_get", nullptr, nullptr, { (double)i });
            outIDs.push_back((int)iid);
        }
    }
    else
    {
        Misc::Print("Treating as instance id, using index directly!");
        outIDs.push_back(int(index));
    }
    // map onto vars
    std::map<int, std::vector<std::string>> result;

    for (int iid : outIDs)
    {
        YYRValue inst = (double)iid;

        // get object_index
        YYRValue objIndexVal;
        CallBuiltin(objIndexVal, "variable_instance_get", nullptr, nullptr, { inst, "object_index" });
        int objIndex = (int)objIndexVal;

        // get variable names (unsafe array, so copy immediately!)
        YYRValue varr, len, item;
        Binds::GetInstanceVariables(varr, inst);

        CallBuiltin(len, "array_length_1d", nullptr, nullptr, { varr });
        int count = (int)len;

        auto& vec = result[objIndex];

        for (int i = 0; i < count; i++)
        {
            CallBuiltin(item, "array_get", nullptr, nullptr, { varr, (double)i });

            std::string varName = DCS(item); // deep copy immediately

            YYRValue type, content;
            bool isok = true;
            // check type 
            if (dumpparsable)
            {
                CallBuiltin(content, "variable_instance_get", nullptr, nullptr, { inst, varName });
                CallBuiltin(type, "typeof", nullptr, nullptr, { content });

                std::string typeStr = DCS(type);
                std::string valueStr;

                if (typeStr != "number" && typeStr != "string" && typeStr != "bool")
                {
                    isok = false;
                    Misc::Print(std::format("Skipping {} as type is unknown.", varName));
                }
                    
            }
           
            if (isok)
            {
                // avoid duplicates
                if (std::find(vec.begin(), vec.end(), varName) == vec.end())
                {
                    vec.push_back(varName);
                }
            }
        }
    }
    //write
    std::ofstream file(filename);

    for (const auto& [objIndex, vars] : result)
    {
        file << "{" << objIndex << " , {";

        for (size_t i = 0; i < vars.size(); i++)
        {
            file << "\"" << vars[i] << "\"";

            if (i < vars.size() - 1)
                file << ", ";
        }

        file << "}},\n";
    }
}

static std::vector<std::string> Tokenize(const std::string& ref)
{
    std::vector<std::string> vResults;
    size_t _beginFuncCall = ref.find_first_of('(');

    if (_beginFuncCall == std::string::npos)
    {
        return {};
    }

    size_t _endFuncCall = ref.find_first_of(')');

    if (_endFuncCall == std::string::npos)
    {
        return {};
    }

    // Function name
    vResults.push_back(ref.substr(0, _beginFuncCall));

    std::stringstream ss(ref.substr(_beginFuncCall + 1, _endFuncCall - _beginFuncCall));
    std::string sCurItem;

    while (std::getline(ss, sCurItem, ','))
    {
        sCurItem.erase(std::remove_if(sCurItem.begin(), sCurItem.end(), ::isspace), sCurItem.end());
        if (sCurItem.find_first_of(')') != std::string::npos)
        {
            auto closePos = sCurItem.find_first_of(')');

            sCurItem = sCurItem.substr(0, closePos);
        }

        if (!sCurItem.empty())
            vResults.push_back(sCurItem);
    }

    return vResults;
}

bool RunCommand(const std::string &cmd)
{
    if (cmd.empty())
        return false;

    std::string cmdcopy = cmd;

    std::regex regexAssignment(R"(global\.([a-zA-Z_]+)\s*=\s*(.*))");
    std::regex regexPeek(R"(global\.([a-zA-Z_]+))");

    if (std::regex_match(cmdcopy, regexAssignment))
    {
        cmdcopy = std::regex_replace(cmdcopy, regexAssignment, "variable_global_set(\"$1\", $2)");
    }
    else if (std::regex_match(cmdcopy, regexPeek))
    {
        cmdcopy = std::regex_replace(cmdcopy, regexPeek, "variable_global_get(\"$1\")");
    }

    std::regex validCall(R"(^[a-zA-Z_]\w*\(.*\)$)");

    if (!std::regex_match(cmdcopy, validCall))
    {
        Misc::Print("Invalid syntax");
        return false;
    }

    std::vector<std::string> tokens = Tokenize(cmdcopy);
    if (tokens.empty())
        return false;

    const std::string& funcName = tokens[0];

    // --- Prepare args ---
    std::vector<YYRValue> args(tokens.size() - 1);

    for (size_t i = 1; i < tokens.size(); i++)
    {
        const std::string& token = tokens[i];

        if (std::regex_match(token, std::regex(R"(^-?\d+(\.\d+)?$)")))
        {
            args[i - 1] = std::stod(token);
        }
        else if (std::regex_match(token, std::regex(R"REGEX("([^"]*)")REGEX")))
        {
            args[i - 1] = token.substr(1, token.size() - 2);
        }
        else if (token == "true")
        {
            args[i - 1] = true;
        }
        else if (token == "false")
        {
            args[i - 1] = false;
        }
        else
        {
            Misc::Print("Unknown token: " + token);
            return false;
        }
    }

    // --- Call builtin ---
    YYRValue result;

    if (!CallBuiltin(result, funcName.c_str(), nullptr, nullptr, args))
    {
        Misc::Print("Call failed: " + funcName);
        return false;
    }

    // --- Print result ---
    if (result.As<RValue>().Kind == VALUE_REAL)
        Misc::Print(std::format("{}", result.As<double>()));
    else if (result.As<RValue>().Kind == VALUE_STRING)
        Misc::Print(result.As<std::string>());
    else
        Misc::Print("<unknown result>");

    return true;
}


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
            if (ImGui::Button("Print all obj"))
            {
                Misc::Print("dumping var names");
                printAllObjects();
            }
            if (ImGui::Button("Obj at cursor pos"))
            {
                Misc::Print("obj at pos");
                getObjectAtMousePos();
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
                toggleDebug();
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

            ImGui::InputText("Filter", g_globalVarNameFilter, sizeof(g_globalVarNameFilter));
            ImGui::SameLine();
            if (ImGui::Button("Apply")) // apply filter by deleting unmatching entries from vector
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
    CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)LHCore::ResolveCore, (LPVOID)pack, 0, NULL); // Wait for LHCC

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

