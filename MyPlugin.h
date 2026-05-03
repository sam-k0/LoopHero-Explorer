#pragma once
#include <fstream>
#include <iterator>
#include <string>
#include <Windows.h>
#include <vector>
#include <direct.h>
#include <ostream>
#include <sstream>
#include <iomanip>
#include <map>
#include <optional>
// YYTK 
#define YYSDK_PLUGIN // Declare the following code as a plugin
//#define DEBUG // enables more prints
#include "SDK/SDK.hpp"
#include "Filesystem.h"


HINSTANCE DllHandle; // Self modhandle
std::string gPluginName = "sam-k0.LHExplorer.yytk";
YYTKPlugin* gThisPlugin = nullptr;

std::map<int, std::string> g_createEvents; // instance id to event
int g_DumpNum = 0;

std::string DCS(YYRValue val)
{
    // Try to get C string from YYRValue
    const char* strPtr = static_cast<const char*>(val);

    if (strPtr)
    {
        // std::string constructor makes a deep copy
        return std::string(strPtr);
    }
    else
    {
        return std::string();
    }
}

// Stores info about a variable
struct VarInfo {
    std::string name;
    std::string type;
    std::string value;
};

// same for a whole instance
struct InstanceInfo {
    std::string name;
    int objectindex;
    int instanceid;
    std::optional<std::vector<VarInfo>> variables;
};


DllExport std::string GetPluginName() // For yytk
{
    return gPluginName;
}

constexpr uint32_t Hash(const char* str)
{
    uint32_t hash = 2166136261u;
    while (*str)
    {
        hash ^= (uint8_t)*str++;
        hash *= 16777619u;
    }
    return hash;
}

namespace Misc {

    void Print(std::string s, Color c = CLR_DEFAULT)
    {
        PrintMessage(c, (gPluginName + ": " + s).c_str());
    }

    void Print(int i, Color c = CLR_DEFAULT)
    {
        PrintMessage(c, (gPluginName + ": " + std::to_string(i)).c_str());
    }

    void Print(const char* s, Color c = CLR_DEFAULT)
    {
        PrintMessage(c, (gPluginName + ": " + std::string(s)).c_str());
    }

    void PrintDbg(std::string s, std::string func, int line, Color c)
    {
        PrintMessage(c, (gPluginName + ": " + s + " (" + func + ":" + std::to_string(line) + ")").c_str());
    }

    void PrintArrayInstanceVariables(YYRValue var, YYRValue inst)
    {
        YYRValue len, item, content, type;

        CallBuiltin(len, "array_length_1d", nullptr, nullptr, { var });
        int count = (int)len;

        for (int i = 0; i < count; i++)
        {
            CallBuiltin(item, "array_get", nullptr, nullptr, { var, (double)i });

            // SAFE: copy variable name immediately
            const char* itemPtr = static_cast<const char*>(item);
            std::string varName = itemPtr ? itemPtr : "";

            // Get value
            CallBuiltin(content, "variable_instance_get", nullptr, nullptr, { inst, varName.c_str() });

            // Get type
            CallBuiltin(type, "typeof", nullptr, nullptr, { content });

            const char* typePtr = static_cast<const char*>(type);
            std::string typeStr = typePtr ? typePtr : "unknown";

            std::string message = varName + " -> " + typeStr;

            // Extract value safely
            if (typeStr == "number")
            {
                message += " : " + std::to_string((double)content);
            }
            else if (typeStr == "bool")
            {
                message += " : " + std::to_string((bool)content ? 1 : 0);
            }
            else if (typeStr == "string")
            {
                const char* strPtr = static_cast<const char*>(content);
                std::string valueStr = strPtr ? strPtr : "";
                message += " : " + valueStr;
            }

            Misc::Print(message);
        }
    }

    void MapToFileA(std::map<int, std::string>* m)
    {
        std::string fname = "./event_dump_" + std::to_string(g_DumpNum) + ".txt";
        while (Filesys::FileExists(fname))
        {
            g_DumpNum++;
            fname = "./event_dump_" + std::to_string(g_DumpNum) + ".txt";
        }

        Print("Dumping to file " + fname, CLR_RED);

        std::ofstream ofile(fname);

        for (const auto& [instanceID, str] : *m)
        {
            ofile << instanceID << " # " << str << "\n";
        }
    }

    bool VectorContains(std::string find, std::vector<std::string>* v)
    {
        if (std::find(v->begin(), v->end(), find) != v->end())
        {
            return true;
        }
        return false;
    }

    bool AddToVectorNoDuplicates(std::string str, std::vector<std::string>* v)
    {
        if (VectorContains(str, v))
        {
            return false;
        }
        v->push_back(str);
        return true;
    }

    inline bool FileExists(const std::string& name) {
        if (FILE* file = fopen(name.c_str(), "r")) {
            fclose(file);
            return true;
        }
        else {
            return false;
        }
    }

    // checks if a string s contains a substring subs
    bool StringHasSubstr(std::string s, std::string subs)
    {
        if (s.find(subs) != std::string::npos) {
            return true;
        }
        return false;
    }

    // checks if the vector contains a string containing a substringand returns the whole string if found
    std::string VectorFindSubstring(std::vector<std::string> strings, std::string subs)
    {
        if (strings.size() == 0)
        {
            return "";
        }

        for (std::string s : strings)
        {
            if (StringHasSubstr(s, subs))
            {
                return s;
            }
        }
        return "";
    }

    //https://www.techiedelight.com/check-if-a-string-ends-with-another-string-in-cpp/
    bool StringEndsWith(std::string const& str, std::string const& suffix) {
        if (str.length() < suffix.length()) {
            return false;
        }
        return str.compare(str.length() - suffix.length(), suffix.length(), suffix) == 0;
    }

    std::string Join(std::vector<std::string> strings)
    {
        if (strings.size() == 0)
        {
            return "";
        }

        const char* const delim = " ";
        std::ostringstream imploded;
        std::string ret;
        std::copy(strings.begin(), strings.end(), std::ostream_iterator<std::string>(imploded, delim));
        ret = imploded.str();
        ret.pop_back();

        return ret;
    }

    void PrintArray(YYRValue var, Color c = Color::CLR_DEFAULT)
    {
        YYRValue len;
        YYRValue item;
        CallBuiltin(len, "array_length_1d", nullptr, nullptr, { var });

        for (int i = 0; i < (int)len; i++)
        {
            CallBuiltin(item, "array_get", nullptr, nullptr, { var, (double)i });
            Misc::Print(static_cast<const char*>(item), c);
        }
    }

    std::string to_string_trimmed(double value, int prec) {
        std::ostringstream out;
        out << std::fixed << std::setprecision(prec) << value;  // Set the precision to 2 or whatever you need
        std::string str = out.str();

        // Remove trailing zeros
        str.erase(str.find_last_not_of('0') + 1, std::string::npos);

        // Remove the decimal point if it's the last character (e.g., "5.")
        if (str.back() == '.') {
            str.pop_back();
        }

        return str;
    }

    void GetFirstOfObject(YYRValue& inst, int objType)
    {
        CallBuiltin(inst, "instance_nearest", nullptr, nullptr, { 0.0,0.0,(double)objType });
        Misc::Print(std::to_string((int)inst));
    }

}

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