#include "SharedState.h"
#include <utility>

namespace LegendaryImpactEventmanager
{
    SharedState::SharedState()
    {
        m_Config.store(std::make_shared<PluginConfig>(), std::memory_order_release);
        m_State.store(std::make_shared<PluginState>(), std::memory_order_release);
    }

    std::shared_ptr<const PluginConfig> SharedState::GetConfig() const
    {
        return m_Config.load(std::memory_order_acquire);
    }

    void SharedState::SetConfig(std::shared_ptr<const PluginConfig> config)
    {
        m_Config.store(std::move(config), std::memory_order_release);
    }

    std::shared_ptr<const PluginState> SharedState::GetState() const
    {
        return m_State.load(std::memory_order_acquire);
    }

    void SharedState::UpdateState(const std::function<void(PluginState&)>& updater)
    {
        auto current = m_State.load(std::memory_order_acquire);

        while (current) {
            auto next = std::make_shared<PluginState>(*current);
            updater(*next);

            if (m_State.compare_exchange_weak(
                current,
                next,
                std::memory_order_acq_rel,
                std::memory_order_acquire)) {
                return;
            }
        }
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