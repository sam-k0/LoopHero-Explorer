// dllmain.cpp : Defines the entry point for the DLL application.
#include <Windows.h>
#include "PluginCore.h"
#include <SDK.hpp>
#include "MyPlugin.h"

#define _CRT_SECURE_NO_WARNINGS

// Entry point for the DLL
DllExport YYTKStatus PluginEntry(YYTKPlugin* PluginObject)
{
    return PluginCore::Initialize(PluginObject);
}

// Standard DLL entry point
BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
        DllHandle = hModule;
        break;
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}

