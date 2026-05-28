#pragma once

#include "imgui/imgui.h"
#include "ConfigStore.h"
#include "ReminderService.h"
#include "SharedState.h"
#include "nexus/Nexus.h"
#include <functional>
#include <string>

namespace LegendaryImpactEventmanager
{
    class EventWindow
    {
    public:
        EventWindow(AddonAPI*& api, SharedState& sharedState, ConfigStore& configStore, ReminderService& reminderService, std::function<void()> syncNow);
        void RenderAddonWindow();
        void RenderOptions();

    private:
        void RenderEventsWindow();
        void RenderEventAttendeesTable(const EventItem& event);
        void RenderRoleWithBoon(const std::string& role, const std::string& boon);
        void RenderBoonIcon(const std::string& boon);
        void RenderMarkdownText(const std::string& text);
        std::string ViewerLabel(const PluginState& state) const;
        ImVec4 RoleColor(const std::string& role) const;
        ImVec4 TagColor(const std::string& tag) const;

        AddonAPI*& m_Api;
        SharedState& m_SharedState;
        ConfigStore& m_ConfigStore;
        ReminderService& m_ReminderService;
        std::function<void()> m_SyncNow;
    };
}
