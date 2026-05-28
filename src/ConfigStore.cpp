#include "ConfigStore.h"
#include "Constants.h"
#include <direct.h>
#include <fstream>
#include "nlohmann/json.hpp"

using json = nlohmann::json;

namespace LegendaryImpactEventmanager
{
    ConfigStore::ConfigStore(SharedState& sharedState) : m_SharedState(sharedState) {}

    void ConfigStore::Load()
    {
        std::ifstream file(Constants::SettingsFile);
        if (!file.is_open()) return;

        try
        {
            json data;
            file >> data;

            auto config = std::make_shared<PluginConfig>();
            config->token = data.value("token", "");
            config->refreshMinutes = data.value("refreshMinutes", 5);
            if (config->refreshMinutes < 5) config->refreshMinutes = 5;

            config->reminderEnabled = data.value("reminderEnabled", true);
            config->reminderMinutesBefore = data.value("reminderMinutesBefore", 15);
            config->reminderRepeatMinutes = data.value("reminderRepeatMinutes", 5);
            if (config->reminderMinutesBefore < 1) config->reminderMinutesBefore = 1;
            if (config->reminderRepeatMinutes < 1) config->reminderRepeatMinutes = 1;

            config->announceNewEventsEnabled = data.value("announceNewEventsEnabled", true);
            m_EditAnnounceNewEventsEnabled = config->announceNewEventsEnabled;

            m_SharedState.SetConfig(config);
            strcpy_s(m_EditToken.data(), m_EditToken.size(), config->token.c_str());
            m_EditRefreshMinutes = config->refreshMinutes;
            m_EditReminderEnabled = config->reminderEnabled;
            m_EditReminderMinutesBefore = config->reminderMinutesBefore;
            m_EditReminderRepeatMinutes = config->reminderRepeatMinutes;
            m_SharedState.SetWindowShown(data.value("showWindow", true));
        }
        catch (...) {}
    }

    void ConfigStore::Save() const
    {
        _mkdir("addons");
        _mkdir(Constants::SettingsDir);

        auto config = m_SharedState.GetConfig();
        json data;
        data["token"] = config ? config->token : "";
        data["refreshMinutes"] = config ? config->refreshMinutes : 5;
        data["showWindow"] = m_SharedState.IsWindowShown();
        data["reminderEnabled"] = config ? config->reminderEnabled : true;
        data["announceNewEventsEnabled"] = config ? config->announceNewEventsEnabled : true;
        data["reminderMinutesBefore"] = config ? config->reminderMinutesBefore : 15;
        data["reminderRepeatMinutes"] = config ? config->reminderRepeatMinutes : 5;

        std::ofstream file(Constants::SettingsFile);
        if (file.is_open()) file << data.dump(4);
    }

    void ConfigStore::ApplyFromEditBuffer()
    {
        auto nextConfig = std::make_shared<PluginConfig>();
        nextConfig->token = m_EditToken.data();
        nextConfig->refreshMinutes = m_EditRefreshMinutes < 5 ? 5 : m_EditRefreshMinutes;
        nextConfig->reminderEnabled = m_EditReminderEnabled;
        nextConfig->announceNewEventsEnabled = m_EditAnnounceNewEventsEnabled;
        nextConfig->reminderMinutesBefore = m_EditReminderMinutesBefore < 1 ? 1 : m_EditReminderMinutesBefore;
        nextConfig->reminderRepeatMinutes = m_EditReminderRepeatMinutes < 1 ? 1 : m_EditReminderRepeatMinutes;
        m_SharedState.SetConfig(nextConfig);
        Save();
    }

    char* ConfigStore::TokenBuffer() { return m_EditToken.data(); }
    int& ConfigStore::RefreshMinutes() { return m_EditRefreshMinutes; }
    bool& ConfigStore::ReminderEnabled() { return m_EditReminderEnabled; }
    bool& ConfigStore::AnnounceNewEventsEnabled() { return m_EditAnnounceNewEventsEnabled; }
    int& ConfigStore::ReminderMinutesBefore() { return m_EditReminderMinutesBefore; }
    int& ConfigStore::ReminderRepeatMinutes() { return m_EditReminderRepeatMinutes; }
}
