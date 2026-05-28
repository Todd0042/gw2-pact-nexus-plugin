#include "ReminderService.h"
#include "Utility.h"
#include "imgui/imgui.h"
#include <mmsystem.h>
#pragma comment(lib, "winmm.lib")

namespace LegendaryImpactEventmanager
{
    ReminderService::ReminderService(SharedState& sharedState) : m_SharedState(sharedState) {}

    void ReminderService::ShowReminder(const std::string& title, const std::string& date, int minutesUntilStart)
    {
        m_ReminderTitle = Utility::UiText(title);
        m_ReminderDate = Utility::UiText(date);
        m_ReminderTimeLeft = "ca. " + std::to_string(minutesUntilStart) + " Minuten";
        m_ShowReminderMessage = true;
        PlaySoundA("SystemExclamation", nullptr, SND_ALIAS | SND_ASYNC);
    }

    void ReminderService::CheckEventReminders(const PluginState& state)
    {
        auto config = m_SharedState.GetConfig();
        if (!config || !config->reminderEnabled) return;

        std::time_t now = std::time(nullptr);
        int beforeSeconds = config->reminderMinutesBefore * 60;
        int repeatSeconds = config->reminderRepeatMinutes * 60;

        for (const auto& event : state.events)
        {
            std::time_t startTime = 0;
            if (!Utility::ParseIsoUtc(event.start, startTime)) continue;

            int secondsUntilStart = static_cast<int>(std::difftime(startTime, now));
            if (secondsUntilStart < 0 || secondsUntilStart > beforeSeconds) continue;

            std::time_t lastShown = m_ReminderLastShown[event.id];
            if (lastShown > 0 && std::difftime(now, lastShown) < repeatSeconds) continue;

            m_ReminderLastShown[event.id] = now;
            int minutesUntilStart = secondsUntilStart / 60;
            if (minutesUntilStart < 1) minutesUntilStart = 1;

            ShowReminder(event.title, Utility::FormatGermanDateTime(event.start), minutesUntilStart);
            break;
        }
    }

    void ReminderService::Render()
    {
        if (!m_ShowReminderMessage) return;

        ImGui::SetNextWindowSize(ImVec2(460.0f, 0.0f), ImGuiCond_Appearing);
        ImGui::SetNextWindowPos(ImVec2(40.0f, 120.0f), ImGuiCond_Appearing);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(18.0f, 16.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(10.0f, 10.0f));

        if (ImGui::Begin("Legendary Impact - Eventmanager###LegendaryImpactReminder", &m_ShowReminderMessage, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoCollapse))
        {
            ImGui::TextColored(ImVec4(1.0f, 0.82f, 0.35f, 1.0f), "Reminder: Ein Event startet bald!");
            ImGui::Separator();
            ImGui::TextDisabled("Event:"); ImGui::SameLine(); ImGui::TextWrapped("%s", m_ReminderTitle.c_str());
            ImGui::TextDisabled("Datum:"); ImGui::SameLine(); ImGui::TextWrapped("%s", m_ReminderDate.c_str());
            ImGui::TextDisabled("Zeit bis Start:"); ImGui::SameLine(); ImGui::TextColored(ImVec4(0.95f, 0.80f, 0.35f, 1.0f), "%s", m_ReminderTimeLeft.c_str());
            ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();

            float buttonWidth = 120.0f;
            float windowWidth = ImGui::GetWindowSize().x;
            float cursorX = (windowWidth - buttonWidth) * 0.5f;
            if (cursorX > 0.0f) ImGui::SetCursorPosX(cursorX);
            if (ImGui::Button("OK", ImVec2(buttonWidth, 0.0f))) m_ShowReminderMessage = false;
        }
        ImGui::End();
        ImGui::PopStyleVar(2);
    }
}
