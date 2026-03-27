#pragma once

#include <Windows.h>

// Forward declarations
struct YYTKEventBase;
struct YYTKCodeEvent;
struct YYTKPresentEvent;
class YYTKPlugin;
enum YYTKStatus;

class PluginCore {
public:
    // Main entry point for the plugin
    static YYTKStatus Initialize(YYTKPlugin* pluginObject);

    // Unload handler
    static YYTKStatus Unload();

    // Frame rendering callback
    static YYTKStatus OnFrameRender(YYTKEventBase* pEvent, void* optArgument);

    // Code execution callback (for event recording)
    static int OnCodeExecute(YYTKCodeEvent* codeEvent, void* optArgument);

private:
    // Install game patches/hooks
    static void InstallPatches();

    static YYTKPlugin* m_pPlugin;
};
