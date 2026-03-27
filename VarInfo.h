#pragma once
#include <string>
struct VarInfo {
    std::string name;
    std::string type;
    std::string value;

    VarInfo(std::string _name, std::string _type, std::string _value)
    {
        name = _name;
        type = _type;
        value = _value;
    };
};
