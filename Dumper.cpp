#include "Dumper.h"
#include "LHObjects.h"
#include "MyPlugin.h"
#include <fstream>
#include <iostream>
#include <algorithm>
#include <map>
#include <vector>
#include <format>

void Dumper::MapToFile(const std::map<int, std::string>& map, const std::string& filename, const std::string& enumName)
{
    std::ofstream outFile(filename);

    if (outFile.is_open()) {
        outFile << "#pragma once\n\n";
        outFile << "enum " << enumName << " {\n";

        for (const auto& entry : map) {
            outFile << "    " << (entry.second) << " = " << (entry.first) << ",\n";
        }

        outFile << "};\n";

        outFile.close();
        std::cout << filename << " file generated successfully.\n";
    }
    else {
        std::cerr << "Unable to open file for writing.\n";
    }
}

void Dumper::DumpObjectIDs()
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

    MapToFile(objMap, "objEnum.txt", "LHObjectEnum");
}

void Dumper::DumpSpriteIDs()
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

void Dumper::DumpInstanceVariables(double index, bool isObject, bool dumpParsableOnly, const char* filename)
{
    YYRValue allObjs, iid;
    std::vector<int> outIDs;
    if (isObject)
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

    for (int instId : outIDs)
    {
        YYRValue inst = (double)instId;

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

            std::string varName = static_cast<const char*>(item); // deep copy immediately

            YYRValue type, content;
            bool isok = true;
            // check type 
            if (dumpParsableOnly)
            {
                CallBuiltin(content, "variable_instance_get", nullptr, nullptr, { inst, varName });
                CallBuiltin(type, "typeof", nullptr, nullptr, { content });

                std::string typeStr = static_cast<const char*>(type);

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

    // Write to file
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

    Misc::Print(std::format("Dumped instance variables to {}", filename));
}

void Dumper::DumpCreateEvents(const char* filename)
{
    // This calls the external Misc::MapToFileA function which handles the g_createEvents map
    Misc::MapToFileA(&g_createEvents);
    Misc::Print(std::format("Dumped create events to {}", filename));
}
