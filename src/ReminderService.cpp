#include "ReminderService.h"
#include "Utility.h"
#include "imgui/imgui.h"
#include <mmsystem.h>
#include <algorithm>
#include <unordered_set>
#pragma comment(lib, "winmm.lib")

namespace LegendaryImpactEventmanager
{
    ReminderService::ReminderService(SharedState& sharedState) : m_SharedState(sharedState) {}

    void ReminderService::ShowReminder(const std::string& title, const std::string& date, const std::string& tag, int minutesUntilStart)
    {
        m_ReminderTitle = Utility::UiText(title);
        m_ReminderDate = Utility::UiText(date);
        m_ReminderTag = tag;
        m_ReminderTimeLeft = "ca. " + std::to_string(minutesUntilStart) + " Minuten";
        m_ShowReminderMessage = true;
        PlaySoundA("SystemExclamation", nullptr, SND_ALIAS | SND_ASYNC);
    }

    void ReminderService::CheckEventReminders(const PluginState& state)
    {
        auto config = m_SharedState.GetConfig();
        if (!config || !config->reminderEnabled) return;

        std::unordered_set<std::string> activeEventIds;

        for (const auto& event : state.events)
        {
            if (!event.id.empty())
            {
                activeEventIds.insert(event.id);
            }
        }

        std::erase_if(m_ReminderLastShown, [&](const auto& item) {
            return !activeEventIds.contains(item.first);
            });

        std::time_t now = std::time(nullptr);

        const int beforeSeconds = config->reminderMinutesBefore * 60;
        const int repeatSeconds = config->reminderRepeatMinutes * 60;

        for (const auto& event : state.events)
        {
            if (!event.isViewerAttending) continue;
            if (event.id.empty()) continue;

            std::time_t startTime = 0;

            if (!Utility::ParseIsoUtc(event.start, startTime)) continue;

            const int secondsUntilStart =
                static_cast<int>(std::difftime(startTime, now));

            if (secondsUntilStart < 0 || secondsUntilStart > beforeSeconds) continue;

            const auto reminderIt = m_ReminderLastShown.find(event.id);

            if (reminderIt != m_ReminderLastShown.end() &&
                reminderIt->second > 0 &&
                std::difftime(now, reminderIt->second) < repeatSeconds)
            {
                continue;
            }

            m_ReminderLastShown[event.id] = now;

            int minutesUntilStart = secondsUntilStart / 60;
            if (minutesUntilStart < 1) minutesUntilStart = 1;

            ShowReminder(
                event.title,
                Utility::FormatGermanDateTime(event.start),
                event.tag,
                minutesUntilStart);

            break;
        }
    }

