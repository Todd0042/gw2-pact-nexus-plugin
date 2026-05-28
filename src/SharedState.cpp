#include "SharedState.h"

namespace LegendaryImpactEventmanager
{
    SharedState::SharedState()
    {
        m_Config.store(std::make_shared<PluginConfig>());
        m_State.store(std::make_shared<PluginState>());
    }

    std::shared_ptr<PluginConfig> SharedState::GetConfig() const
    {
        return m_Config.load();
    }

    void SharedState::SetConfig(std::shared_ptr<PluginConfig> config)
    {
        m_Config.store(config);
    }

    std::shared_ptr<PluginState> SharedState::GetState() const
    {
        return m_State.load();
    }

    void SharedState::SetState(std::shared_ptr<PluginState> state)
    {
        m_State.store(state);
    }

    bool SharedState::IsWindowShown() const
    {
        return m_ShowWindow.load();
    }

    void SharedState::SetWindowShown(bool value)
    {
        m_ShowWindow.store(value);
    }

    void SharedState::ToggleWindowShown()
    {
        m_ShowWindow.store(!m_ShowWindow.load());
    }

    bool SharedState::IsFetching() const
    {
        return m_Fetching.load();
    }

    bool SharedState::TryBeginFetch()
    {
        bool expected = false;
        return m_Fetching.compare_exchange_strong(expected, true);
    }

    void SharedState::EndFetch()
    {
        m_Fetching.store(false);
    }
}