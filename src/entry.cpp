#include "EventManagerApp.h"
#include "Constants.h"
#include "nexus/Nexus.h"
#include <memory>

using namespace LegendaryImpactEventmanager;

namespace
{
    AddonDefinition g_AddonDef = {};
    HMODULE g_Self = nullptr;
    std::unique_ptr<EventManagerApp> g_App;
}

namespace LegendaryImpactEventmanager
{
    EventManagerApp* GetAppInstance()
    {
        return g_App.get();
    }
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH) g_Self = hModule;
    return TRUE;
}

void AddonLoad(AddonAPI* api)
{
    g_App = std::make_unique<EventManagerApp>(g_Self);
    g_App->Load(api);
}

void AddonUnload()
{
    if (g_App)
    {
        g_App->Unload();
        g_App.reset();
    }
}

void AddonRender()
{
    if (g_App) g_App->Render();
}

void AddonOptions()
{
    if (g_App) g_App->RenderOptions();
}

void OnInputBind(const char* identifier, bool isRelease)
{
    if (g_App) g_App->OnInputBind(identifier, isRelease);
}

extern "C" __declspec(dllexport) AddonDefinition* GetAddonDef()
{
    g_AddonDef.Signature = -84629;
    g_AddonDef.APIVersion = NEXUS_API_VERSION;
    g_AddonDef.Name = Constants::AddonName;
    g_AddonDef.Version.Major = 1;
    g_AddonDef.Version.Minor = 1;
    g_AddonDef.Version.Build = 3;
    g_AddonDef.Version.Revision = 1;
    g_AddonDef.Author = "Backxtar";
    g_AddonDef.Description = "Guild Wars 2 Eventmanager for Legendary Impact.";
    g_AddonDef.Load = AddonLoad;
    g_AddonDef.Unload = AddonUnload;
    g_AddonDef.Flags = EAddonFlags_None;

    g_AddonDef.Provider = EUpdateProvider_GitHub;
    g_AddonDef.UpdateLink = "https://github.com/Backxtar/gw2-pact-nexus-plugin";

    return &g_AddonDef;
}
