#pragma once

#include <string>
#include <deque>
#include <vector>

class CommandExecutor {
public:
    // Parse and execute a command string
    // Supports: function_name(arg1, arg2, ...) or global.var_name = value
    static bool Execute(const std::string& command);

    // Add command to history
    static void AddToHistory(const std::string& command);

    // Get command history
    static const std::deque<std::string>& GetHistory() { return m_commandHistory; }

    // Clear history
    static void ClearHistory() { m_commandHistory.clear(); }

private:
    // Parse command string into tokens
    static std::vector<std::string> Tokenize(const std::string& command);

    static std::deque<std::string> m_commandHistory;
    static const size_t MAX_HISTORY_SIZE;
};
