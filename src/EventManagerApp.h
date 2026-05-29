#pragma once

#include "ConfigStore.h"
#include "EventService.h"
#include "EventWindow.h"
#include "HttpClient.h"
#include "ReminderService.h"
#include "SquadManager.h"
#include "SharedState.h"
#include "nexus/Nexus.h"
#include "mumble/Mumble.h"
#include "RTAPI/RTAPI.hpp"
#include <condition_variable>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace LegendaryImpactEventmanager
{
    class EventManagerApp
    {
    public:
        explicit EventManagerApp(HMODULE self);
        ~EventManagerApp();

        void Load(AddonAPI_t* api);
        void Unload();

        void Render();
        void RenderOptions();

        void OnInputBind(const char* identifier, bool isRelease);

        void RequestSyncNow();

    private:
        static EventManagerApp* GetInstance();
        static EventManagerApp* s_Instance;

        static void OnExtAddonLoaded(int* signature);
        static void OnExtAddonUnloaded(int* signature);

        void WorkerLoop();
        void ProcessPendingSquadEvents();
        void EnqueueSquadUpdate(const SquadMember& member);
        void EnqueueSquadRemove(const std::string& accountName);

        void RegisterNexusHooks();
        void DeregisterNexusHooks();

        void RegisterSquadHooks();
        void DeregisterSquadHooks();

        void LoadResources();

        static void OnSquadUpdate(RTAPI::GroupMember* aGroupMember);
        static void OnSquadLeave(RTAPI::GroupMember* aGroupMember);

        HMODULE m_Self = nullptr;
        AddonAPI_t* m_Api = nullptr;

        NexusLinkData_t* m_NexusLink = nullptr;
        Mumble::Data* m_MumbleLink = nullptr;
        RTAPI::RealTimeData* m_RtApi = nullptr;

        SharedState m_SharedState;
        HttpClient m_HttpClient;
        ConfigStore m_ConfigStore;
        EventService m_EventService;
        ReminderService m_ReminderService;
        EventWindow m_EventWindow;
        SquadManager m_SquadManager;

        bool m_Running = false;
        bool m_ManualSyncRequested = false;
        bool m_SquadHooksRegistered = false;

        struct PendingSquadEvent
        {
            bool remove = false;
            SquadMember member;
            std::string accountName;
        };

        std::vector<PendingSquadEvent> m_PendingSquadEvents;

        std::thread m_Worker;
        std::mutex m_WorkerMutex;
        std::condition_variable m_WorkerWake;
    };
}