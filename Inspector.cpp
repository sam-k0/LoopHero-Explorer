#include "Inspector.h"
#include "LHObjects.h"
#include "MyPlugin.h"
#include <vector>
#include <algorithm>

std::vector<VarInfo> Inspector::FetchInstanceVariables(double instanceId)
{
    std::vector<VarInfo> result;

    YYRValue varr = Binds::CallBuiltinA("variable_instance_get_names", { instanceId });    
    Misc::Print(varr.As<double>());
    YYRValue len = Binds::CallBuiltinA("array_length_1d", { varr });

    int count = len.As<double>();

    for (int i = 0; i < count; i++)
    {
        YYRValue item, content, type;

        CallBuiltin(item, "array_get", nullptr, nullptr, { varr, (double)i });
        std::string varName = static_cast<const char*>(item);

        CallBuiltin(content, "variable_instance_get", nullptr, nullptr, { instanceId, varName.c_str() });
        CallBuiltin(type, "typeof", nullptr, nullptr, { content });

        std::string typeStr = static_cast<const char*>(type);
        std::string valueStr;

        if (typeStr == "number")
            valueStr = std::to_string(double(content));
        else if (typeStr == "bool")
            valueStr = bool(content) ? "true" : "false";
        else if (typeStr == "string")
            valueStr = static_cast<const char*>(content);
        else
            valueStr = "<unknown>";

        result.push_back({ varName, typeStr, valueStr });
    }

    return result;
}

std::vector<VarInfo> Inspector::FetchInstanceVariablesSafe(double instanceId, bool& outExists)
{
    std::vector<VarInfo> result;
    int oid = INSTANCE_GLOBAL; // global inst doesnt have object index, so use as default
    
    // Try to find inst in map
    if (instanceId != INSTANCE_GLOBAL)
    {
        oid = Binds::CallBuiltinA("variable_instance_get", { instanceId, "object_index" });

        if (!g_ObjectVarNames.contains((int)oid))
        {
            Misc::Print(std::format("Instance id {} ({}) not found in map!", instanceId, oid), CLR_RED);
            outExists = false;
            return result;
        }
    }
    
    outExists = true;
    for (const auto& varName : g_ObjectVarNames.at((int)oid))
    {
        YYRValue item, content, type, isset;
        
        // Check if the variable exists already (may be inited later, would be violation)
        if (instanceId != INSTANCE_GLOBAL)
        {
            CallBuiltin(isset, "variable_instance_exists", nullptr, nullptr, { instanceId, varName });
        }
        else // use this for globals, other doesnt work for some reason
        {
            CallBuiltin(isset, "variable_global_exists", nullptr, nullptr, { varName });
        }
        
        if (bool(isset)) // exists
        {
            CallBuiltin(content, "variable_instance_get", nullptr, nullptr, { instanceId, varName });
            CallBuiltin(type, "typeof", nullptr, nullptr, { content });

            std::string typeStr = static_cast<const char*>(type);
            std::string valueStr;

            if (typeStr == "number")
                valueStr = std::to_string(double(content));
            else if (typeStr == "bool")
                valueStr = bool(content) ? "true" : "false";
            else if (typeStr == "string")
                valueStr = static_cast<const char*>(content);
            else
                valueStr = "<unknown>";

            result.push_back({ varName, typeStr, valueStr });
        }
        else
        {
            result.push_back({ varName, "<?type>", "<unset>" });
        }
    }

    return result;
}

void Inspector::PrintAllObjects()
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

void Inspector::GetObjectAtMousePos()
{
    YYRValue mx = Binds::CallBuiltin("device_mouse_x", nullptr, nullptr, { 0.0 });
    YYRValue my = Binds::CallBuiltin("device_mouse_y", nullptr, nullptr, { 0.0 });
    
    YYRValue obj = Binds::CallBuiltin("instance_nearest", nullptr, nullptr, { mx, my, INSTANCE_ALL });

    YYRValue arr;
    Binds::GetInstanceVariables(arr, obj);
    Misc::Print("______________" + std::to_string((int)obj), Color::CLR_AQUA);
    Misc::PrintArrayInstanceVariables(arr, obj);

    YYRValue oi = Binds::CallBuiltinA("variable_instance_get", { obj, "object_index" });
    Misc::Print(static_cast<const char*>(oi));
}

void Inspector::ToggleDebugOverlay()
{
    // This should call the game's debug overlay function
    // Requires g_showDebugOverlay to be in global state
}
