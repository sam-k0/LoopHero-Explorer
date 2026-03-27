#include "UIWindows.h"
#include "Dumper.h"
#include "Inspector.h"
#include "CommandExecutor.h"
#include "MyPlugin.h"
#include "LHObjects.h"
#include "imgui/imgui.h"
#include <format>

// Static member initialization
bool UIWindows::m_bShowObjectsSafe = false;
bool UIWindows::m_bShowGlobals = false;
bool UIWindows::m_bShowDebugOverlay = false;
bool UIWindows::m_bShowFpsPlot = true;
bool UIWindows::m_bShowRunCmd = false;
bool UIWindows::m_bFilterShowParsedOnly = true;
bool UIWindows::m_bRecordCreateEvents = true;
bool UIWindows::m_bLogUncommonEvents = false;

std::vector<int> UIWindows::m_InstanceIds;
std::map<int, std::vector<VarInfo>> UIWindows::m_InstanceVarInfo;
std::vector<VarInfo> UIWindows::m_GlobalVarInfo;

char UIWindows::m_commandBuffer[512] = "";
char UIWindows::m_globalVarNameFilter[256] = "";
char UIWindows::m_globalVarValueFilter[256] = "";

float UIWindows::m_fpsHistory[FPS_BUFFER_SIZE] = {};
int UIWindows::m_fpsOffset = 0;
float UIWindows::m_currentFps = 0.0f;
double UIWindows::m_currentFrameDelta = 0.0;

void UIWindows::Initialize()
{
    // FPS profiler initialization handled in FrameCallback
}

void UIWindows::Render()
{
    RenderMainWindow();

    if (m_bShowObjectsSafe)
        RenderObjectExplorer();

    if (m_bShowGlobals)
        RenderGlobalExplorer();

    if (m_bShowFpsPlot)
        RenderFpsMonitor();

    if (m_bShowRunCmd)
        RenderCommandWindow();
}

void UIWindows::RenderMainWindow()
{
    ImGui::Begin("Loop Hero Explorer");

    // Variables Section
    if (ImGui::CollapsingHeader("Variables"))
    {
        ImGui::Text(std::format("Only show parsed variables: {}", m_bFilterShowParsedOnly).c_str());
        ImGui::SameLine();
        if (ImGui::Button("Toggle"))
        {
            ToggleFilterParsedOnly();
        }

        if (ImGui::Button("Safe Object explorer"))
        {
            Misc::Print("Show safe global explorer");
            ToggleObjectExplorer();
        }
        if (ImGui::Button("Global Variable Explorer"))
        {
            Misc::Print("Show global explorer");
            ToggleGlobalExplorer();
        }
    }

    // Dumping Section
    if (ImGui::CollapsingHeader("Dumping"))
    {
        if (ImGui::Button("Dump Objects"))
        {
            Dumper::DumpInstanceVariables(INSTANCE_ALL, true, true, "dump_objects.txt");
        }
        if (ImGui::Button("Dump Globals"))
        {
            Dumper::DumpInstanceVariables(INSTANCE_GLOBAL, false, true, "dump_globals.txt");
        }
        if (ImGui::Button("Dump create events"))
        {
            Misc::Print("Dumping to file!");
            Dumper::DumpCreateEvents("create_events.txt");
        }
        if (ImGui::Button("Reset event storage"))
        {
            Misc::Print("Resetting event vector");
            g_createEvents.clear();
        }
        if (ImGui::Button("Dump sprite names with ID"))
        {
            Misc::Print("Dumping sprite names with ID");
            Dumper::DumpSpriteIDs();
        }
        if (ImGui::Button("Dump object names with ID"))
        {
            Misc::Print("Dumping object names with ID");
            Dumper::DumpObjectIDs();
        }
        if (ImGui::Button("Print all obj"))
        {
            Misc::Print("dumping var names");
            Inspector::PrintAllObjects();
        }
        if (ImGui::Button("Obj at cursor pos"))
        {
            Misc::Print("obj at pos");
            Inspector::GetObjectAtMousePos();
        }
    }

    // Other Section
    if (ImGui::CollapsingHeader("Other"))
    {
        if (ImGui::Button("Hide Native Cursor"))
        {
            ShowCursor(FALSE);
        }
        ImGui::SameLine();
        if (ImGui::Button("Toggle Debug Overlay"))
        {
            ToggleDebugOverlay();
            Binds::CallBuiltin("show_debug_overlay", nullptr, nullptr, { double(m_bShowDebugOverlay) });
        }

        if (ImGui::Button("Record Create Events"))
        {
            ToggleRecordCreateEvents();
            Misc::Print("Recording create events: " + std::to_string(m_bRecordCreateEvents));
        }
        if (ImGui::Button("Run Command"))
        {
            ToggleCommandWindow();
        }
        ImGui::SameLine();
        if (ImGui::Button("Show Fps"))
        {
            ToggleFpsPlot();
        }
        ImGui::SameLine();
        if (ImGui::Button("Log Events"))
        {
            ToggleLogUncommonEvents();
        }
    }

    ImGui::End();
}

