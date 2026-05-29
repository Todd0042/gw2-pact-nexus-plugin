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
        return m_ShowWindow.load(std::memory_order_relaxed);
    }

    void SharedState::SetWindowShown(bool value)
    {
        m_ShowWindow.store(value, std::memory_order_relaxed);
    }

    void SharedState::ToggleWindowShown()
    {
        bool oldValue = m_ShowWindow.load(std::memory_order_relaxed);

        while (!m_ShowWindow.compare_exchange_weak(
            oldValue,
            !oldValue,
            std::memory_order_relaxed,
            std::memory_order_relaxed)) {}
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
