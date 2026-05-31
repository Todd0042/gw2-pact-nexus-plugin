#include "EventWindow.h"
#include "Constants.h"
#include "Utility.h"
#include "SquadManager.h"
#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"
#include <shellapi.h>

namespace LegendaryImpactEventmanager
{
    EventWindow::EventWindow(AddonAPI_t*& api, SharedState& sharedState, ConfigStore& configStore, ReminderService& reminderService, RTAPI::RealTimeData*& rtApi, std::function<void()> syncNow)
        : m_Api(api), m_SharedState(sharedState), m_ConfigStore(configStore), m_ReminderService(reminderService), m_RtApi(rtApi), m_SyncNow(std::move(syncNow)) {
    }

    bool EventWindow::IsViewerAttending(const EventItem& event, const PluginState& state) const
    {
        // The API's per-event isViewerAttending flag is unreliable (can be false
        // while signed up), so treat it only as a hint and confirm against the
        // attendee list using the viewer's own account.
        if (event.isViewerAttending) return true;

        // Resolve the viewer's own account. The Legendary Impact API sometimes
        // returns no viewer identity at all, so fall back to the RealTime API's
        // self account name (RealTimeData::AccountName) when it's available.
        std::string viewer = SquadManager::NormalizeAccountName(state.viewerGw2Account);
        if (viewer.empty() && m_RtApi && m_RtApi->AccountName[0] != '\0')
        {
            viewer = SquadManager::NormalizeAccountName(m_RtApi->AccountName);
        }
        if (viewer.empty()) return false;

        for (const auto& attendee : event.attendees)
        {
            if (SquadManager::NormalizeAccountName(attendee.gw2Account) == viewer)
            {
                return true;
            }
        }

        return false;
    }

    bool EventWindow::ShouldShowJoinButton(const EventItem& event, const PluginState& state) const
    {
        // Base eligibility: a real squad event with a leader (not a meeting).
        if (event.leaderAccount.empty() || event.slotCount <= 5 || event.tag == "MEETING")
        {
            return false;
        }

        // Only for events the viewer actually signed up for.
        if (!IsViewerAttending(event, state)) return false;

        // Minutes before start at which the button becomes available.
        constexpr int kJoinLeadMinutes = 15;

        std::time_t startTime = 0;
        if (Utility::ParseIsoUtc(event.start, startTime))
        {
            const int secondsUntilStart =
                static_cast<int>(std::difftime(startTime, std::time(nullptr)));

            if (secondsUntilStart >= 0 && secondsUntilStart <= kJoinLeadMinutes * 60)
            {
                return true;
            }
        }

        // Keep it available while the event is under way (now between start/end).
        return Utility::IsEventActive(event);
    }

    void EventWindow::QueueChatCommand(const std::string& command)
    {
        if (command.empty()) return;

        // A new request overrides any in-flight one.
        m_ChatPending = command;
        m_ChatStage = ChatSendStage::OpenChat;
        m_ChatStageAt = std::chrono::steady_clock::now();
    }

    HWND EventWindow::ResolveGameWindow()
    {
        if (m_GameWindow && IsWindow(m_GameWindow)) return m_GameWindow;

        m_GameWindow = FindWindowA("ArenaNet_Dx_Window_Class", nullptr);
        if (!m_GameWindow) m_GameWindow = FindWindowA(nullptr, "Guild Wars 2");
        if (!m_GameWindow) m_GameWindow = GetForegroundWindow();

        return m_GameWindow;
    }

    void EventWindow::SendKeyToGame(WORD virtualKey, bool keyUp)
    {
        if (!m_Api || !m_Api->WndProc_SendToGameOnly) return;

        const HWND hwnd = ResolveGameWindow();
        if (!hwnd) return;

        const UINT scanCode = MapVirtualKeyW(virtualKey, MAPVK_VK_TO_VSC);

        // Standard WM_KEY* lParam: bits 0-15 repeat count (1), bits 16-23 scan code.
        LPARAM lParam = static_cast<LPARAM>(1) | (static_cast<LPARAM>(scanCode) << 16);
        if (keyUp)
        {
            // Bit 30 = previous key state down, bit 31 = transition (key released).
            lParam |= (static_cast<LPARAM>(1) << 30) | (static_cast<LPARAM>(1) << 31);
        }

        m_Api->WndProc_SendToGameOnly(hwnd, keyUp ? WM_KEYUP : WM_KEYDOWN, virtualKey, lParam);
    }

