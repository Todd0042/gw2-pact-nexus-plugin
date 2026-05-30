#pragma once
#include "SharedState.h"
#include <string>
#include <vector>

namespace LegendaryImpactEventmanager
{
    class ReminderService
    {
    public:
        explicit ReminderService(SharedState& sharedState);

        void CheckEventReminders(const PluginState& state);
        void CheckNewEventAnnouncements(const PluginState& state);

        void ShowReminder(
            const std::string& title,
            const std::string& date,
            const std::string& tag,
            int minutesUntilStart);

        void ShowNewEventsAnnouncement(const std::vector<EventItem>& events);

        void Render();

    private:
        SharedState& m_SharedState;

        void RenderEventListTable(
            const char* childId,
            const char* tableId,
            const std::vector<EventItem>& events,
            float listHeight,
            bool showTimeRemaining);

        float CalculatePopupListHeight(std::size_t eventCount) const;
    };
}