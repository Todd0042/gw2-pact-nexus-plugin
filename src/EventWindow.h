#pragma once

#include "imgui/imgui.h"
#include "ConfigStore.h"
#include "ReminderService.h"
#include "SharedState.h"
#include "RTAPI/RTAPI.hpp"
#include "nexus/Nexus.h"
#include "SquadManager.h"
#include <functional>
#include <string>
#include <chrono>

namespace LegendaryImpactEventmanager
{
    class EventWindow
    {
    public:
        EventWindow(
            AddonAPI_t*& api, 
            SharedState& sharedState,
            ConfigStore& configStore, 
            ReminderService& reminderService,
            RTAPI::RealTimeData*& rtApi,
            std::function<void()> syncNow
        );
        void RenderAddonWindow();
        void RenderOptions();

    private:
        void RenderEventsWindow(const PluginState& state);
        void RenderEventAttendeesTable(const EventItem& event, const PluginState& state);
        void RenderRoleWithBoon(const std::string& role, const std::string& boon);
        void RenderBoonIcon(const std::string& boon);
        void RenderMarkdownText(const std::string& text);
        void RenderRtApiStatus();
        void RenderStatusIcon(bool active);
        void RenderEventTitle(const EventItem& event);

        std::chrono::steady_clock::time_point m_LastManualSync{};
        std::string ViewerLabel(const PluginState& state) const;
        ImVec4 RoleColor(const std::string& role) const;

        // Auto chat-send: types a command into the GW2 chat box by injecting
        // WndProc messages straight to the game (no manual copy/paste). The
        // game needs a frame or two between opening chat and accepting text,
        // so this runs as a small state machine ticked once per render frame
        // on the game's main thread (the only safe place for WndProc injection).
        enum class ChatSendStage { Idle, OpenChat, TypeText, Submit };
        void QueueChatCommand(const std::string& command);
        void TickChatSender();
        void SendKeyToGame(WORD virtualKey, bool keyUp);
        void SendCharToGame(char character);
        HWND ResolveGameWindow();

        ChatSendStage m_ChatStage = ChatSendStage::Idle;
        std::string m_ChatPending;
        std::chrono::steady_clock::time_point m_ChatStageAt{};
        HWND m_GameWindow = nullptr;

        AddonAPI_t*& m_Api;
        RTAPI::RealTimeData*& m_RtApi;
        SharedState& m_SharedState;
        ConfigStore& m_ConfigStore;
        ReminderService& m_ReminderService;
        std::function<void()> m_SyncNow;
    };
}
