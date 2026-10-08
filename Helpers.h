#pragma once
#include <string>
#include <vector>
#include <regex>
#include <sstream>
#include "SDK/SDK.hpp"

namespace Binds {
    void GetInstanceVariables(YYRValue& arr, YYRValue inst)
    {
        CallBuiltin(arr, "variable_instance_get_names", nullptr, nullptr, { inst });
    }

    YYRValue CallBuiltin(const std::string name, CInstance* self, CInstance* other, const std::vector<YYRValue> args)
    {
        YYRValue var;
        CallBuiltin(var, name, self, other, args);
        return var;
    }

    YYRValue CallBuiltinA(const std::string name, const std::vector<YYRValue> args)
    {
        YYRValue var;
        CallBuiltin(var, name, nullptr, nullptr, args);
        return var;
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

void mapToFile(std::map<int, std::string> arg, std::string fname, std::string enumName)
{
    std::ofstream outFile(fname);

    if (outFile.is_open()) {
        outFile << "#pragma once\n\n";
        outFile << "enum " << enumName << " {\n";

        for (const auto& entry : arg) {
            outFile << "    " << (entry.second) << " = " << (entry.first) << ",\n";
        }

        outFile << "};\n";

        outFile.close();
        std::cout << fname << " file generated successfully.\n";
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
        std::string namestr = name.operator std::string();
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
    while (true)
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
        std::string spriteNameStr = spriteName.operator std::string();
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

constexpr int PREVIEW_ITEMS = 16;
constexpr int PREVIEW_DEPTH = 2;
constexpr int MAX_ARRAY_ITEMS = 1000;
// Arrays can contain themselves, this stops the recursion.
constexpr int MAX_ARRAY_DEPTH = 8;

std::string GetTypeName(const YYRValue& val)
{
    return DCS(Binds::CallBuiltinA("typeof", { val }));
}

std::string FormatScalar(const YYRValue& val, const std::string& typeStr)
{
    if (typeStr == "number" || typeStr == "int32" || typeStr == "int64")
        return std::to_string(double(val));
    if (typeStr == "bool")
        return bool(val) ? "true" : "false";
    if (typeStr == "string")
        return DCS(val);
    if (typeStr == "undefined")
        return "undefined";
    return "<unknown>";
}

std::string ArrayPreview(const VarInfo& arr, int depth = 0)
{
    if (depth >= PREVIEW_DEPTH || (arr.children.empty() && arr.length > 0))
        return std::format("[{} items]", arr.length);

    std::string out = "[";
    for (int i = 0; i < (int)arr.children.size() && i < PREVIEW_ITEMS; i++)
    {
        const VarInfo& item = arr.children[i];
        if (i > 0)
            out += ", ";
        if (item.type == "array")
            out += ArrayPreview(item, depth + 1);
        else if (item.type == "string")
            out += "\"" + item.value + "\"";
        else
            out += item.value;
    }
    if (arr.length > PREVIEW_ITEMS)
        out += std::format(", ... +{}", arr.length - PREVIEW_ITEMS);
    return out + "]";
}

VarInfo ReadValue(const std::string& name, const YYRValue& val, int depth = 0)
{
    VarInfo info{ name, GetTypeName(val) };
    if (info.type != "array")
    {
        info.value = FormatScalar(val, info.type);
        return info;
    }

    info.length = (int)Binds::CallBuiltinA("array_length_1d", { val });
    if (depth < MAX_ARRAY_DEPTH)
    {
        for (int i = 0; i < info.length && i < MAX_ARRAY_ITEMS; i++)
        {
            YYRValue item = Binds::CallBuiltinA("array_get", { val, (double)i });
            info.children.push_back(ReadValue(std::format("[{}]", i), item, depth + 1));
        }
    }
    info.value = ArrayPreview(info);
    return info;
}

std::string YYRValueToString(const YYRValue& val)
{
    return ReadValue("", val).value;
}

// exists is false if the instance is gone
std::vector<VarInfo> FetchInstanceVariables(double inst, bool& exists)
{
    std::vector<VarInfo> result;

    exists = inst == INSTANCE_GLOBAL || bool(Binds::CallBuiltinA("instance_exists", { inst }));
    if (!exists)
        return result;

    YYRValue names;
    Binds::GetInstanceVariables(names, inst);
    int count = (int)Binds::CallBuiltinA("array_length_1d", { names });

    for (int i = 0; i < count; i++)
    {
        std::string varName = DCS(Binds::CallBuiltinA("array_get", { names, (double)i }));
        YYRValue content = Binds::CallBuiltinA("variable_instance_get", { inst, varName });
        result.push_back(ReadValue(varName, content));
    }

    return result;
}

void FetchInstanceVarsDumpFile(double index, bool isobject, bool dumpparsable, const char* filename)
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

bool RunCommand(const std::string& cmd)
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
