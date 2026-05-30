#include "ReminderService.h"
#include "Utility.h"
#include "imgui/imgui.h"
#include <mmsystem.h>
#include <algorithm>
#include <unordered_set>

#pragma comment(lib, "winmm.lib")

namespace LegendaryImpactEventmanager
{
    ReminderService::ReminderService(SharedState& sharedState)
        : m_SharedState(sharedState)
    {
    }

    void ReminderService::ShowReminder(
        const std::string& title,
        const std::string& date,
        const std::string& tag,
        int minutesUntilStart)
    {
        EventItem event;
        event.title = Utility::UiText(title);
        event.start = date;
        event.tag = tag;
        event.attendeeCount = minutesUntilStart;

        m_SharedState.SetReminderEvents({ event });

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

        m_SharedState.CleanupReminderLastShown(activeEventIds);

        const std::time_t now = std::time(nullptr);
        const int beforeSeconds = config->reminderMinutesBefore * 60;
        const int repeatSeconds = config->reminderRepeatMinutes * 60;

        std::vector<EventItem> reminderEvents;

        for (const auto& event : state.events)
        {
            if (!event.isViewerAttending) continue;
            if (event.id.empty()) continue;

            std::time_t startTime = 0;

            if (!Utility::ParseIsoUtc(event.start, startTime)) continue;

            const int secondsUntilStart =
                static_cast<int>(std::difftime(startTime, now));

            if (secondsUntilStart < 0 || secondsUntilStart > beforeSeconds) continue;

            if (!m_SharedState.MarkReminderShownIfAllowed(event.id, now, repeatSeconds))
            {
                continue;
            }

            reminderEvents.push_back(event);
        }

        if (!reminderEvents.empty())
        {
            m_SharedState.SetReminderEvents(reminderEvents);
            PlaySoundA("SystemExclamation", nullptr, SND_ALIAS | SND_ASYNC);
        }
    }

    void ReminderService::RenderEventListTable(
        const char* childId,
        const char* tableId,
        const std::vector<EventItem>& events,
        float listHeight)
    {
        if (ImGui::BeginChild(childId, ImVec2(0.0f, listHeight), false))
        {
            if (ImGui::BeginTable(
                tableId,
                3,
                ImGuiTableFlags_RowBg |
                ImGuiTableFlags_SizingStretchProp |
                ImGuiTableFlags_ScrollY |
                ImGuiTableFlags_Resizable))
            {
                ImGui::TableSetupColumn("Event", ImGuiTableColumnFlags_WidthStretch, 2.5f);
                ImGui::TableSetupColumn("Datum", ImGuiTableColumnFlags_WidthStretch, 1.4f);
                ImGui::TableSetupColumn("Tag", ImGuiTableColumnFlags_WidthFixed, 90.0f);
                ImGui::TableHeadersRow();

                for (const auto& event : events)
                {
                    ImGui::TableNextRow();

                    ImGui::TableSetColumnIndex(0);
                    ImGui::TextWrapped("%s", Utility::UiText(event.title).c_str());

                    ImGui::TableSetColumnIndex(1);

                    if (!event.start.empty())
                    {
                        if (event.start.find('T') != std::string::npos)
                        {
                            ImGui::TextWrapped(
                                "%s",
                                Utility::FormatGermanDateTime(event.start).c_str());
                        }
                        else
                        {
                            ImGui::TextWrapped("%s", event.start.c_str());
                        }
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
    }

    float ReminderService::CalculatePopupListHeight(std::size_t eventCount) const
    {
        const float rowHeight = ImGui::GetTextLineHeightWithSpacing() + 8.0f;
        const float headerHeight = ImGui::GetTextLineHeightWithSpacing() + 10.0f;

        constexpr float maxListHeight = 220.0f;

        return (std::min)(
            maxListHeight,
            headerHeight + rowHeight * static_cast<float>(eventCount));
    }

    void ReminderService::Render()
    {
        if (m_SharedState.IsReminderWindowShown())
        {
            std::vector<EventItem> reminderEvents = m_SharedState.GetReminderEvents();

            if (reminderEvents.empty())
            {
                m_SharedState.CloseReminderWindow();
            }
            else
            {
                const float minWidth = 700.0f;
                const float maxWidth = 1100.0f;
                const float paddingWidth = 350.0f;

                float contentWidth = minWidth;

                for (const auto& event : reminderEvents)
                {
                    const std::string title = Utility::UiText(event.title);
                    const ImVec2 titleSize = ImGui::CalcTextSize(title.c_str());

                    contentWidth = (std::max)(contentWidth, titleSize.x + paddingWidth);
                }

                contentWidth = (std::min)(contentWidth, maxWidth);

                const float listHeight = CalculatePopupListHeight(reminderEvents.size());
                const float windowHeight = 155.0f + listHeight;

                ImGui::SetNextWindowSize(
                    ImVec2(contentWidth, windowHeight),
                    ImGuiCond_Appearing);

                ImGui::SetNextWindowPos(
                    ImVec2(40.0f, 120.0f),
                    ImGuiCond_FirstUseEver);

                ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(18.0f, 16.0f));
                ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(10.0f, 10.0f));
                ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(8.0f, 5.0f));

                bool showReminderMessage = true;

                if (ImGui::Begin(
                    "Legendary Impact - Eventmanager###LegendaryImpactReminder",
                    &showReminderMessage,
                    ImGuiWindowFlags_NoCollapse))
                {
                    ImGui::TextColored(
                        ImVec4(1.0f, 0.82f, 0.35f, 1.0f),
                        reminderEvents.size() == 1
                        ? "Ein Event startet bald!"
                        : "%d Events starten bald!",
                        static_cast<int>(reminderEvents.size()));

                    ImGui::Separator();

                    RenderEventListTable(
                        "reminderEventsList",
                        "reminderEventsTable",
                        reminderEvents,
                        listHeight);

                    ImGui::Spacing();
                    ImGui::Separator();
                    ImGui::Spacing();

                    constexpr float buttonWidth = 140.0f;
                    const float windowWidth = ImGui::GetWindowSize().x;
                    const float cursorX = (windowWidth - buttonWidth) * 0.5f;

                    if (cursorX > 0.0f)
                    {
                        ImGui::SetCursorPosX(cursorX);
                    }

                    if (ImGui::Button("OK", ImVec2(buttonWidth, 0.0f)))
                    {
                        m_SharedState.CloseReminderWindow();
                    }
                }

                ImGui::End();

                if (!showReminderMessage)
                {
                    m_SharedState.CloseReminderWindow();
                }

                ImGui::PopStyleVar(3);
            }
        }

        if (m_SharedState.IsNewEventsWindowShown())
        {
            std::vector<EventItem> newEvents = m_SharedState.GetNewEvents();

            if (newEvents.empty())
            {
                m_SharedState.CloseNewEventsWindow();
                return;
            }

            const float minWidth = 700.0f;
            const float maxWidth = 1100.0f;
            const float paddingWidth = 350.0f;

            float contentWidth = minWidth;

            for (const auto& event : newEvents)
            {
                const std::string title = Utility::UiText(event.title);
                const ImVec2 titleSize = ImGui::CalcTextSize(title.c_str());

                contentWidth = (std::max)(contentWidth, titleSize.x + paddingWidth);
            }

            contentWidth = (std::min)(contentWidth, maxWidth);

            const float listHeight = CalculatePopupListHeight(newEvents.size());
            const float windowHeight = 155.0f + listHeight;

            ImGui::SetNextWindowSize(
                ImVec2(contentWidth, windowHeight),
                ImGuiCond_Appearing);

            ImGui::SetNextWindowPos(
                ImVec2(40.0f, 120.0f),
                ImGuiCond_FirstUseEver);

            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(18.0f, 16.0f));
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(10.0f, 10.0f));
            ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(8.0f, 5.0f));

