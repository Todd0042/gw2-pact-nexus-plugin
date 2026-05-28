#pragma once
#include "HttpClient.h"
#include "SharedState.h"
#include <string>

namespace LegendaryImpactEventmanager
{
    class EventService
    {
    public:
        EventService(SharedState& sharedState, HttpClient& httpClient);
        void FetchEvents();
        void StoreError(const std::string& error);
        void LoadCachedEvents();

    private:
        std::string BuildEventsUrl(const PluginConfig& config) const;
        SharedState& m_SharedState;
        HttpClient& m_HttpClient;
        void SaveCachedEvents(const PluginState& state) const;
    };
}