void UIWindows::RenderObjectExplorer()
{
    ImGui::Begin("Safe Object explorer");
    
    // Refresh button
    if (ImGui::Button("Refresh"))
    {
        m_InstanceIds.clear();

        YYRValue allObjs, iid;

        CallBuiltin(allObjs, "instance_number", nullptr, nullptr, { INSTANCE_ALL });
        int count = (int)allObjs;

        for (int objIndex = 0; objIndex < count; objIndex++)
        {
            CallBuiltin(iid, "instance_id_get", nullptr, nullptr, { (double)objIndex });
            m_InstanceIds.push_back(int(iid));
        }
    }

    // Variable display
    ImGui::BeginChild("scroll_region", ImVec2(0, 0), true);
    for (const auto& iid : m_InstanceIds)
    {
        int objid = (int)Binds::CallBuiltinA("variable_instance_get", { double(iid), "object_index" });
        std::string obj_name = LHObjects::GetObjectName(objid);
        std::string label = std::format("{}:{}({})", std::to_string(iid), obj_name, objid);

        if (ImGui::CollapsingHeader(label.c_str()))
        {
            bool exists = true;
            if (ImGui::Button(("Refresh##" + std::to_string(iid)).c_str()))
            {
                Misc::Print(std::format("Getting vars for instance {}", iid));
                m_InstanceVarInfo[iid] = Inspector::FetchInstanceVariablesSafe(iid, exists);
            }
            
            if (exists)
            {
                if (bool(Binds::CallBuiltinA("instance_exists", { double(iid) })))
                {
                    for (const auto& var : m_InstanceVarInfo[iid])
                    {
                        if (var.value != "<unknown>" || !m_bFilterShowParsedOnly)
                        {
                            ImVec4 col = ImVec4(255, 255, 255, 255); // white default
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
                    ImGui::TextColored({ 255, 0, 0, 255 }, "Does not exist anymore");
                }
            }
            else
            {
                ImGui::TextColored({ 255, 0, 0, 255 }, "Not indexed!");
            }
        }
    }

    ImGui::EndChild();
    ImGui::End();
}

void UIWindows::RenderGlobalExplorer()
{
    ImGui::Begin("Global Variables");

    if (ImGui::Button("Refresh"))
    {
        m_GlobalVarInfo.clear();
        bool _;
        m_GlobalVarInfo = Inspector::FetchInstanceVariablesSafe(INSTANCE_GLOBAL, _);
    }
    
    if (ImGui::CollapsingHeader("Filters"))
    {
        // Filter var name
        ImGui::InputText("Filter Name", m_globalVarNameFilter, sizeof(m_globalVarNameFilter));
        ImGui::SameLine();
        if (ImGui::Button("Apply##Name"))
        {
            std::string filter = m_globalVarNameFilter;
            std::vector<VarInfo>::iterator it = m_GlobalVarInfo.begin();

            while (it != m_GlobalVarInfo.end())
            {
                if (!Misc::StringHasSubstr(it->name, filter))
                {
                    it = m_GlobalVarInfo.erase(it);
                }
                else
                    ++it;
            }
        }
        
        // Filter value
        ImGui::InputText("Filter Value", m_globalVarValueFilter, sizeof(m_globalVarValueFilter));
        ImGui::SameLine();
        if (ImGui::Button("Apply##Value"))
        {
            std::string filter = m_globalVarValueFilter;
            std::vector<VarInfo>::iterator it = m_GlobalVarInfo.begin();

            while (it != m_GlobalVarInfo.end())
            {
                if (!Misc::StringHasSubstr(it->value, filter))
                {
                    it = m_GlobalVarInfo.erase(it);
                }
                else
                    ++it;
            }
        }
    }
    
    // Variable display
    ImGui::BeginChild("scroll_region", ImVec2(0, 0), true);

    for (const auto& var : m_GlobalVarInfo)
    {
        if (var.value == "<unset>")
        {
            ImGui::TextColored({ 255.0, 0., 0., 255.0 }, std::format("{} ({}): {}", var.name, var.type, var.value).c_str());
        }
        else if (var.value != "<unknown>" || !m_bFilterShowParsedOnly)
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

void UIWindows::RenderFpsMonitor()
{
    float minFPS = FLT_MAX;
    float maxFPS = 0.0f;

    for (int i = 0; i < FPS_BUFFER_SIZE; i++)
    {
        float v = m_fpsHistory[i];

        if (v > 0.0f) // ignore uninitialized values
        {
            if (v < minFPS) minFPS = v;
            if (v > maxFPS) maxFPS = v;
        }
    }

    ImGui::Begin("Fps Monitor");
    ImGui::PlotLines(
        "FPS",
        m_fpsHistory,
        FPS_BUFFER_SIZE,
        m_fpsOffset,         // offset for circular buffer
        nullptr,             // overlay text
        0.0f,                // min
        144.0f,              // max (adjust to your monitor)
        ImVec2(0, 80)        // size
    );
    ImGui::Text(("Fps: " + std::to_string(m_currentFps)).c_str());
    ImGui::SameLine();
    ImGui::Text(("ft(ms): " + std::to_string(m_currentFrameDelta * 1000.0)).c_str());
    ImGui::Text(("High: " + std::to_string(maxFPS)).c_str());
    ImGui::SameLine();
    ImGui::Text(("Low: " + std::to_string(minFPS)).c_str());

    ImGui::End();
}

void UIWindows::RenderCommandWindow()
{
    ImGui::Begin("Run Command");

    ImGui::InputText("cmd", m_commandBuffer, sizeof(m_commandBuffer));

    if (ImGui::Button("Run"))
    {
        std::string cmd = m_commandBuffer;

        if (CommandExecutor::Execute(cmd))
        {
            CommandExecutor::AddToHistory(cmd);
        }
    }

    ImGui::Separator();
    ImGui::Text("History:");

    const auto& history = CommandExecutor::GetHistory();
    for (int i = (int)history.size() - 1; i >= 0; --i)
    {
        const std::string& command = history[i];

        if (ImGui::Button(command.c_str()))
        {
            // Copy into input field
            strncpy(m_commandBuffer, command.c_str(), sizeof(m_commandBuffer));
            m_commandBuffer[sizeof(m_commandBuffer) - 1] = '\0'; // safety
        }
    }

    ImGui::End();
}

void UIWindows::UpdateFrameMetrics(float fps, double frameDelta)
{
    m_currentFps = fps;
    m_currentFrameDelta = frameDelta;
    m_fpsHistory[m_fpsOffset] = fps;
    m_fpsOffset = (m_fpsOffset + 1) % FPS_BUFFER_SIZE;
}
