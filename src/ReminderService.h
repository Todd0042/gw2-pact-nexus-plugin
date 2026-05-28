#pragma once
#include "SharedState.h"
#include <string>
#include <unordered_map>

namespace LegendaryImpactEventmanager
{
    class ReminderService
    {
    public:
        explicit ReminderService(SharedState& sharedState);
        void CheckEventReminders(const PluginState& state);
        void ShowReminder(const std::string& title, const std::string& date, const std::string& tag, int minutesUntilStart);
        void CheckNewEventAnnouncements(const PluginState& state);
        void ShowNewEventsAnnouncement(const std::vector<EventItem>& events);
        void Render();

    private:
        SharedState& m_SharedState;
        bool m_ShowReminderMessage = false;
        std::string m_ReminderTitle;
        std::string m_ReminderDate;
        std::string m_ReminderTag;
        std::string m_ReminderTimeLeft;
        std::unordered_map<std::string, std::time_t> m_ReminderLastShown;
        bool m_ShowNewEventsMessage = false;
        std::vector<EventItem> m_NewEvents;
        std::unordered_map<std::string, bool> m_NewEventAnnouncementShown;
    };
}
