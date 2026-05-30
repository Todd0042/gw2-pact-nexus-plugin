#include "SharedState.h"
#include <utility>

namespace LegendaryImpactEventmanager
{
    SharedState::SharedState() = default;

    std::shared_ptr<const PluginConfig> SharedState::GetConfig() const
    {
        std::shared_lock lock(m_ConfigMutex);
        return std::make_shared<PluginConfig>(m_Config);
    }

    void SharedState::SetConfig(std::shared_ptr<const PluginConfig> config)
    {
        if (!config) return;

        std::unique_lock lock(m_ConfigMutex);
        m_Config = *config;
    }

    std::shared_ptr<const PluginState> SharedState::GetState() const
    {
        return std::make_shared<PluginState>(GetStateCopy());
    }

    PluginState SharedState::GetStateCopy() const
    {
        std::shared_lock lock(m_StateMutex);
        return m_State;
    }

    void SharedState::WithStateRead(const std::function<void(const PluginState&)>& reader) const
    {
        std::shared_lock lock(m_StateMutex);
        reader(m_State);
    }

    void SharedState::UpdateState(const std::function<void(PluginState&)>& updater)
    {
        std::unique_lock lock(m_StateMutex);
        updater(m_State);
    }

    bool SharedState::IsWindowShown() const
    {
        return m_ShowWindow;
    }

    bool* SharedState::GetWindowShownPtr()
    {
        return &m_ShowWindow;
    }

    void SharedState::SetWindowShown(bool value)
    {
        m_ShowWindow = value;
    }

    void SharedState::ToggleWindowShown()
    {
        m_ShowWindow = !m_ShowWindow;
    }

    bool SharedState::IsReminderWindowShown() const
    {
        std::shared_lock lock(m_ReminderMutex);
        return m_ShowReminderMessage;
    }

    std::vector<EventItem> SharedState::GetReminderEvents() const
    {
        std::shared_lock lock(m_ReminderMutex);
        return m_UpcommingEvents;
    }

    void SharedState::SetReminderEvents(const std::vector<EventItem>& events)
    {
        std::unique_lock lock(m_ReminderMutex);
        m_UpcommingEvents = events;
        m_ShowReminderMessage = !m_UpcommingEvents.empty();
    }

    void SharedState::CloseReminderWindow()
    {
        std::unique_lock lock(m_ReminderMutex);
        m_ShowReminderMessage = false;
        m_UpcommingEvents.clear();
        m_UpcommingEvents.shrink_to_fit();
    }

    bool SharedState::IsNewEventsWindowShown() const
    {
        std::shared_lock lock(m_ReminderMutex);
        return m_ShowNewEventsMessage;
    }

    std::vector<EventItem> SharedState::GetNewEvents() const
    {
        std::shared_lock lock(m_ReminderMutex);
        return m_NewEvents;
    }

    void SharedState::SetNewEvents(const std::vector<EventItem>& events)
    {
        std::unique_lock lock(m_ReminderMutex);
        m_NewEvents = events;
        m_ShowNewEventsMessage = !m_NewEvents.empty();
    }

    void SharedState::CloseNewEventsWindow()
    {
        std::unique_lock lock(m_ReminderMutex);
        m_ShowNewEventsMessage = false;
        m_NewEvents.clear();
        m_NewEvents.shrink_to_fit();
    }

    void SharedState::CloseAllReminderWindows()
    {
        CloseReminderWindow();
        CloseNewEventsWindow();
    }

    bool SharedState::IsAnyReminderWindowOpen() const
    {
        std::shared_lock lock(m_ReminderMutex);
        return m_ShowReminderMessage || m_ShowNewEventsMessage;
    }

    void SharedState::CleanupReminderLastShown(const std::unordered_set<std::string>& activeEventIds)
    {
        std::unique_lock lock(m_ReminderMutex);
        std::erase_if(m_ReminderLastShown, [&](const auto& item) {
            return !activeEventIds.contains(item.first);
            });
    }

    bool SharedState::MarkReminderShownIfAllowed(const std::string& eventId, std::time_t now, int repeatSeconds)
    {
        if (eventId.empty()) return false;

        std::unique_lock lock(m_ReminderMutex);

        const auto reminderIt = m_ReminderLastShown.find(eventId);
        if (reminderIt != m_ReminderLastShown.end() &&
            reminderIt->second > 0 &&
            std::difftime(now, reminderIt->second) < repeatSeconds)
        {
            return false;
        }

        m_ReminderLastShown[eventId] = now;
        return true;
    }

    void SharedState::CleanupNewEventAnnouncementShown(const std::unordered_set<std::string>& activeEventIds)
    {
        std::unique_lock lock(m_ReminderMutex);
        std::erase_if(m_NewEventAnnouncementShown, [&](const auto& item) {
            return !activeEventIds.contains(item.first);
            });
    }

    bool SharedState::MarkNewEventAnnouncementShownIfNeeded(const std::string& eventId)
    {
        if (eventId.empty()) return false;

        std::unique_lock lock(m_ReminderMutex);

        if (m_NewEventAnnouncementShown.find(eventId) != m_NewEventAnnouncementShown.end())
        {
            return false;
        }

        m_NewEventAnnouncementShown[eventId] = true;
        return true;
    }

    bool SharedState::IsFetching() const
    {
        return m_Fetching.load(std::memory_order_acquire);
    }

    bool SharedState::TryBeginFetch()
    {
        bool expected = false;

        return m_Fetching.compare_exchange_strong(
            expected,
            true,
            std::memory_order_acq_rel,
            std::memory_order_acquire);
    }

    void SharedState::EndFetch()
    {
        m_Fetching.store(false, std::memory_order_release);
    }
}
