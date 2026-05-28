#pragma once

#include "Models.h"
#include <atomic>
#include <memory>

namespace LegendaryImpactEventmanager
{
    class SharedState
    {
    public:
        SharedState();

        std::shared_ptr<const PluginConfig> GetConfig() const;
        void SetConfig(std::shared_ptr<const PluginConfig> config);

        std::shared_ptr<const PluginState> GetState() const;
        void SetState(std::shared_ptr<const PluginState> state);

        bool IsWindowShown() const;
        void SetWindowShown(bool value);
        void ToggleWindowShown();

        bool IsFetching() const;
        bool TryBeginFetch();
        void EndFetch();

    private:
        std::atomic<std::shared_ptr<const PluginConfig>> m_Config;
        std::atomic<std::shared_ptr<const PluginState>> m_State;

        std::atomic<bool> m_ShowWindow{ true };
        std::atomic<bool> m_Fetching{ false };
    };
}