#include "CommandExecutor.h"
#include "MyPlugin.h"
#include <regex>
#include <sstream>
#include <algorithm>

std::deque<std::string> CommandExecutor::m_commandHistory;
const size_t CommandExecutor::MAX_HISTORY_SIZE = 5;

std::vector<std::string> CommandExecutor::Tokenize(const std::string& command)
{
    std::vector<std::string> vResults;
    size_t _beginFuncCall = command.find_first_of('(');

    if (_beginFuncCall == std::string::npos)
    {
        return {};
    }

    size_t _endFuncCall = command.find_first_of(')');

    if (_endFuncCall == std::string::npos)
    {
        return {};
    }

    // Function name
    vResults.push_back(command.substr(0, _beginFuncCall));

    std::stringstream ss(command.substr(_beginFuncCall + 1, _endFuncCall - _beginFuncCall));
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

bool CommandExecutor::Execute(const std::string& cmd)
{
    if (cmd.empty())
        return false;

    std::string cmdcopy = cmd;

    // Handle global variable assignment/read
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

void CommandExecutor::AddToHistory(const std::string& command)
{
    // Avoid duplicate of last command
    if (m_commandHistory.empty() || m_commandHistory.back() != command)
    {
        m_commandHistory.push_back(command);

        if (m_commandHistory.size() > MAX_HISTORY_SIZE)
            m_commandHistory.pop_front();
    }
}