    void ReminderService::Render()
    {
        if (m_ShowReminderMessage)
        {
            const float minWidth = 500.0f;
            const float maxWidth = 780.0f;
            const float paddingWidth = 180.0f;

            float contentWidth = minWidth;
            contentWidth = (std::max)(contentWidth, ImGui::CalcTextSize(m_ReminderTitle.c_str()).x + paddingWidth);
            contentWidth = (std::max)(contentWidth, ImGui::CalcTextSize(m_ReminderDate.c_str()).x + paddingWidth);
            contentWidth = (std::min)(contentWidth, maxWidth);

            ImGui::SetNextWindowSize(ImVec2(contentWidth, 0.0f), ImGuiCond_Appearing);
            ImGui::SetNextWindowPos(ImVec2(40.0f, 120.0f), ImGuiCond_FirstUseEver);

            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(18.0f, 16.0f));
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(10.0f, 10.0f));
            ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(8.0f, 6.0f));

            if (ImGui::Begin(
                "Legendary Impact - Eventmanager###LegendaryImpactReminder",
                &m_ShowReminderMessage,
                ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoCollapse))
            {
                ImGui::TextColored(
                    ImVec4(1.0f, 0.82f, 0.35f, 1.0f),
                    "Reminder: Ein Event startet bald!");

                ImGui::Separator();

                if (ImGui::BeginTable(
                    "reminderDetailsTable",
                    2,
                    ImGuiTableFlags_RowBg |
                    ImGuiTableFlags_SizingStretchProp))
                {
                    ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, 105.0f);
                    ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::TextDisabled("Event");
                    ImGui::TableSetColumnIndex(1);
                    ImGui::TextWrapped("%s", m_ReminderTitle.c_str());

                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::TextDisabled("Datum");
                    ImGui::TableSetColumnIndex(1);
                    ImGui::TextWrapped("%s", m_ReminderDate.c_str());

                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::TextDisabled("Tag");
                    ImGui::TableSetColumnIndex(1);
                    if (!m_ReminderTag.empty())
                    {
                        ImGui::TextColored(
                            Utility::TagColor(m_ReminderTag),
                            "%s",
                            Utility::TagLabel(m_ReminderTag).c_str());
                    }
                    else
                    {
                        ImGui::TextDisabled("-");
                    }

                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::TextDisabled("Startet in");
                    ImGui::TableSetColumnIndex(1);
                    ImGui::TextColored(
                        ImVec4(0.95f, 0.80f, 0.35f, 1.0f),
                        "%s",
                        m_ReminderTimeLeft.c_str());

                    ImGui::EndTable();
                }

                ImGui::Spacing();
                ImGui::Separator();
                ImGui::Spacing();

                const float buttonWidth = 140.0f;
                const float windowWidth = ImGui::GetWindowSize().x;
                const float cursorX = (windowWidth - buttonWidth) * 0.5f;

                if (cursorX > 0.0f)
                {
                    ImGui::SetCursorPosX(cursorX);
                }

                if (ImGui::Button("OK", ImVec2(buttonWidth, 0.0f)))
                {
                    m_ShowReminderMessage = false;
                }
            }

            ImGui::End();

            ImGui::PopStyleVar(3);
        }

        if (m_ShowNewEventsMessage)
        {
            const float minWidth = 560.0f;
            const float maxWidth = 900.0f;
            const float paddingWidth = 260.0f;

            float contentWidth = minWidth;

            for (const auto& event : m_NewEvents)
            {
                const std::string title = Utility::UiText(event.title);
                const ImVec2 titleSize = ImGui::CalcTextSize(title.c_str());

                contentWidth = (std::max)(contentWidth, titleSize.x + paddingWidth);
            }

            contentWidth = (std::min)(contentWidth, maxWidth);

            const float rowHeight = ImGui::GetTextLineHeightWithSpacing() + 8.0f;
            const float listHeight = (std::min)(
                260.0f,
                rowHeight * static_cast<float>(m_NewEvents.size() + 1));

            const float windowHeight = 120.0f + listHeight + 60.0f;

            ImGui::SetNextWindowSize(
                ImVec2(contentWidth, windowHeight),
                ImGuiCond_Appearing);

            ImGui::SetNextWindowPos(
                ImVec2(40.0f, 120.0f),
                ImGuiCond_FirstUseEver);

            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(18.0f, 16.0f));
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(10.0f, 10.0f));
            ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(8.0f, 5.0f));

            if (ImGui::Begin(
                "Legendary Impact - Eventmanager###LegendaryImpactNewEvents",
                &m_ShowNewEventsMessage,
                ImGuiWindowFlags_NoCollapse))
            {
                ImGui::TextColored(
                    ImVec4(1.0f, 0.82f, 0.35f, 1.0f),
                    "Es sind neue Events verfuegbar!");

                ImGui::TextDisabled(
                    "%d neue%s Event%s gefunden.",
                    static_cast<int>(m_NewEvents.size()),
                    m_NewEvents.size() == 1 ? "s" : "",
                    m_NewEvents.size() == 1 ? "" : "s");

                ImGui::Separator();

                if (ImGui::BeginChild(
                    "newEventsList",
                    ImVec2(0.0f, listHeight),
                    false))
                {
                    if (ImGui::BeginTable(
                        "newEventsTable",
                        3,
                        ImGuiTableFlags_RowBg |
                        ImGuiTableFlags_SizingStretchProp |
                        ImGuiTableFlags_ScrollY))
                    {
                        ImGui::TableSetupColumn(
                            "Event",
                            ImGuiTableColumnFlags_WidthStretch,
                            2.2f);

                        ImGui::TableSetupColumn(
                            "Datum",
                            ImGuiTableColumnFlags_WidthStretch,
                            1.5f);

                        ImGui::TableSetupColumn(
                            "Tag",
                            ImGuiTableColumnFlags_WidthFixed,
                            110.0f);

                        ImGui::TableHeadersRow();

                        for (const auto& event : m_NewEvents)
                        {
                            ImGui::TableNextRow();

                            ImGui::TableSetColumnIndex(0);

                            ImGui::TextWrapped(
                                "%s",
                                Utility::UiText(event.title).c_str());

                            ImGui::TableSetColumnIndex(1);

                            if (!event.start.empty())
                            {
                                ImGui::TextWrapped(
                                    "%s",
                                    Utility::FormatGermanDateTime(event.start).c_str());
                            }
                            else
                            {
                                ImGui::TextDisabled("-");
                            }

                            ImGui::TableSetColumnIndex(2);

                            if (!event.tag.empty())
                            {
                                ImGui::TextColored(
                                    Utility::TagColor(event.tag),
                                    "%s",
                                    Utility::TagLabel(event.tag).c_str());
                            }
                            else
                            {
                                ImGui::TextDisabled("-");
                            }
                        }

                        ImGui::EndTable();
                    }
                }

                ImGui::EndChild();

                ImGui::Spacing();
                ImGui::Separator();
                ImGui::Spacing();

                const float buttonWidth = 140.0f;
                const float windowWidth = ImGui::GetWindowSize().x;
                const float cursorX = (windowWidth - buttonWidth) * 0.5f;

                if (cursorX > 0.0f)
                {
                    ImGui::SetCursorPosX(cursorX);
                }

                if (ImGui::Button("OK", ImVec2(buttonWidth, 0.0f)))
                {
                    m_ShowNewEventsMessage = false;
                }
            }

            ImGui::End();

            ImGui::PopStyleVar(3);
        }
    }

    void ReminderService::CheckNewEventAnnouncements(const PluginState& state)
    {
        auto config = m_SharedState.GetConfig();

        if (!config || !config->announceNewEventsEnabled)
        {
            return;
        }

        std::unordered_set<std::string> activeEventIds;

        for (const auto& event : state.events)
        {
            if (!event.id.empty())
            {
                activeEventIds.insert(event.id);
            }
        }

        std::erase_if(m_NewEventAnnouncementShown, [&](const auto& item) {
            return !activeEventIds.contains(item.first);
            });

        std::unordered_set<std::string> newEventIds(
            state.newEventIds.begin(),
            state.newEventIds.end());

        std::vector<EventItem> eventsToShow;

        for (const auto& event : state.events)
        {
            if (event.id.empty() || !newEventIds.contains(event.id))
            {
                continue;
            }

            if (m_NewEventAnnouncementShown.find(event.id) != m_NewEventAnnouncementShown.end())
            {
                continue;
            }

            m_NewEventAnnouncementShown[event.id] = true;
            eventsToShow.push_back(event);
        }

        if (!eventsToShow.empty())
        {
            ShowNewEventsAnnouncement(eventsToShow);
        }
    }

    void ReminderService::ShowNewEventsAnnouncement(const std::vector<EventItem>& events)
    {
        m_NewEvents = events;
        m_ShowNewEventsMessage = true;

        PlaySoundA("SystemExclamation", nullptr, SND_ALIAS | SND_ASYNC);
    }
}
