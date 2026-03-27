#pragma once

#include <map>
#include <string>
#include <vector>

class Dumper {
public:
    // Dump all object IDs to enum file
    static void DumpObjectIDs();

    // Dump all sprite IDs to enum file
    static void DumpSpriteIDs();

    // Dump instance variables for objects or globals
    static void DumpInstanceVariables(double index, bool isObject, bool dumpParsableOnly, const char* filename);

    // Dump create events to file
    static void DumpCreateEvents(const char* filename);

private:
    // Helper: Write map to file as C++ enum
    static void MapToFile(const std::map<int, std::string>& map, const std::string& filename, const std::string& enumName);
};
