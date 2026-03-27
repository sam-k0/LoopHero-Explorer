#pragma once

#include <string>
#include <vector>

// Information about a variable
struct VarInfo {
    std::string name;
    std::string type;
    std::string value;
};

class Inspector {
public:
    // Fetch instance variables (unsafe - requires immediate deep copy)
    static std::vector<VarInfo> FetchInstanceVariables(double instanceId);

    // Fetch instance variables safely (only returns variables from indexed map)
    static std::vector<VarInfo> FetchInstanceVariablesSafe(double instanceId, bool& outExists);

    // Print all objects in the game world
    static void PrintAllObjects();

    // Get object at mouse cursor position and print its info
    static void GetObjectAtMousePos();

    // Toggle debug overlay
    static void ToggleDebugOverlay();
};
