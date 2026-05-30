#pragma once
#include "SharedState.h"
#include <array>

namespace LegendaryImpactEventmanager
{
    class ConfigStore
    {
    public:
        explicit ConfigStore(SharedState& sharedState);

        void Load();
        void Save() const;
        void ApplyFromEditBuffer();

        char* TokenBuffer();
        int& RefreshMinutes();
        bool& ReminderEnabled();
        bool& AnnounceNewEventsEnabled();
        int& ReminderMinutesBefore();
        int& ReminderRepeatMinutes();

    private:
        SharedState& m_SharedState;
        std::array<char, 512> m_EditToken{};
        int m_EditRefreshMinutes = 5;
        bool m_EditReminderEnabled = true;
        bool m_EditAnnounceNewEventsEnabled = true;
        int m_EditReminderMinutesBefore = 15;
        int m_EditReminderRepeatMinutes = 5;
    };
}
