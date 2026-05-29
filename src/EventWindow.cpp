#include "EventWindow.h"
#include "Constants.h"
#include "Utility.h"
#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"
#include <shellapi.h>

namespace LegendaryImpactEventmanager
{
    EventWindow::EventWindow(AddonAPI_t*& api, SharedState& sharedState, ConfigStore& configStore, ReminderService& reminderService, RTAPI::RealTimeData*& rtApi, std::function<void()> syncNow)
        : m_Api(api), m_SharedState(sharedState), m_ConfigStore(configStore), m_ReminderService(reminderService), m_RtApi(rtApi), m_SyncNow(std::move(syncNow)) {
    }

    ImVec4 EventWindow::RoleColor(const std::string& role) const
    {
        if (role == "HEAL") return ImVec4(0.25f, 0.85f, 0.45f, 1.0f);
        if (role == "BOONDPS") return ImVec4(0.95f, 0.82f, 0.25f, 1.0f);
        if (role == "DPS") return ImVec4(1.0f, 0.35f, 0.35f, 1.0f);
        return ImVec4(0.75f, 0.75f, 0.75f, 1.0f);
    }

    std::string EventWindow::ViewerLabel(const PluginState& state) const
    {
        if (!state.viewerGw2Account.empty())
        {
            if (!state.viewerUsername.empty()) return state.viewerUsername + " [" + state.viewerGw2Account + "]";
            return state.viewerGw2Account;
        }
        return "Public";
    }

    void EventWindow::RenderRtApiStatus()
    {
        ImGui::TextUnformatted("RealTime API:");
        ImGui::SameLine();

        if (m_RtApi && m_RtApi->GameBuild != 0)
        {
            ImGui::TextColored(
                ImVec4(0.20f, 0.90f, 0.30f, 1.0f),
                "Installiert");
        }
        else
        {
            ImGui::TextColored(
                ImVec4(1.0f, 0.25f, 0.25f, 1.0f),
                "Nicht installiert");
            ImGui::SameLine();
            ImGui::TextDisabled("(eingeschraenkte Funktionalitaet)");
        }
    }

    void EventWindow::RenderBoonIcon(const std::string& boon)
    {
        const char* textureId = nullptr;
        if (boon == "QUICKNESS") textureId = Constants::QuicknessIconId;
        else if (boon == "ALACRITY") textureId = Constants::AlacrityIconId;

        if (!textureId) { ImGui::TextDisabled("-"); return; }
        Texture_t* texture = m_Api->Textures_Get(textureId);
        if (!texture || !texture->Resource) { ImGui::TextDisabled("%s", Utility::BoonLabel(boon).c_str()); return; }
        ImGui::Image((ImTextureID)texture->Resource, ImVec2(22.0f, 22.0f));
    }

    void EventWindow::RenderRoleWithBoon(const std::string& role, const std::string& boon)
    {
        const float iconSize = 22.0f;
        const float textHeight = ImGui::GetTextLineHeight();
        float startY = ImGui::GetCursorPosY();
        if (!boon.empty()) ImGui::SetCursorPosY(startY + (iconSize - textHeight) * 0.5f);
        ImGui::TextColored(RoleColor(role), "%s", Utility::RoleLabel(role).c_str());
        if (!boon.empty()) { ImGui::SameLine(); ImGui::SetCursorPosY(startY); RenderBoonIcon(boon); }
    }

