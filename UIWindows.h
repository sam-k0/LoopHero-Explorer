#pragma once

#include "Inspector.h"
#include <vector>
#include <map>
#include <string>

class UIWindows {
public:
    // Initialize UI state (call once)
    static void Initialize();

    // Render all UI windows (call each frame)
    static void Render();

    // Update FPS measurements each frame
    static void UpdateFrameMetrics(float fps, double frameDelta);

    // Toggle visibility of specific windows
    static void ToggleObjectExplorer() { m_bShowObjectsSafe = !m_bShowObjectsSafe; }
    static void ToggleGlobalExplorer() { m_bShowGlobals = !m_bShowGlobals; }
    static void ToggleFpsPlot() { m_bShowFpsPlot = !m_bShowFpsPlot; }
    static void ToggleCommandWindow() { m_bShowRunCmd = !m_bShowRunCmd; }
    static void ToggleFilterParsedOnly() { m_bFilterShowParsedOnly = !m_bFilterShowParsedOnly; }
    static void ToggleLogUncommonEvents() { m_bLogUncommonEvents = !m_bLogUncommonEvents; }
    static void ToggleRecordCreateEvents() { m_bRecordCreateEvents = !m_bRecordCreateEvents; }
    static void ToggleDebugOverlay() { m_bShowDebugOverlay = !m_bShowDebugOverlay; }

    // Getters for state
    static bool ShouldRecordCreateEvents() { return m_bRecordCreateEvents; }
    static bool ShouldLogUncommonEvents() { return m_bLogUncommonEvents; }

private:
    // UI Rendering methods
    static void RenderMainWindow();
    static void RenderObjectExplorer();
    static void RenderGlobalExplorer();
    static void RenderFpsMonitor();
    static void RenderCommandWindow();

    // UI State
    static bool m_bShowObjectsSafe;
    static bool m_bShowGlobals;
    static bool m_bShowDebugOverlay;
    static bool m_bShowFpsPlot;
    static bool m_bShowRunCmd;
    static bool m_bFilterShowParsedOnly;
    static bool m_bRecordCreateEvents;
    static bool m_bLogUncommonEvents;

    // Instance tracking
    static std::vector<int> m_InstanceIds;
    static std::map<int, std::vector<VarInfo>> m_InstanceVarInfo;
    static std::vector<VarInfo> m_GlobalVarInfo;

    // Command executor
    static char m_commandBuffer[512];
    static char m_globalVarNameFilter[256];
    static char m_globalVarValueFilter[256];

    // FPS monitoring
    static constexpr int FPS_BUFFER_SIZE = 120;
    static float m_fpsHistory[FPS_BUFFER_SIZE];
    static int m_fpsOffset;
    static float m_currentFps;
    static double m_currentFrameDelta;
};
