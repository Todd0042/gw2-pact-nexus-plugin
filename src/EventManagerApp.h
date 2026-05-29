#pragma once

#include "ConfigStore.h"
#include "EventService.h"
#include "EventWindow.h"
#include "HttpClient.h"
#include "ReminderService.h"
#include "SharedState.h"
#include "nexus/Nexus.h"
#include "mumble/Mumble.h"
#include "RTAPI/RTAPI.hpp"
#include <condition_variable>
#include <mutex>
#include <thread>

namespace LegendaryImpactEventmanager
{
    class EventManagerApp
    {
    public:
        explicit EventManagerApp(HMODULE self);
        ~EventManagerApp();

        void Load(AddonAPI_t* api);
        void Unload();

        static void OnExtAddonLoaded(int* signature);
        static void OnExtAddonUnloaded(int* signature);

        void Render();
        void RenderOptions();

        void OnInputBind(const char* identifier, bool isRelease);

        void RequestSyncNow();

    private:
        static EventManagerApp* GetInstance();
        static EventManagerApp* s_Instance;

        void WorkerLoop();

        void RegisterNexusHooks();
        void DeregisterNexusHooks();
        void LoadResources();

        void HandleExtAddonLoaded(int* signature);
        void HandleExtAddonUnloaded(int* signature);

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

        bool m_Running = false;
        bool m_ManualSyncRequested = false;

        std::thread m_Worker;
        std::mutex m_WorkerMutex;
        std::condition_variable m_WorkerWake;
    };
}