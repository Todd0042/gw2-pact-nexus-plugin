#pragma once

#include "imgui/imgui.h"
#include "ConfigStore.h"
#include "ReminderService.h"
#include "SharedState.h"
#include "RTAPI/RTAPI.hpp"
#include "nexus/Nexus.h"
#include "SquadManager.h"
#include <functional>
#include <string>
#include <chrono>

namespace LegendaryImpactEventmanager
{
    class EventWindow
    {
    public:
        EventWindow(
            AddonAPI_t*& api, 
            SharedState& sharedState,
            ConfigStore& configStore, 
            ReminderService& reminderService,
            RTAPI::RealTimeData*& rtApi,
            std::function<void()> syncNow
        );
        void RenderAddonWindow();
        void RenderOptions();

    private:
        void RenderEventsWindow(const PluginState& state);
        void RenderEventAttendeesTable(const EventItem& event, const PluginState& state);
        void RenderRoleWithBoon(const std::string& role, const std::string& boon);
        void RenderBoonIcon(const std::string& boon);
        void RenderMarkdownText(const std::string& text);
        void RenderRtApiStatus();
        void RenderStatusIcon(bool active);
        void RenderEventTitle(const EventItem& event);

        std::chrono::steady_clock::time_point m_LastManualSync{};
        std::string ViewerLabel(const PluginState& state) const;
        ImVec4 RoleColor(const std::string& role) const;

        AddonAPI_t*& m_Api;
        RTAPI::RealTimeData*& m_RtApi;
        SharedState& m_SharedState;
        ConfigStore& m_ConfigStore;
        ReminderService& m_ReminderService;
        std::function<void()> m_SyncNow;
    };
}
