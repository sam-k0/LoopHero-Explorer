#pragma once
#include <Windows.h>
#include "MyPlugin.h"

namespace LHCore {

    // Ready-check function
    typedef bool (*CoreReady)();
    CoreReady pCoreReady = nullptr;

    // Saying hello to the core function
    typedef bool (*RegisterModule)(std::string, YYTKPlugin*);
    RegisterModule pRegisterModule = nullptr;

    // unregistering from the core function
    typedef bool (*UnregisterModule)(std::string);
    UnregisterModule pUnregisterModule = nullptr;

    // Prototype for the callback function
    typedef int (*PrePostPatchCallback)(YYTKCodeEvent*, void*);

    typedef void (*InstallPrePatch)(PrePostPatchCallback);
    InstallPrePatch pInstallPrePatch = nullptr;

    typedef void (*InstallPostPatch)(PrePostPatchCallback);
    InstallPostPatch pInstallPostPatch = nullptr;

    typedef void (*CoreReadyCallback)();

    typedef struct CoreReadyPack {
        YYTKPlugin* myplugin = nullptr;
        CoreReadyCallback mycallback = nullptr;

        CoreReadyPack(YYTKPlugin* plugin, CoreReadyCallback callback)
        {
            myplugin = plugin;
            mycallback = callback;
        }
    } CoreReadyPack;

    DWORD WINAPI ResolveCore(LPVOID lpParam)
    {
        //Misc::Print("Unpacking struct");
        CoreReadyPack* pack = (CoreReadyPack*)(lpParam);
        //Misc::Print("Importing Core function");
        void* rawCoreReady;
        void* rawRegisterModule;
        void* rawUnregisterModule;
        void* rawInstallPrePatch;
        void* rawInstallPostPatch;
        while (true)
        {
            Sleep(10);
            // Loading ready function
            if (PmGetExported("CoreReady", rawCoreReady) == YYTK_OK)
            {
                pCoreReady = reinterpret_cast<CoreReady>(rawCoreReady);
                if (pCoreReady() == true)
                {
                    //Misc::Print("Core is present", CLR_GREEN);
                    // Loading register function
                    if (PmGetExported("RegisterModule", rawRegisterModule) == YYTK_OK)
                    {
                        pRegisterModule = reinterpret_cast<RegisterModule>(rawRegisterModule);
                        pRegisterModule(gPluginName, pack->myplugin);
                        //Misc::Print("Registered to Core", CLR_GREEN);
                    }
                    else
                    {
                        Misc::Print("Failed to load RegisterModule", CLR_RED);
                    }


                    // Loading unregister function
                    if (PmGetExported("UnregisterModule", rawUnregisterModule) == YYTK_OK)
                    {
                        pUnregisterModule = reinterpret_cast<UnregisterModule>(rawUnregisterModule);
                    }
                    else
                    {
                        Misc::Print("Failed to load UnregisterModule", CLR_RED);
                    }


                    // Loading PrePatch and PostPatch functions
                    if (PmGetExported("API_InstallPostPatch", rawInstallPostPatch) == YYTK_OK)
                    {
                        pInstallPostPatch = reinterpret_cast<InstallPostPatch>(rawInstallPostPatch);
                    }
                    else
                    {
                        Misc::Print("Failed to load InstallPostPatch", CLR_RED);
                    }

                    if (PmGetExported("API_InstallPrePatch", rawInstallPrePatch) == YYTK_OK)
                    {
                        pInstallPrePatch = reinterpret_cast<InstallPostPatch>(rawInstallPrePatch);
                    }
                    else
                    {
                        Misc::Print("Failed to load InstallPrePatch", CLR_RED);
                    }

                    if (pInstallPrePatch != nullptr)
                    {
                        // Call the callback in which all the imported funcs are called
                        //Misc::Print("According to exports, core is ready", CLR_GREEN);
                        pack->mycallback();
                    }
                    else {
                        Misc::Print("Could not call the Install Callback as either PrePatch or PostPatch is null", CLR_RED);
                    }

                    delete pack; // Delete the pack
                    return TRUE;
                }
            }
            Misc::Print("Waiting for Core. Did you install LoopHeroCallbackCore.dll?", CLR_RED);
        }
    }

}