            bool showNewEventsMessage = true;

            if (ImGui::Begin(
                "Legendary Impact - Eventmanager###LegendaryImpactNewEvents",
                &showNewEventsMessage,
                ImGuiWindowFlags_NoCollapse))
            {
                ImGui::TextColored(
                    ImVec4(1.0f, 0.82f, 0.35f, 1.0f),
                    newEvents.size() == 1
                    ? "Es ist ein neues Event verfuegbar!"
                    : "Es sind %d neue Events verfuegbar!",
                    static_cast<int>(newEvents.size()));

                ImGui::Separator();

                RenderEventListTable(
                    "newEventsList",
                    "newEventsTable",
                    newEvents,
                    listHeight);

                ImGui::Spacing();
                ImGui::Separator();
                ImGui::Spacing();

                constexpr float buttonWidth = 140.0f;
                const float windowWidth = ImGui::GetWindowSize().x;
                const float cursorX = (windowWidth - buttonWidth) * 0.5f;

                if (cursorX > 0.0f)
                {
                    ImGui::SetCursorPosX(cursorX);
                }

                if (ImGui::Button("OK", ImVec2(buttonWidth, 0.0f)))
                {
                    m_SharedState.CloseNewEventsWindow();
                }
            }

            ImGui::End();

            if (!showNewEventsMessage)
            {
                m_SharedState.CloseNewEventsWindow();
            }

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

        m_SharedState.CleanupNewEventAnnouncementShown(activeEventIds);

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

            if (!m_SharedState.MarkNewEventAnnouncementShownIfNeeded(event.id))
            {
                continue;
            }

            eventsToShow.push_back(event);
        }

        if (!eventsToShow.empty())
        {
            ShowNewEventsAnnouncement(eventsToShow);
        }
    }

    void ReminderService::ShowNewEventsAnnouncement(const std::vector<EventItem>& events)
    {
        m_SharedState.SetNewEvents(events);
        PlaySoundA("SystemExclamation", nullptr, SND_ALIAS | SND_ASYNC);
    }
}