    void EventWindow::RenderMarkdownText(const std::string& text)
    {
        std::string line;
        auto renderLine = [](const std::string& raw)
            {
                std::string line = Utility::StripSimpleMarkdown(raw);
                if (line.empty()) { ImGui::Spacing(); return; }
                if (line == "---") return;
                if (line.rfind("### ", 0) == 0) ImGui::TextColored(ImVec4(0.90f, 0.75f, 0.35f, 1.0f), "%s", line.substr(4).c_str());
                else if (line.rfind("## ", 0) == 0) ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.45f, 1.0f), "%s", line.substr(3).c_str());
                else if (line.rfind("# ", 0) == 0) ImGui::TextColored(ImVec4(1.0f, 0.9f, 0.55f, 1.0f), "%s", line.substr(2).c_str());
                else if (line.rfind("- ", 0) == 0 || line.rfind("* ", 0) == 0) ImGui::BulletText("%s", line.substr(2).c_str());
                else ImGui::TextWrapped("%s", line.c_str());
            };
        for (char c : text)
        {
            if (c == '\n') { renderLine(line); line.clear(); }
            else line += c;
        }
        if (!line.empty()) renderLine(line);
    }

    void EventWindow::RenderEventAttendeesTable(const EventItem& event)
    {
        if (event.attendees.empty()) { ImGui::TextDisabled("Keine Teilnehmerdaten vorhanden."); return; }
        if (ImGui::BeginTable("attendeesTable", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp))
        {
            ImGui::TableSetupColumn("Spieler"); ImGui::TableSetupColumn("Rolle"); ImGui::TableSetupColumn("Boon"); ImGui::TableSetupColumn("Flex"); ImGui::TableHeadersRow();
            for (const auto& attendee : event.attendees)
            {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                auto state = m_SharedState.GetState();
                bool isSelf = state && !state->viewerGw2Account.empty() && attendee.gw2Account == state->viewerGw2Account;
                if (isSelf) ImGui::TextColored(ImVec4(1.0f, 0.15f, 0.15f, 1.0f), "%s", Utility::DisplayUser(attendee.username, attendee.gw2Account).c_str());
                else ImGui::TextUnformatted(Utility::DisplayUser(attendee.username, attendee.gw2Account).c_str());

                ImGui::TableSetColumnIndex(1); ImGui::TextColored(RoleColor(attendee.role), "%s", Utility::RoleLabel(attendee.role).c_str());
                ImGui::TableSetColumnIndex(2); RenderBoonIcon(attendee.boon);
                ImGui::TableSetColumnIndex(3);
                if (attendee.flexRoles.empty()) ImGui::TextDisabled("-");
                else for (const auto& flex : attendee.flexRoles) RenderRoleWithBoon(flex.role, flex.boon);
            }
            ImGui::EndTable();
        }
    }

    void EventWindow::RenderEventsWindow()
    {
        auto state = m_SharedState.GetState();
        if (!state) return;

        ImGui::TextUnformatted("Angemeldet als:"); ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.95f, 0.80f, 0.35f, 1.0f), "%s", ViewerLabel(*state).c_str());
        ImGui::Text("Letzter Sync: %s", state->lastSync.c_str());
        RenderRtApiStatus();
        ImGui::Dummy(ImVec2(0.0f, 0.5f));

        const auto now = std::chrono::steady_clock::now();
        const bool isFetching = m_SharedState.IsFetching();
        const bool syncCooldownActive = now - m_LastManualSync < std::chrono::seconds(10);

        const bool canSync = !isFetching && !syncCooldownActive;

        if (!canSync)
        {
            ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
            ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
        }

        if (ImGui::Button("Jetzt synchronisieren") && canSync)
        {
            m_LastManualSync = now;
            m_SyncNow();
        }

        if (!canSync)
        {
            ImGui::PopItemFlag();
            ImGui::PopStyleVar();
        }

        ImGui::SameLine();

        if (isFetching)
        {
            ImGui::TextDisabled("Synchronisiere...");
        }
        else if (syncCooldownActive)
        {
            const auto remaining =
                10 - std::chrono::duration_cast<std::chrono::seconds>(
                    now - m_LastManualSync).count();

            ImGui::TextDisabled("Bitte warten... %llds", remaining);
        }
        else ImGui::TextDisabled("Auto Sync aktiv");
        ImGui::Dummy(ImVec2(0.0f, 8.0f));

        if (state->events.empty()) { ImGui::TextDisabled("Keine Events vorhanden."); return; }

        if (ImGui::BeginTable("events", 7, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingStretchProp))
        {
            ImGui::TableSetupColumn("Event", ImGuiTableColumnFlags_WidthStretch, 2.6f);
            ImGui::TableSetupColumn("Start", ImGuiTableColumnFlags_WidthStretch, 1.2f);
            ImGui::TableSetupColumn("Eventleiter", ImGuiTableColumnFlags_WidthStretch, 1.3f);
            ImGui::TableSetupColumn("Tag", ImGuiTableColumnFlags_WidthFixed, 90.0f);
            ImGui::TableSetupColumn("Teilnehmer / Rollen", ImGuiTableColumnFlags_WidthStretch, 2.2f);
            ImGui::TableSetupColumn("Angemeldet", ImGuiTableColumnFlags_WidthFixed, 95.0f);
            ImGui::TableSetupColumn("Aktionen", ImGuiTableColumnFlags_WidthFixed, 170.0f);
            ImGui::TableHeadersRow();

            for (const auto& event : state->events)
            {
                ImGui::PushID(event.id.c_str());
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                bool eventOpen = ImGui::TreeNodeEx("event", ImGuiTreeNodeFlags_SpanFullWidth, "%s", event.title.c_str());
                if (Utility::IsEventActive(event)) { 
                    ImGui::SameLine(0.0f, 4.0f);
                    ImGui::TextColored(ImVec4(0.20f, 0.90f, 0.30f, 1.0f), "[Aktiv]"); 
                }
                if (eventOpen)
                {
                    std::string cleanDescription = Utility::CleanEventDescription(event.description);
                    if (!cleanDescription.empty()) RenderMarkdownText(cleanDescription);
                    else ImGui::TextDisabled("Keine Beschreibung.");
                    ImGui::TreePop();
                }

                ImGui::TableSetColumnIndex(1); 
                ImGui::TextWrapped("%s", Utility::FormatGermanDateTime(event.start).c_str());
                ImGui::TableSetColumnIndex(2);

                if (!event.leaderName.empty() || !event.leaderAccount.empty())
                {
                    bool isSelfLeader = !state->viewerGw2Account.empty() && event.leaderAccount == state->viewerGw2Account;
                    if (isSelfLeader) ImGui::TextColored(ImVec4(1.0f, 0.15f, 0.15f, 1.0f), "%s", Utility::DisplayUser(event.leaderName, event.leaderAccount).c_str());
                    else ImGui::TextWrapped("%s", Utility::DisplayUser(event.leaderName, event.leaderAccount).c_str());
                }
                else ImGui::TextDisabled("-");

                ImGui::TableSetColumnIndex(3); 
                ImGui::TextColored(Utility::TagColor(event.tag), "%s", Utility::TagLabel(event.tag).c_str());
                ImGui::TableSetColumnIndex(4);

                std::string attendeeLabel = std::to_string(event.attendeeCount) + "/" + std::to_string(event.slotCount);
                if (ImGui::TreeNodeEx(attendeeLabel.c_str(), ImGuiTreeNodeFlags_SpanFullWidth)) { 
                    RenderEventAttendeesTable(event); 
                    ImGui::TreePop(); 
                }

                ImGui::TableSetColumnIndex(5);
                if (event.isViewerAttending) ImGui::TextColored(ImVec4(0.20f, 0.90f, 0.30f, 1.0f), "Ja");
                else ImGui::TextColored(ImVec4(1.0f, 0.25f, 0.25f, 1.0f), "Nein");

                ImGui::TableSetColumnIndex(6);
                if (!event.url.empty() && ImGui::Button("Web")) ShellExecuteA(nullptr, "open", event.url.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
                if (!event.leaderAccount.empty()) { 
                    ImGui::SameLine(); 
                    if (ImGui::Button("SqJoin")) Utility::CopyToClipboard("/sqjoin " + event.leaderAccount); 
                }
                ImGui::PopID();
            }
            ImGui::EndTable();
        }
    }

    void EventWindow::RenderAddonWindow()
    {
        auto state = m_SharedState.GetState();
        if (state)
        {
            m_ReminderService.CheckEventReminders(*state);
            m_ReminderService.CheckNewEventAnnouncements(*state);
        }

        m_ReminderService.Render();

        bool show = m_SharedState.IsWindowShown();
        if (!show) return;

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(14.0f, 8.0f));
        if (!ImGui::Begin("Legendary Impact - Eventmanager###LegendaryImpactEventmanagerWindow", &show))
        {
            m_SharedState.SetWindowShown(show);
            ImGui::End(); ImGui::PopStyleVar(); return;
        }
        m_SharedState.SetWindowShown(show);
        RenderEventsWindow();
        ImGui::End();
        ImGui::PopStyleVar();
    }

    void EventWindow::RenderOptions()
    {
        ImGui::Separator();
        ImGui::Text("Legendary Impact - Eventmanager");
        RenderRtApiStatus();
        auto state = m_SharedState.GetState();
        if (state)
        {
            ImGui::TextUnformatted("Angemeldet als:"); ImGui::SameLine();
            ImGui::TextColored(ImVec4(0.95f, 0.80f, 0.35f, 1.0f), "%s", ViewerLabel(*state).c_str());
        }

        bool show = m_SharedState.IsWindowShown();
        if (ImGui::Checkbox("Fenster anzeigen", &show)) { m_SharedState.SetWindowShown(show); m_ConfigStore.Save(); }

        ImGui::InputText("Legendary Impact Token", m_ConfigStore.TokenBuffer(), 512, ImGuiInputTextFlags_Password);
        ImGui::SliderInt("Auto Sync Intervall Minuten", &m_ConfigStore.RefreshMinutes(), 5, 60);
        if (m_ConfigStore.RefreshMinutes() < 5) m_ConfigStore.RefreshMinutes() = 5;
        ImGui::TextDisabled("Keybind: bitte in den Nexus Keybind-Einstellungen fuer Legendary Impact - Eventmanager setzen.");

        ImGui::Spacing(); 
        ImGui::Separator(); 
        ImGui::Text("Reminder");
        ImGui::Checkbox("Reminder aktivieren", &m_ConfigStore.ReminderEnabled());
        ImGui::SliderInt("Reminder Minuten vor Event", &m_ConfigStore.ReminderMinutesBefore(), 1, 120);
        if (m_ConfigStore.ReminderMinutesBefore() < 1) m_ConfigStore.ReminderMinutesBefore() = 1;
        ImGui::SliderInt("Reminder wiederholen alle Minuten", &m_ConfigStore.ReminderRepeatMinutes(), 1, 60);
        if (m_ConfigStore.ReminderRepeatMinutes() < 1) m_ConfigStore.ReminderRepeatMinutes() = 1;
        ImGui::Checkbox("Neue Events ankuendigen", &m_ConfigStore.AnnounceNewEventsEnabled());

        if (ImGui::Button("Test Reminder")) m_ReminderService.ShowReminder("Wing 4 Fullclear (Auch fuer Anfaenger)", Utility::FormatLocalNow(), "RAID", 15);
        ImGui::SameLine();
        if (ImGui::Button("Test Neue Events"))
        {
            std::vector<EventItem> testEvents;

            EventItem raid;
            raid.title = "Wing 4 Fullclear";
            raid.start = "2025-01-15T19:00:00";
            raid.tag = "RAID";

            EventItem strike;
            strike.title = "Strike Training";
            strike.start = "2025-01-16T20:00:00";
            strike.tag = "STRIKE";

            EventItem fractal;
            fractal.title = "CM Fraktale";
            fractal.start = "2025-01-17T18:30:00";
            fractal.tag = "FRACTAL";

            testEvents.push_back(raid);
            testEvents.push_back(strike);
            testEvents.push_back(fractal);

            m_ReminderService.ShowNewEventsAnnouncement(testEvents);
        }
        ImGui::Spacing();
        if (ImGui::Button("Einstellungen speichern")) { m_ConfigStore.ApplyFromEditBuffer(); m_SyncNow(); }
    }
}
