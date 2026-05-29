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
    EventManagerApp* EventManagerApp::s_Instance = nullptr;
    EventManagerApp* EventManagerApp::GetInstance()
    {
        return s_Instance;
    }

    EventManagerApp::EventManagerApp(HMODULE self)
        : m_Self(self),
        m_ConfigStore(m_SharedState),
        m_EventService(m_SharedState, m_HttpClient),
        m_ReminderService(m_SharedState),
        m_SquadManager(m_SharedState),
        m_EventWindow(
            m_Api,
            m_SharedState,
            m_ConfigStore,
            m_ReminderService,
            m_RtApi,
            [this]() { RequestSyncNow(); }) 
    {
        s_Instance = this;
    }

    EventManagerApp::~EventManagerApp()
    {
        Unload();
        s_Instance = nullptr;
    }

    void EventManagerApp::Load(AddonAPI_t* api)
    {
        m_Api = api;

        ImGui::SetCurrentContext((ImGuiContext*)m_Api->ImguiContext);
        ImGui::SetAllocatorFunctions(
            (void* (*)(size_t, void*))m_Api->ImguiMalloc,
            (void (*)(void*, void*))m_Api->ImguiFree);
 
        m_NexusLink = (NexusLinkData_t*)m_Api->DataLink_Get(DL_NEXUS_LINK);
        m_MumbleLink = (Mumble::Data*)m_Api->DataLink_Get(DL_MUMBLE_LINK);
        m_RtApi = (RTAPI::RealTimeData*)m_Api->DataLink_Get(DL_RTAPI);

        if (!m_RtApi || (m_RtApi && m_RtApi->GameBuild == 0))
        {
            m_RtApi = nullptr;
        }

        LoadResources();
        RegisterNexusHooks();
        RegisterSquadHooks();

        m_ConfigStore.Load();
        m_EventService.LoadCachedEvents();

        {
            std::lock_guard<std::mutex> lock(m_WorkerMutex);
            m_Running = true;
            m_ManualSyncRequested = false;
        }

        m_Worker = std::thread(&EventManagerApp::WorkerLoop, this);
    }

    void EventManagerApp::Unload()
    {
        if (!m_Api) return;

        DeregisterSquadHooks();

        m_ConfigStore.Save();

        {
            std::lock_guard<std::mutex> lock(m_WorkerMutex);
            m_Running = false;
            m_ManualSyncRequested = true;
        }

        m_WorkerWake.notify_one();

        if (m_Worker.joinable())
        {
            m_Worker.join();
        }

        m_SquadManager.Clear();

        DeregisterNexusHooks();

        m_NexusLink = nullptr;
        m_MumbleLink = nullptr;
        m_RtApi = nullptr;

        m_Api = nullptr;
    }

    void EventManagerApp::OnExtAddonLoaded(int* signature)
    {
        if (!signature) return;

        auto* instance = GetInstance();
        if (!instance) return;

        // RTAPI
        if (*signature == RTAPI_SIG)
        {
            instance->m_RtApi = (RTAPI::RealTimeData*)instance->m_Api->DataLink_Get(DL_RTAPI);

            if (instance->m_RtApi && instance->m_RtApi->GameBuild == 0)
            {
                instance->m_RtApi = nullptr;
            }

            instance->RegisterSquadHooks();
        }
    }

    void EventManagerApp::OnExtAddonUnloaded(int* signature)
    {
        if (!signature) return;

        auto* instance = GetInstance();
        if (!instance) return;

        // RTAPI
        if (*signature == RTAPI_SIG)
        {
            instance->DeregisterSquadHooks();
            instance->m_RtApi = nullptr;
            instance->m_SquadManager.Clear();
        }
    }

    void EventManagerApp::LoadResources()
    {
        m_Api->Textures_LoadFromResource(Constants::IconId, IDB_PNG1, m_Self, nullptr);
        m_Api->Textures_LoadFromResource(Constants::IconHoverId, IDB_PNG2, m_Self, nullptr);
        m_Api->Textures_LoadFromResource(Constants::QuicknessIconId, IDB_PNG3, m_Self, nullptr);
        m_Api->Textures_LoadFromResource(Constants::AlacrityIconId, IDB_PNG4, m_Self, nullptr);
        m_Api->Textures_LoadFromResource(Constants::SquadIconId, IDB_PNG5, m_Self, nullptr);
        m_Api->Textures_LoadFromResource(Constants::NoSquadIconId, IDB_PNG6, m_Self, nullptr);
    }

    void EventManagerApp::OnSquadUpdate(RTAPI::GroupMember* aGroupMember)
    {
        if (!aGroupMember) return;
        auto* instance = GetInstance();

        if (!instance || !instance->m_Api) return;
        instance->m_SquadManager.UpdateMember(aGroupMember);
    }

    void EventManagerApp::OnSquadLeave(RTAPI::GroupMember* aGroupMember)
    {
        if (!aGroupMember) return;

        auto* instance = GetInstance();
        if (!instance || !instance->m_Api) return;

        instance->m_SquadManager.RemoveMember(aGroupMember);
    }

    void EventManagerApp::RegisterNexusHooks()
    {
        m_Api->InputBinds_RegisterWithString(Constants::KeybindId, ::OnInputBind, "F8");

        m_Api->QuickAccess_Add(
            Constants::QuickAccessId,
            Constants::IconId,
            Constants::IconHoverId,
            Constants::KeybindId,
            "Legendary Impact - Eventmanager");
      
        m_Api->GUI_Register(RT_Render, AddonRender);
        m_Api->GUI_Register(RT_OptionsRender, AddonOptions);

        m_Api->Events_Subscribe(EV_ADDON_LOADED, (EVENT_CONSUME) EventManagerApp::OnExtAddonLoaded);
        m_Api->Events_Subscribe(EV_ADDON_UNLOADED, (EVENT_CONSUME) EventManagerApp::OnExtAddonUnloaded);
    }

    void EventManagerApp::DeregisterNexusHooks()
    {
        m_Api->QuickAccess_Remove(Constants::QuickAccessId);
        m_Api->InputBinds_Deregister(Constants::KeybindId);

        m_Api->GUI_Deregister(AddonRender);
        m_Api->GUI_Deregister(AddonOptions);

        m_Api->Events_Unsubscribe(EV_ADDON_LOADED, (EVENT_CONSUME) EventManagerApp::OnExtAddonLoaded);
        m_Api->Events_Unsubscribe(EV_ADDON_UNLOADED, (EVENT_CONSUME) EventManagerApp::OnExtAddonUnloaded);
    }

    void EventManagerApp::RegisterSquadHooks()
    {
        if (!m_Api || !m_RtApi || m_RtApi->GameBuild == 0 || m_SquadHooksRegistered) return;

        m_Api->Events_Subscribe(EV_RTAPI_GROUP_MEMBER_JOINED, (EVENT_CONSUME) EventManagerApp::OnSquadUpdate);
        m_Api->Events_Subscribe(EV_RTAPI_GROUP_MEMBER_UPDATED, (EVENT_CONSUME) EventManagerApp::OnSquadUpdate);
        m_Api->Events_Subscribe(EV_RTAPI_GROUP_MEMBER_LEFT, (EVENT_CONSUME) EventManagerApp::OnSquadLeave);

        m_SquadHooksRegistered = true;
    }

    void EventManagerApp::DeregisterSquadHooks()
    {
        if (!m_Api || !m_SquadHooksRegistered) return;

        m_Api->Events_Unsubscribe(EV_RTAPI_GROUP_MEMBER_JOINED, (EVENT_CONSUME) EventManagerApp::OnSquadUpdate);
        m_Api->Events_Unsubscribe(EV_RTAPI_GROUP_MEMBER_UPDATED, (EVENT_CONSUME) EventManagerApp::OnSquadUpdate);
        m_Api->Events_Unsubscribe(EV_RTAPI_GROUP_MEMBER_LEFT, (EVENT_CONSUME) EventManagerApp::OnSquadLeave);

        m_SquadHooksRegistered = false;
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
        if (isRelease || !identifier)
        {
            return;
        }

        if (std::strcmp(identifier, Constants::KeybindId) != 0)
        {
            return;
        }

        m_SharedState.ToggleWindowShown();
        m_ConfigStore.Save();
    }

    void EventManagerApp::RequestSyncNow()
    {
        {
            std::lock_guard<std::mutex> lock(m_WorkerMutex);
            m_ManualSyncRequested = true;
        }

        m_WorkerWake.notify_one();
    }

    void EventManagerApp::WorkerLoop()
    {
        m_EventService.FetchEvents();

        while (true)
        {
            auto config = m_SharedState.GetConfig();

            int minutes = config ? config->refreshMinutes : 5;

            if (minutes < 5)
            {
                minutes = 5;
            }

            std::unique_lock<std::mutex> lock(m_WorkerMutex);

            m_WorkerWake.wait_for(lock, std::chrono::minutes(minutes), [this]()
                {
                    return !m_Running || m_ManualSyncRequested;
                });

            if (!m_Running)
            {
                break;
            }

            m_ManualSyncRequested = false;

            lock.unlock();

            m_EventService.FetchEvents();
        }
    }
}