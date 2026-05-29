#pragma once

#include "Models.h"
#include <atomic>
#include <memory>
#include <functional>
#include <shared_mutex>
#include <mutex>

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
        void SetWindowShown(bool value);
        void ToggleWindowShown();

        bool IsFetching() const;
        bool TryBeginFetch();
        void EndFetch();

    private:
        mutable std::shared_mutex m_ConfigMutex;
        PluginConfig m_Config;

        mutable std::shared_mutex m_StateMutex;
        PluginState m_State;

        std::atomic<bool> m_ShowWindow{ true };
        std::atomic<bool> m_Fetching{ false };
    };
}
