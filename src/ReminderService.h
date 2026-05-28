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
        void ShowReminder(const std::string& title, const std::string& date, int minutesUntilStart);
        void Render();

    private:
        SharedState& m_SharedState;
        bool m_ShowReminderMessage = false;
        std::string m_ReminderTitle;
        std::string m_ReminderDate;
        std::string m_ReminderTimeLeft;
        std::unordered_map<std::string, std::time_t> m_ReminderLastShown;
    };
}
