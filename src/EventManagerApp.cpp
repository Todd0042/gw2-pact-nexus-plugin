#include "EventManagerApp.h"
#include "Constants.h"
#include "resource.h"
#include "imgui/imgui.h"
#include <chrono>
#include <cstring>

void AddonRender();
void AddonOptions();
void OnInputBind(const char* identifier, bool isRelease);

namespace LegendaryImpactEventmanager
{
    EventManagerApp::EventManagerApp(HMODULE self)
        : m_Self(self),
        m_ConfigStore(m_SharedState),
        m_EventService(m_SharedState, m_HttpClient),
        m_ReminderService(m_SharedState),
        m_EventWindow(m_Api, m_SharedState, m_ConfigStore, m_ReminderService, [this]() { RequestSyncNow(); })
    {
    }

    EventManagerApp::~EventManagerApp()
    {
        Unload();
    }

    void EventManagerApp::Load(AddonAPI* api)
    {
        m_Api = api;

        ImGui::SetCurrentContext((ImGuiContext*)m_Api->ImguiContext);
        ImGui::SetAllocatorFunctions(
            (void* (*)(size_t, void*))m_Api->ImguiMalloc,
            (void (*)(void*, void*))m_Api->ImguiFree
        );

        m_NexusLink = (NexusLinkData*)m_Api->DataLink.Get("DL_NEXUS_LINK");
        m_MumbleLink = (Mumble::Data*)m_Api->DataLink.Get("DL_MUMBLE_LINK");

        LoadResources();
        RegisterNexusHooks();
        m_ConfigStore.Load();

        m_Running = true;
        m_Worker = std::thread(&EventManagerApp::WorkerLoop, this);

        m_Api->Log(ELogLevel_DEBUG, Constants::AddonName, "<c=#00ff00>Legendary Impact - Eventmanager</c> was loaded.");
    }

    void EventManagerApp::Unload()
    {
        if (!m_Api) return;

        m_ConfigStore.Save();
        m_Running = false;
        m_ManualSyncRequested = true;
        m_WorkerWake.notify_one();

        if (m_Worker.joinable()) m_Worker.join();
        DeregisterNexusHooks();

        m_Api->Log(ELogLevel_DEBUG, Constants::AddonName, "Signing off <c=#ff0000>Legendary Impact - Eventmanager</c>, it was an honor commander.");
        m_Api = nullptr;
    }

    void EventManagerApp::LoadResources()
    {
        m_Api->Textures.LoadFromResource(Constants::IconId, IDB_PNG1, m_Self, nullptr);
        m_Api->Textures.LoadFromResource(Constants::IconHoverId, IDB_PNG2, m_Self, nullptr);
        m_Api->Textures.LoadFromResource(Constants::QuicknessIconId, IDB_PNG3, m_Self, nullptr);
        m_Api->Textures.LoadFromResource(Constants::AlacrityIconId, IDB_PNG4, m_Self, nullptr);
    }

    void EventManagerApp::RegisterNexusHooks()
    {
        m_Api->InputBinds.RegisterWithString(Constants::KeybindId, ::OnInputBind, "F8");
        m_Api->QuickAccess.Add(Constants::QuickAccessId, Constants::IconId, Constants::IconHoverId, Constants::KeybindId, "Legendary Impact - Eventmanager");
        m_Api->Renderer.Register(ERenderType_Render, AddonRender);
        m_Api->Renderer.Register(ERenderType_OptionsRender, AddonOptions);
    }

    void EventManagerApp::DeregisterNexusHooks()
    {
        m_Api->QuickAccess.Remove(Constants::QuickAccessId);
        m_Api->InputBinds.Deregister(Constants::KeybindId);
        m_Api->Renderer.Deregister(AddonRender);
        m_Api->Renderer.Deregister(AddonOptions);
    }

    void EventManagerApp::Render()
    {
        m_EventWindow.RenderAddonWindow();
    }

    void EventManagerApp::RenderOptions()
    {
        m_EventWindow.RenderOptions();
    }

    void EventManagerApp::OnInputBind(const char* identifier, bool isRelease)
    {
        if (isRelease || !identifier) return;
        if (std::strcmp(identifier, Constants::KeybindId) != 0) return;
        m_SharedState.ToggleWindowShown();
        m_ConfigStore.Save();
    }

    void EventManagerApp::RequestSyncNow()
    {
        m_ManualSyncRequested = true;
        m_WorkerWake.notify_one();
    }

    void EventManagerApp::WorkerLoop()
    {
        m_EventService.FetchEvents();
        while (m_Running)
        {
            auto config = m_SharedState.GetConfig();
            int minutes = config ? config->refreshMinutes : 5;
            if (minutes < 5) minutes = 5;

            auto nextWake = std::chrono::steady_clock::now() + std::chrono::minutes(minutes);
            std::unique_lock<std::mutex> lock(m_WorkerMutex);
            m_WorkerWake.wait_until(lock, nextWake, [this]() { return !m_Running.load() || m_ManualSyncRequested.load(); });

            if (!m_Running) break;
            m_ManualSyncRequested = false;
            lock.unlock();
            m_EventService.FetchEvents();
        }
    }
}