    void EventWindow::SendCharToGame(char character)
    {
        if (!m_Api || !m_Api->WndProc_SendToGameOnly) return;

        const HWND hwnd = ResolveGameWindow();
        if (!hwnd) return;

        m_Api->WndProc_SendToGameOnly(
            hwnd,
            WM_CHAR,
            static_cast<WPARAM>(static_cast<unsigned char>(character)),
            static_cast<LPARAM>(1));
    }

    void EventWindow::TickChatSender()
    {
        if (m_ChatStage == ChatSendStage::Idle) return;

        const auto now = std::chrono::steady_clock::now();
        const auto elapsedMs =
            std::chrono::duration_cast<std::chrono::milliseconds>(now - m_ChatStageAt).count();

        switch (m_ChatStage)
        {
        case ChatSendStage::OpenChat:
            // Enter opens the chat input box.
            SendKeyToGame(VK_RETURN, false);
            SendKeyToGame(VK_RETURN, true);
            m_ChatStageAt = now;
            m_ChatStage = ChatSendStage::TypeText;
            break;

        case ChatSendStage::TypeText:
            // Give the game a moment to actually open the chat box before typing,
            // otherwise the characters are dropped or interpreted as keybinds.
            if (elapsedMs < 150) break;
            for (char c : m_ChatPending) SendCharToGame(c);
            m_ChatStageAt = now;
            m_ChatStage = ChatSendStage::Submit;
            break;

        case ChatSendStage::Submit:
            if (elapsedMs < 40) break;
            // Enter again submits the command.
            SendKeyToGame(VK_RETURN, false);
            SendKeyToGame(VK_RETURN, true);
            m_ChatPending.clear();
            m_ChatStage = ChatSendStage::Idle;
            break;

        case ChatSendStage::Idle:
            break;
        }
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
            ImGui::TextColored(ImVec4(0.20f, 0.90f, 0.30f, 1.0f), "Installiert");
        }
        else
        {
            ImGui::TextColored(ImVec4(1.0f, 0.25f, 0.25f, 1.0f), "Nicht installiert");
            ImGui::SameLine();
            ImGui::TextDisabled("(eingeschränkte Funktionalität)");
        }
    }

    void EventWindow::RenderStatusIcon(bool active)
    {
        const char* textureId = active
            ? Constants::SquadIconId
            : Constants::NoSquadIconId;

        Texture_t* texture = m_Api->Textures_Get(textureId);

        if (!texture || !texture->Resource)
        {
            if (active) ImGui::TextColored(ImVec4(0.20f, 0.90f, 0.30f, 1.0f), "Ja");
            else ImGui::TextColored(ImVec4(1.0f, 0.25f, 0.25f, 1.0f), "Nein");
            return;
        }

        ImGui::Image((ImTextureID)texture->Resource, ImVec2(18.0f, 18.0f));
    }

    void EventWindow::RenderEventTitle(const EventItem& event)
    {
        constexpr float iconSize = 14.0f;

        if (!event.isPublic)
        {
            Texture_t* texture = m_Api->Textures_Get(Constants::Lock);

            const float startY = ImGui::GetCursorPosY();
            const float textHeight = ImGui::GetTextLineHeight();

            if (texture && texture->Resource)
            {
                ImGui::Image((ImTextureID)texture->Resource, ImVec2(iconSize, iconSize));
            }
            else
            {
                ImGui::TextUnformatted("[Intern]");
            }
            ImGui::SameLine();
        }

        ImGui::TextWrapped("%s", event.title.c_str());
    }

    void EventWindow::RenderBoonIcon(const std::string& boon)
    {
        const char* textureId = nullptr;
        if (boon == "QUICKNESS") textureId = Constants::QuicknessIconId;
        else if (boon == "ALACRITY") textureId = Constants::AlacrityIconId;

        if (!textureId) { ImGui::TextDisabled("-"); return; }

        Texture_t* texture = m_Api->Textures_Get(textureId);
        if (!texture || !texture->Resource)
        {
            ImGui::TextDisabled("%s", Utility::BoonLabel(boon).c_str());
            return;
        }

        ImGui::Image((ImTextureID)texture->Resource, ImVec2(22.0f, 22.0f));
    }

    void EventWindow::RenderRoleWithBoon(const std::string& role, const std::string& boon)
    {
        const float iconSize = 22.0f;
        const float textHeight = ImGui::GetTextLineHeight();

        float startY = ImGui::GetCursorPosY();

        if (!boon.empty())
        {
            ImGui::SetCursorPosY(startY + (iconSize - textHeight) * 0.5f);
        }

        ImGui::TextColored(RoleColor(role), "%s", Utility::RoleLabel(role).c_str());

        if (!boon.empty())
        {
            ImGui::SameLine();
            ImGui::SetCursorPosY(startY);
            RenderBoonIcon(boon);
        }
    }

    void EventWindow::RenderMarkdownText(const std::string& text)
    {
        std::string line;

        auto trimLeft = [](std::string value)
            {
                while (!value.empty() && (value.front() == ' ' || value.front() == '\t'))
                    value.erase(value.begin());

                return value;
            };

        auto renderLine = [&](const std::string& raw)
            {
                std::string line = Utility::StripSimpleMarkdown(raw);
                line = trimLeft(line);

                if (line.empty()) { ImGui::Spacing(); return; }
                if (line == "---" || line == "***" || line == "___") return;

                if (line.rfind("###", 0) == 0)
                    ImGui::TextColored(ImVec4(0.90f, 0.75f, 0.35f, 1.0f), "%s", trimLeft(line.substr(3)).c_str());
                else if (line.rfind("##", 0) == 0)
                    ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.45f, 1.0f), "%s", trimLeft(line.substr(2)).c_str());
                else if (line.rfind("#", 0) == 0)
                    ImGui::TextColored(ImVec4(1.0f, 0.9f, 0.55f, 1.0f), "%s", trimLeft(line.substr(1)).c_str());
                else if (line.rfind("> ", 0) == 0)
                    ImGui::TextDisabled("%s", line.substr(2).c_str());
                else if (line.rfind("- [ ] ", 0) == 0 || line.rfind("* [ ] ", 0) == 0)
                    ImGui::BulletText("[ ] %s", line.substr(6).c_str());
                else if (line.rfind("- [x] ", 0) == 0 || line.rfind("* [x] ", 0) == 0 ||
                    line.rfind("- [X] ", 0) == 0 || line.rfind("* [X] ", 0) == 0)
                    ImGui::BulletText("[x] %s", line.substr(6).c_str());
                else if (line.size() >= 2 &&
                    (line[0] == '-' || line[0] == '*' || line[0] == '+') &&
                    line[1] == ' ')
                    ImGui::BulletText("%s", line.substr(2).c_str());
                else
                {
                    size_t dot = line.find(". ");

                    bool orderedList = dot != std::string::npos && dot > 0;
                    for (size_t i = 0; orderedList && i < dot; ++i)
                        if (!std::isdigit(static_cast<unsigned char>(line[i])))
                            orderedList = false;

                    if (orderedList)
                        ImGui::BulletText("%s", line.substr(dot + 2).c_str());
                    else
                        ImGui::TextWrapped("%s", line.c_str());
                }
            };

        for (char c : text)
        {
            if (c == '\r') continue;
            if (c == '\n') { renderLine(line); line.clear(); }
            else line += c;
        }

        if (!line.empty()) renderLine(line);
    }

    void EventWindow::RenderEventAttendeesTable(const EventItem& event, const PluginState& state)
    {
        if (event.attendees.empty())
        {
            ImGui::TextDisabled("Keine Teilnehmerdaten vorhanden.");
            return;
        }

        const bool showSquadColumn = m_RtApi && m_RtApi->GameBuild != 0;
        const int columnCount = showSquadColumn ? 4 : 3;

        if (ImGui::BeginTable(
            "attendeesTable",
            columnCount,
            ImGuiTableFlags_Borders |
            ImGuiTableFlags_RowBg |
            ImGuiTableFlags_Resizable |
            ImGuiTableFlags_SizingStretchProp))
        {
            if (showSquadColumn)
            {
                ImGui::TableSetupColumn("Squad", ImGuiTableColumnFlags_WidthFixed);
            }

            ImGui::TableSetupColumn("Spieler", ImGuiTableColumnFlags_WidthFixed);
            ImGui::TableSetupColumn("Rolle", ImGuiTableColumnFlags_WidthFixed);
            ImGui::TableSetupColumn("Flex", ImGuiTableColumnFlags_WidthStretch, 1.0f);
            ImGui::TableHeadersRow();

            for (const auto& attendee : event.attendees)
            {
                ImGui::TableNextRow();

                int column = 0;

                bool isSelf =
                    !state.viewerGw2Account.empty() &&
                    attendee.gw2Account == state.viewerGw2Account;

                if (showSquadColumn)
                {
                    ImGui::TableSetColumnIndex(column++);

                    bool inSquad = false;
                    std::string characterName;
                    uint32_t subGroup = 0;

                    if (!attendee.gw2Account.empty())
                    {
                        const std::string attendeeAccount = SquadManager::NormalizeAccountName(attendee.gw2Account);

                        for (const auto& member : state.squadMembers)
                        {
                            if (member.normalizedAccountName == attendeeAccount)
                            {
                                inSquad = true;
                                characterName = member.characterName;
                                isSelf = isSelf || member.isSelf;
                                subGroup = member.subgroup;
                                break;
                            }
                        }
                    }

                    const float iconSize = 18.0f;
                    const float textHeight = ImGui::GetTextLineHeight();
                    const float startY = ImGui::GetCursorPosY();

                    RenderStatusIcon(inSquad);

                    if (inSquad && !characterName.empty())
                    {
                        ImGui::SameLine();
                        ImGui::SetCursorPosY(startY + (iconSize - textHeight) * 0.5f);

                        if (isSelf)
                        {
                            ImGui::TextColored(ImVec4(1.0f, 0.15f, 0.15f, 1.0f), "%s", characterName.c_str());
                        }
                        else
                        {
                            ImGui::TextUnformatted(characterName.c_str());
                        }

                        if (subGroup != 0)
                        {
                            ImGui::SameLine();
                            ImGui::TextUnformatted(("[" + std::to_string(subGroup) + "]").c_str());
                        }

                        ImGui::SetCursorPosY(startY);
                    }
                }

                ImGui::TableSetColumnIndex(column++);

                if (isSelf)
                {
                    ImGui::TextColored(
                        ImVec4(1.0f, 0.15f, 0.15f, 1.0f),
                        "%s",
                        Utility::DisplayUser(attendee.username, attendee.gw2Account).c_str());
                }
                else
                {
                    ImGui::TextUnformatted(Utility::DisplayUser(attendee.username, attendee.gw2Account).c_str());
                }

                ImGui::TableSetColumnIndex(column++);
                RenderRoleWithBoon(attendee.role, attendee.boon);

                ImGui::TableSetColumnIndex(column++);

                if (attendee.flexRoles.empty())
                {
                    ImGui::TextDisabled("-");
                }
                else
                {
                    for (const auto& flex : attendee.flexRoles)
                    {
                        RenderRoleWithBoon(flex.role, flex.boon);
                    }
                }
            }

            ImGui::EndTable();
        }
    }

    void EventWindow::RenderEventsWindow(const PluginState& state)
    {
        ImGui::TextUnformatted("Angemeldet als:");
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.95f, 0.80f, 0.35f, 1.0f), "%s", ViewerLabel(state).c_str());
        ImGui::Text("Letzter Sync: %s", state.lastSync.c_str());
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
        else
        {
            ImGui::TextDisabled("Auto Sync aktiv");
        }

        ImGui::Dummy(ImVec2(0.0f, 8.0f));

        if (state.events.empty())
        {
            ImGui::TextDisabled("Keine Events vorhanden.");
            return;
        }

        if (ImGui::BeginTable(
            "events",
            7,
            ImGuiTableFlags_Borders |
            ImGuiTableFlags_RowBg |
            ImGuiTableFlags_Resizable |
            ImGuiTableFlags_SizingStretchProp))
        {
            ImGui::TableSetupColumn("Event", ImGuiTableColumnFlags_WidthStretch, 2.0f);
            ImGui::TableSetupColumn("Start", ImGuiTableColumnFlags_WidthFixed);
            ImGui::TableSetupColumn("Eventleiter", ImGuiTableColumnFlags_WidthFixed);
            ImGui::TableSetupColumn("Tag", ImGuiTableColumnFlags_WidthFixed);
            ImGui::TableSetupColumn("Teilnehmer / Rollen", ImGuiTableColumnFlags_WidthStretch, 2.5f);
            ImGui::TableSetupColumn("Angemeldet", ImGuiTableColumnFlags_WidthFixed);
            ImGui::TableSetupColumn("Aktionen", ImGuiTableColumnFlags_WidthFixed);
            ImGui::TableHeadersRow();

            for (const auto& event : state.events)
            {
                ImGui::PushID(event.id.c_str());
                ImGui::TableNextRow();

                ImGui::TableSetColumnIndex(0);

                bool eventOpen = ImGui::TreeNodeEx("event", ImGuiTreeNodeFlags_SpanFullWidth, "");
                ImGui::SameLine(0.0f, 12.0f);
                RenderEventTitle(event);

                if (Utility::IsEventActive(event))
                {
                    ImGui::SameLine();
                    ImGui::TextColored(ImVec4(0.20f, 0.90f, 0.30f, 1.0f), "[Aktiv]");
                }

                if (eventOpen)
                {
                    std::string cleanDescription = Utility::CleanEventDescription(event.description);

                    if (!cleanDescription.empty())
                    {
                        RenderMarkdownText(cleanDescription);
                    }
                    else
                    {
                        ImGui::TextDisabled("Keine Beschreibung.");
                    }

                    ImGui::TreePop();
                }

                ImGui::TableSetColumnIndex(1);
                ImGui::TextWrapped("%s", Utility::FormatGermanDateTime(event.start).c_str());

                ImGui::TableSetColumnIndex(2);

                if (!event.leaderName.empty() || !event.leaderAccount.empty())
                {
                    bool isSelfLeader =
                        !state.viewerGw2Account.empty() &&
                        event.leaderAccount == state.viewerGw2Account;

                    if (isSelfLeader)
                    {
                        ImGui::TextColored(
                            ImVec4(1.0f, 0.15f, 0.15f, 1.0f),
                            "%s",
                            Utility::DisplayUser(event.leaderName, event.leaderAccount).c_str());
                    }
                    else
                    {
                        ImGui::TextWrapped("%s", Utility::DisplayUser(event.leaderName, event.leaderAccount).c_str());
                    }
                }
                else
                {
                    ImGui::TextDisabled("-");
                }

                ImGui::TableSetColumnIndex(3);
                ImGui::TextColored(Utility::TagColor(event.tag), "%s", Utility::TagLabel(event.tag).c_str());

                ImGui::TableSetColumnIndex(4);

                std::string attendeeLabel = std::to_string(event.attendeeCount) + "/" + std::to_string(event.slotCount);

                if (ImGui::TreeNodeEx(attendeeLabel.c_str(), ImGuiTreeNodeFlags_SpanFullWidth))
                {
                    RenderEventAttendeesTable(event, state);
                    ImGui::TreePop();
                }

                ImGui::TableSetColumnIndex(5);
                RenderStatusIcon(event.isViewerAttending);

                ImGui::TableSetColumnIndex(6);

                if (!event.url.empty() && ImGui::Button("Event anzeigen"))
                {
                    ShellExecuteA(nullptr, "open", event.url.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
                }

                if (ShouldShowJoinButton(event, state))
                {
                    ImGui::SameLine();

                    const std::string sqjoinCommand = "/sqjoin " + event.leaderAccount;

                    if (ImGui::Button("Squad beitreten"))
                    {
                        QueueChatCommand(sqjoinCommand);
                    }

                    if (ImGui::IsItemHovered())
                    {
                        ImGui::SetTooltip(
                            "Linksklick: automatisch beitreten\n"
                            "Rechtsklick: %s kopieren",
                            sqjoinCommand.c_str());
                    }

                    if (ImGui::IsItemClicked(ImGuiMouseButton_Right))
                    {
                        Utility::CopyToClipboard(sqjoinCommand);
                    }
                }

                ImGui::PopID();
            }

            ImGui::EndTable();
        }
    }

    void EventWindow::RenderAddonWindow()
    {
        // Advance any in-flight /sqjoin send. Ticked unconditionally (even while
        // the window is hidden) so a queued send finishes if the user closes the
        // window mid-flight.
        TickChatSender();

        m_SharedState.WithStateRead([&](const PluginState& state) {
            m_ReminderService.CheckEventReminders(state);
            m_ReminderService.CheckNewEventAnnouncements(state);
            });

        m_ReminderService.Render();

        bool show = m_SharedState.IsWindowShown();
        if (!show) return;

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(14.0f, 8.0f));

        if (!ImGui::Begin("Legendary Impact - Eventmanager###LegendaryImpactEventmanagerWindow", &show))
        {
            m_SharedState.SetWindowShown(show);
            ImGui::End();
            ImGui::PopStyleVar();
            return;
        }

        m_SharedState.SetWindowShown(show);
        m_SharedState.WithStateRead([&](const PluginState& state) { RenderEventsWindow(state); });

        ImGui::End();
        ImGui::PopStyleVar();
    }

    void EventWindow::RenderOptions()
    {
        ImGui::Separator();
        ImGui::Text("Legendary Impact - Eventmanager");
        RenderRtApiStatus();

        m_SharedState.WithStateRead([&](const PluginState& state) {
            ImGui::TextUnformatted("Angemeldet als:");
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(0.95f, 0.80f, 0.35f, 1.0f), "%s", ViewerLabel(state).c_str());
            });

        bool show = m_SharedState.IsWindowShown();

        if (ImGui::Checkbox("Fenster anzeigen", &show))
        {
            m_SharedState.SetWindowShown(show);
            m_ConfigStore.Save();
        }

        ImGui::InputText("Legendary Impact Token", m_ConfigStore.TokenBuffer(), 512, ImGuiInputTextFlags_Password);
        ImGui::SliderInt("Auto Sync Intervall Minuten", &m_ConfigStore.RefreshMinutes(), 5, 60);

        if (m_ConfigStore.RefreshMinutes() < 5)
        {
            m_ConfigStore.RefreshMinutes() = 5;
        }

        ImGui::TextDisabled("Keybind: bitte in den Nexus Keybind-Einstellungen für Legendary Impact - Eventmanager setzen.");

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Text("Reminder");

        ImGui::Checkbox("Reminder aktivieren", &m_ConfigStore.ReminderEnabled());
        ImGui::SliderInt("Reminder Minuten vor Event", &m_ConfigStore.ReminderMinutesBefore(), 1, 120);

        if (m_ConfigStore.ReminderMinutesBefore() < 1)
        {
            m_ConfigStore.ReminderMinutesBefore() = 1;
        }

        ImGui::SliderInt("Reminder wiederholen alle Minuten", &m_ConfigStore.ReminderRepeatMinutes(), 1, 60);

        if (m_ConfigStore.ReminderRepeatMinutes() < 1)
        {
            m_ConfigStore.ReminderRepeatMinutes() = 1;
        }

        ImGui::Checkbox("Neue Events ankündigen", &m_ConfigStore.AnnounceNewEventsEnabled());

        if (ImGui::Button("Test Reminder"))
        {
            m_ReminderService.ShowReminder(
                "Wing 4 Fullclear (Auch für Anfänger)",
                Utility::FormatLocalNow(),
                "RAID",
                15
            );
        }

        ImGui::SameLine();

        if (ImGui::Button("Test Neue Events"))
        {
            std::vector<EventItem> testEvents;

            EventItem raid;
            raid.title = "Wing 4 Fullclear (Auch für Anfänger)";
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

        if (ImGui::Button("Einstellungen speichern"))
        {
            m_ConfigStore.ApplyFromEditBuffer();
            m_SyncNow();
        }
    }
}