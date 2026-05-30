#pragma once

#include "Models.h"
#include <atomic>
#include <memory>
#include <functional>
#include <shared_mutex>
#include <mutex>
#include <unordered_map>
#include <unordered_set>
#include <ctime>
#include <vector>
#include <string>

namespace LegendaryImpactEventmanager
{
    class SharedState
    {
    public:
        SharedState();

        std::shared_ptr<const PluginConfig> GetConfig() const;
        void SetConfig(std::shared_ptr<const PluginConfig> config);

        // Compatibility snapshot. Prefer WithStateRead for hot UI paths.
        std::shared_ptr<const PluginState> GetState() const;
        PluginState GetStateCopy() const;
        void WithStateRead(const std::function<void(const PluginState&)>& reader) const;
        void UpdateState(const std::function<void(PluginState&)>& updater);

        bool IsWindowShown() const;
        bool* GetWindowShownPtr();
        void SetWindowShown(bool value);
        void ToggleWindowShown();

        bool IsFetching() const;
        bool TryBeginFetch();
        void EndFetch();

        bool IsReminderWindowShown() const;
        std::vector<EventItem> GetReminderEvents() const;
        void SetReminderEvents(const std::vector<EventItem>& events);
        void CloseReminderWindow();

        bool IsNewEventsWindowShown() const;
        std::vector<EventItem> GetNewEvents() const;
        void SetNewEvents(const std::vector<EventItem>& events);
        void CloseNewEventsWindow();
        void CloseAllReminderWindows();
        bool IsAnyReminderWindowOpen() const;

        void CleanupReminderLastShown(const std::unordered_set<std::string>& activeEventIds);
        bool MarkReminderShownIfAllowed(const std::string& eventId, std::time_t now, int repeatSeconds);

        void CleanupNewEventAnnouncementShown(const std::unordered_set<std::string>& activeEventIds);
        bool MarkNewEventAnnouncementShownIfNeeded(const std::string& eventId);

    private:
        std::atomic<bool> m_Fetching{ false };

        mutable std::shared_mutex m_ConfigMutex;
        PluginConfig m_Config;

        mutable std::shared_mutex m_StateMutex;
        PluginState m_State;

        bool m_ShowWindow = true;

        mutable std::shared_mutex m_ReminderMutex;
        bool m_ShowReminderMessage = false;
    
        std::vector<EventItem> m_UpcommingEvents;
        std::unordered_map<std::string, std::time_t> m_ReminderLastShown;
        bool m_ShowNewEventsMessage = false;
        std::vector<EventItem> m_NewEvents;
        std::unordered_map<std::string, bool> m_NewEventAnnouncementShown;
    };
}
