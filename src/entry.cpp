#include <Windows.h>
#include <winhttp.h>
#include <shellapi.h>
#pragma comment(lib, "winhttp.lib")
#include <mmsystem.h>
#pragma comment(lib, "winmm.lib")

#include "nexus/Nexus.h"
#include "mumble/Mumble.h"
#include "imgui/imgui.h"
#include "nlohmann/json.hpp"
#include "resource.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstring>
#include <ctime>
#include <direct.h>
#include <fstream>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

using json = nlohmann::json;

void AddonLoad(AddonAPI* aApi);
void AddonUnload();
void AddonRender();
void AddonOptions();
void WorkerLoop();
void FetchEvents();
void RenderEventsWindow();
void SaveSettings();
void LoadSettings();
void OnInputBind(const char* aIdentifier, bool aIsRelease);
void RequestSyncNow();

AddonDefinition AddonDef = {};
HMODULE hSelf = nullptr;
AddonAPI* APIDefs = nullptr;
NexusLinkData* NexusLink = nullptr;
Mumble::Data* MumbleLink = nullptr;

const char* name = "Legendary Impact - Eventmanager";
const char* API_BASE_URL = "https://legendary-impact.de";
const char* SETTINGS_DIR = "addons/LegendaryImpactEventmanager";
const char* SETTINGS_FILE = "addons/LegendaryImpactEventmanager/settings.json";

const char* QA_ID = "QA_LI_EVENTMANAGER";
const char* KB_ID = "KB_LI_EVENTMANAGER";
const char* ICON_ID = "ICON_LI_EVENTMANAGER";
const char* ICON_HOVER_ID = "ICON_LI_EVENTMANAGER_HOVER";

std::atomic<bool> g_Running = false;
std::atomic<bool> g_Fetching = false;
std::atomic<bool> g_ShowWindow = true;
std::atomic<bool> g_ManualSyncRequested = false;

std::thread g_Worker;
std::mutex g_WorkerMutex;
std::condition_variable g_WorkerWake;

struct PluginConfig
{
    std::string token = "";
    int refreshMinutes = 5;

    bool reminderEnabled = true;
    int reminderMinutesBefore = 15;
    int reminderRepeatMinutes = 5;
};

struct EventFlexRole
{
    std::string role;
    std::string boon;
};

struct EventAttendee
{
    std::string username;
    std::string gw2Account;
    std::string role;
    std::string boon;
    std::vector<EventFlexRole> flexRoles;
};

struct EventItem
{
    std::string id;
    std::string title;
    std::string description;
    std::string start;
    std::string end;
    std::string tag;
    std::string location;
    std::string url;

    std::string leaderName;
    std::string leaderAccount;

    bool isViewerAttending = false;
    int attendeeCount = 0;
    int slotCount = 0;

    std::vector<EventAttendee> attendees;
};

struct PluginState
{
    std::string lastSync = "-";

    std::string viewerUsername = "";
    std::string viewerGw2Account = "";

    std::vector<EventItem> events;
};

std::shared_ptr<PluginConfig> g_Config = std::make_shared<PluginConfig>();
std::shared_ptr<PluginState> g_State = std::make_shared<PluginState>();

char g_EditToken[512] = "";
int g_EditRefreshMinutes = 5;

bool g_EditReminderEnabled = true;
int g_EditReminderMinutesBefore = 15;
int g_EditReminderRepeatMinutes = 5;

bool g_ShowReminderMessage = false;
std::string g_ReminderTitle = "";
std::string g_ReminderDate = "";
std::string g_ReminderTimeLeft = "";
std::unordered_map<std::string, std::time_t> g_ReminderLastShown;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH) hSelf = hModule;
    return TRUE;
}

extern "C" __declspec(dllexport) AddonDefinition* GetAddonDef()
{
    AddonDef.Signature = -84629;
    AddonDef.APIVersion = NEXUS_API_VERSION;
    AddonDef.Name = name;
    AddonDef.Version.Major = 1;
    AddonDef.Version.Minor = 0;
    AddonDef.Version.Build = 0;
    AddonDef.Version.Revision = 1;
    AddonDef.Author = "Backxtar";
    AddonDef.Description = "Guild Wars 2 Eventmanager for Legendary Impact.";
    AddonDef.Load = AddonLoad;
    AddonDef.Unload = AddonUnload;
    AddonDef.Flags = EAddonFlags_None;
    return &AddonDef;
}

void RequestSyncNow()
{
    g_ManualSyncRequested = true;
    g_WorkerWake.notify_one();
}


std::string JsonString(const json& item, const char* key, const std::string& fallback = "")
{
    if (!item.contains(key) || !item[key].is_string()) return fallback;
    return item[key].get<std::string>();
}

void ReplaceAll(std::string& value, const std::string& from, const std::string& to)
{
    size_t pos = 0;

    while ((pos = value.find(from, pos)) != std::string::npos)
    {
        value.replace(pos, from.length(), to);
        pos += to.length();
    }
}

std::string ReplaceGermanUmlauts(std::string value)
{
    ReplaceAll(value, u8"ä", "ae");
    ReplaceAll(value, u8"Ä", "Ae");
    ReplaceAll(value, u8"ö", "oe");
    ReplaceAll(value, u8"Ö", "Oe");
    ReplaceAll(value, u8"ü", "ue");
    ReplaceAll(value, u8"Ü", "Ue");
    ReplaceAll(value, u8"ß", "ss");
    return value;
}

std::string StripUnsupportedEmoji(const std::string& input)
{
    std::string out;

    for (size_t i = 0; i < input.size();)
    {
        unsigned char c = (unsigned char)input[i];

        if (c < 0x80)
        {
            out += input[i++];
            continue;
        }

        int len = 0;

        if ((c & 0xE0) == 0xC0) len = 2;
        else if ((c & 0xF0) == 0xE0) len = 3;
        else if ((c & 0xF8) == 0xF0) len = 4;
        else
        {
            ++i;
            continue;
        }

        if (i + len > input.size()) break;

        unsigned int cp = 0;

        if (len == 2)
        {
            cp = ((input[i] & 0x1F) << 6) |
                (input[i + 1] & 0x3F);
        }
        else if (len == 3)
        {
            cp = ((input[i] & 0x0F) << 12) |
                ((input[i + 1] & 0x3F) << 6) |
                (input[i + 2] & 0x3F);
        }
        else if (len == 4)
        {
            cp = ((input[i] & 0x07) << 18) |
                ((input[i + 1] & 0x3F) << 12) |
                ((input[i + 2] & 0x3F) << 6) |
                (input[i + 3] & 0x3F);
        }

        bool remove =
            (cp >= 0x1F000 && cp <= 0x1FAFF) ||
            (cp >= 0x2600 && cp <= 0x27BF) ||
            (cp >= 0xFE00 && cp <= 0xFE0F) ||
            (cp == 0x200D);

        if (!remove)
        {
            out.append(input, i, len);
        }

        i += len;
    }

    return out;
}

std::string UiText(const std::string& value)
{
    return ReplaceGermanUmlauts(StripUnsupportedEmoji(value));
}

std::string DisplayUser(const std::string& username, const std::string& account)
{
    if (!username.empty() && !account.empty()) return username + " [" + account + "]";
    if (!username.empty()) return username;
    if (!account.empty()) return "[" + account + "]";
    return "Unbekannt";
}

std::string CleanEventDescription(std::string text)
{
    size_t pos = text.find("Rollen");
    if (pos != std::string::npos) text = text.substr(0, pos);

    pos = text.find("Teilnehmer");
    if (pos != std::string::npos) text = text.substr(0, pos);

    return text;
}

std::wstring ToWide(const std::string& value)
{
    if (value.empty()) return L"";

    int size = MultiByteToWideChar(CP_UTF8, 0, value.c_str(), -1, nullptr, 0);
    if (size <= 0) return L"";

    std::wstring result(size, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, value.c_str(), -1, &result[0], size);

    if (!result.empty() && result.back() == L'\0') result.pop_back();
    return result;
}

std::string ReadResponse(HINTERNET request)
{
    std::string body;
    DWORD available = 0;

    while (WinHttpQueryDataAvailable(request, &available) && available > 0)
    {
        std::string buffer(available, '\0');
        DWORD downloaded = 0;

        if (!WinHttpReadData(request, (LPVOID)buffer.data(), available, &downloaded)) break;

        buffer.resize(downloaded);
        body += buffer;
    }

    return body;
}

bool HttpGet(const std::string& url, std::string& response, std::string& error)
{
    response.clear();
    error.clear();

    std::wstring wideUrl = ToWide(url);

    if (wideUrl.empty())
    {
        error = "Ungueltige URL.";
        return false;
    }

    URL_COMPONENTS components = {};
    components.dwStructSize = sizeof(components);

    wchar_t host[256] = {};
    wchar_t path[2048] = {};

    components.lpszHostName = host;
    components.dwHostNameLength = ARRAYSIZE(host);
    components.lpszUrlPath = path;
    components.dwUrlPathLength = ARRAYSIZE(path);

    if (!WinHttpCrackUrl(&wideUrl[0], 0, 0, &components))
    {
        error = "URL konnte nicht verarbeitet werden.";
        return false;
    }

    const bool isHttps = components.nScheme == INTERNET_SCHEME_HTTPS;

    HINTERNET session = WinHttpOpen(
        L"LegendaryImpactEventmanager/1.0",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME,
        WINHTTP_NO_PROXY_BYPASS,
        0
    );

    if (!session)
    {
        error = "WinHttpOpen fehlgeschlagen.";
        return false;
    }

    HINTERNET connection = WinHttpConnect(session, components.lpszHostName, components.nPort, 0);

    if (!connection)
    {
        WinHttpCloseHandle(session);
        error = "WinHttpConnect fehlgeschlagen.";
        return false;
    }

    HINTERNET request = WinHttpOpenRequest(
        connection,
        L"GET",
        components.lpszUrlPath,
        nullptr,
        WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES,
        isHttps ? WINHTTP_FLAG_SECURE : 0
    );

    if (!request)
    {
        WinHttpCloseHandle(connection);
        WinHttpCloseHandle(session);
        error = "WinHttpOpenRequest fehlgeschlagen.";
        return false;
    }

    BOOL sent = WinHttpSendRequest(request, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0);

    if (!sent || !WinHttpReceiveResponse(request, nullptr))
    {
        WinHttpCloseHandle(request);
        WinHttpCloseHandle(connection);
        WinHttpCloseHandle(session);
        error = "Request fehlgeschlagen.";
        return false;
    }

    DWORD statusCode = 0;
    DWORD statusSize = sizeof(statusCode);

    WinHttpQueryHeaders(
        request,
        WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
        WINHTTP_HEADER_NAME_BY_INDEX,
        &statusCode,
        &statusSize,
        WINHTTP_NO_HEADER_INDEX
    );

    response = ReadResponse(request);

    WinHttpCloseHandle(request);
    WinHttpCloseHandle(connection);
    WinHttpCloseHandle(session);

    if (statusCode < 200 || statusCode >= 300)
    {
        error = "HTTP Fehler: " + std::to_string(statusCode);
        return false;
    }

    return true;
}

bool ParseIsoUtc(const std::string& value, std::time_t& out)
{
    int y = 0, mon = 0, d = 0, h = 0, min = 0, sec = 0;

    if (sscanf_s(value.c_str(), "%d-%d-%dT%d:%d:%d", &y, &mon, &d, &h, &min, &sec) < 5)
    {
        return false;
    }

    std::tm tm = {};
    tm.tm_year = y - 1900;
    tm.tm_mon = mon - 1;
    tm.tm_mday = d;
    tm.tm_hour = h;
    tm.tm_min = min;
    tm.tm_sec = sec;
    tm.tm_isdst = -1;

    out = _mkgmtime(&tm);
    return out != -1;
}

std::string FormatGermanDateTime(const std::string& value)
{
    static const char* months[] = {
        "Jan.", "Feb.", "Maerz", "Apr.", "Mai", "Juni",
        "Juli", "Aug.", "Sept.", "Okt.", "Nov.", "Dez."
    };

    std::time_t utc = 0;
    if (!ParseIsoUtc(value, utc)) return UiText(value);

    std::tm local = {};
    localtime_s(&local, &utc);

    char buffer[128] = {};
    sprintf_s(
        buffer,
        "%02d. %s %04d %02d:%02d Uhr",
        local.tm_mday,
        months[local.tm_mon],
        local.tm_year + 1900,
        local.tm_hour,
        local.tm_min
    );

    return buffer;
}

std::string FormatLocalNow()
{
    std::time_t now = std::time(nullptr);

    std::tm local = {};
    localtime_s(&local, &now);

    static const char* months[] = {
        "Jan.", "Feb.", "Maerz", "Apr.", "Mai", "Juni",
        "Juli", "Aug.", "Sept.", "Okt.", "Nov.", "Dez."
    };

    char buffer[128] = {};
    sprintf_s(
        buffer,
        "%02d. %s %04d %02d:%02d Uhr",
        local.tm_mday,
        months[local.tm_mon],
        local.tm_year + 1900,
        local.tm_hour,
        local.tm_min
    );

    return buffer;
}

std::string RoleLabel(const std::string& role)
{
    if (role == "BOONDPS") return "Support DPS";
    if (role == "HEAL") return "Heiler";
    if (role == "DPS") return "DPS";
    return role.empty() ? "Keine Rolle" : role;
}

std::string BoonLabel(const std::string& boon)
{
    if (boon == "QUICKNESS") return "Quickness";
    if (boon == "ALACRITY") return "Alacrity";
    return boon;
}

ImVec4 RoleColor(const std::string& role)
{
    if (role == "HEAL") return ImVec4(0.25f, 0.85f, 0.45f, 1.0f);
    if (role == "BOONDPS") return ImVec4(0.95f, 0.55f, 0.20f, 1.0f);
    if (role == "DPS") return ImVec4(1.0f, 0.35f, 0.35f, 1.0f);
    return ImVec4(0.75f, 0.75f, 0.75f, 1.0f);
}

ImVec4 BoonColor(const std::string& boon)
{
    if (boon == "QUICKNESS") return ImVec4(0.95f, 0.75f, 0.25f, 1.0f);
    if (boon == "ALACRITY") return ImVec4(0.45f, 0.75f, 1.0f, 1.0f);
    return ImVec4(0.75f, 0.75f, 0.75f, 1.0f);
}

ImVec4 TagColor(const std::string& tag)
{
    if (tag == "RAID") return ImVec4(0.90f, 0.25f, 0.20f, 1.0f);
    if (tag == "FRACTAL") return ImVec4(0.35f, 0.55f, 1.0f, 1.0f);
    if (tag == "STRIKE") return ImVec4(0.80f, 0.35f, 1.0f, 1.0f);
    if (tag == "OPEN_WORLD") return ImVec4(0.25f, 0.85f, 0.40f, 1.0f);
    if (tag == "WVW") return ImVec4(1.0f, 0.55f, 0.20f, 1.0f);
    if (tag == "PVP") return ImVec4(1.0f, 0.25f, 0.35f, 1.0f);
    if (tag == "MEETING") return ImVec4(0.95f, 0.80f, 0.35f, 1.0f);
    if (tag == "COMMUNITY") return ImVec4(0.30f, 0.90f, 0.80f, 1.0f);
    return ImVec4(0.75f, 0.75f, 0.75f, 1.0f);
}

std::string TagLabel(const std::string& tag)
{
    if (tag == "OPEN_WORLD") return "Open World";
    if (tag == "WVW") return "WvW";
    if (tag == "PVP") return "PvP";
    if (tag == "RAID") return "Raid";
    if (tag == "FRACTAL") return "Fraktal";
    if (tag == "STRIKE") return "Strike";
    if (tag == "MEETING") return "Besprechung";
    if (tag == "COMMUNITY") return "Community";
    return tag;
}

std::string StripSimpleMarkdown(std::string text)
{
    const char* tokens[] = { "**", "__", "`" };

    for (const char* token : tokens)
    {
        size_t pos = 0;
        size_t len = std::strlen(token);

        while ((pos = text.find(token, pos)) != std::string::npos)
        {
            text.erase(pos, len);
        }
    }

    return UiText(text);
}

void RenderMarkdownText(const std::string& text)
{
    std::string line;

    auto renderLine = [](const std::string& raw)
        {
            std::string line = StripSimpleMarkdown(raw);

            if (line.empty())
            {
                ImGui::Spacing();
                return;
            }

            if (line == "---")
            {
                return;
            }

            if (line.rfind("### ", 0) == 0)
            {
                ImGui::TextColored(ImVec4(0.90f, 0.75f, 0.35f, 1.0f), "%s", line.substr(4).c_str());
            }
            else if (line.rfind("## ", 0) == 0)
            {
                ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.45f, 1.0f), "%s", line.substr(3).c_str());
            }
            else if (line.rfind("# ", 0) == 0)
            {
                ImGui::TextColored(ImVec4(1.0f, 0.9f, 0.55f, 1.0f), "%s", line.substr(2).c_str());
            }
            else if (line.rfind("- ", 0) == 0 || line.rfind("* ", 0) == 0)
            {
                ImGui::BulletText("%s", line.substr(2).c_str());
            }
            else
            {
                ImGui::TextWrapped("%s", line.c_str());
            }
        };

    for (char c : text)
    {
        if (c == '\n')
        {
            renderLine(line);
            line.clear();
        }
        else
        {
            line += c;
        }
    }

    if (!line.empty()) renderLine(line);
}

void CopyToClipboard(const std::string& text)
{
    if (!OpenClipboard(nullptr)) return;

    EmptyClipboard();

    HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, text.size() + 1);

    if (hMem)
    {
        void* ptr = GlobalLock(hMem);

        if (ptr)
        {
            memcpy(ptr, text.c_str(), text.size() + 1);
            GlobalUnlock(hMem);
            SetClipboardData(CF_TEXT, hMem);
        }
    }

    CloseClipboard();
}

void SaveSettings()
{
    _mkdir("addons");
    _mkdir(SETTINGS_DIR);

    auto config = std::atomic_load(&g_Config);

    json data;
    data["token"] = config->token;
    data["refreshMinutes"] = config->refreshMinutes;
    data["showWindow"] = g_ShowWindow.load();

    data["reminderEnabled"] = config->reminderEnabled;
    data["reminderMinutesBefore"] = config->reminderMinutesBefore;
    data["reminderRepeatMinutes"] = config->reminderRepeatMinutes;

    std::ofstream file(SETTINGS_FILE);
    if (file.is_open()) file << data.dump(4);
}

void LoadSettings()
{
    std::ifstream file(SETTINGS_FILE);
    if (!file.is_open()) return;

    try
    {
        json data;
        file >> data;

        auto config = std::make_shared<PluginConfig>();

        config->token = data.value("token", "");
        config->refreshMinutes = data.value("refreshMinutes", 5);

        if (config->refreshMinutes < 5) config->refreshMinutes = 5;

        config->reminderEnabled = data.value("reminderEnabled", true);
        config->reminderMinutesBefore = data.value("reminderMinutesBefore", 15);
        config->reminderRepeatMinutes = data.value("reminderRepeatMinutes", 5);

        if (config->reminderMinutesBefore < 1) config->reminderMinutesBefore = 1;
        if (config->reminderRepeatMinutes < 1) config->reminderRepeatMinutes = 1;

        std::atomic_store(&g_Config, config);

        strcpy_s(g_EditToken, config->token.c_str());
        g_EditRefreshMinutes = config->refreshMinutes;

        g_EditReminderEnabled = config->reminderEnabled;
        g_EditReminderMinutesBefore = config->reminderMinutesBefore;
        g_EditReminderRepeatMinutes = config->reminderRepeatMinutes;

        g_ShowWindow = data.value("showWindow", true);
    }
    catch (...) {}
}

void StoreError(const std::string& error)
{
    auto oldState = std::atomic_load(&g_State);
    auto nextState = std::make_shared<PluginState>();

    if (oldState)
    {
        nextState->events = oldState->events;
        nextState->viewerUsername = oldState->viewerUsername;
        nextState->viewerGw2Account = oldState->viewerGw2Account;
        nextState->lastSync = oldState->lastSync;
    }

    nextState->lastSync = "Fehler: " + UiText(error);
    std::atomic_store(&g_State, nextState);
}

std::string BuildEventsUrl(const PluginConfig& config)
{
    if (!config.token.empty())
    {
        return std::string(API_BASE_URL) + "/api/nexus/events/" + config.token;
    }

    return std::string(API_BASE_URL) + "/api/nexus/events/public";
}

void ShowReminder(const std::string& title, const std::string& date, int minutesUntilStart)
{
    g_ReminderTitle = UiText(title);
    g_ReminderDate = UiText(date);
    g_ReminderTimeLeft = "ca. " + std::to_string(minutesUntilStart) + " Minuten";
    g_ShowReminderMessage = true;

    PlaySoundA("SystemExclamation", nullptr, SND_ALIAS | SND_ASYNC);
}

void CheckEventReminders(const PluginState& state)
{
    auto config = std::atomic_load(&g_Config);

    if (!config || !config->reminderEnabled)
    {
        return;
    }

    std::time_t now = std::time(nullptr);
    int beforeSeconds = config->reminderMinutesBefore * 60;
    int repeatSeconds = config->reminderRepeatMinutes * 60;

    for (const auto& event : state.events)
    {
        std::time_t startTime = 0;

        if (!ParseIsoUtc(event.start, startTime))
        {
            continue;
        }

        int secondsUntilStart = (int)std::difftime(startTime, now);

        if (secondsUntilStart < 0 || secondsUntilStart > beforeSeconds)
        {
            continue;
        }

        std::time_t lastShown = g_ReminderLastShown[event.id];

        if (lastShown > 0 && std::difftime(now, lastShown) < repeatSeconds)
        {
            continue;
        }

        g_ReminderLastShown[event.id] = now;

        int minutesUntilStart = secondsUntilStart / 60;
        if (minutesUntilStart < 1) minutesUntilStart = 1;

        ShowReminder(
            event.title,
            FormatGermanDateTime(event.start),
            minutesUntilStart
        );

        break;
    }
}

void RenderReminderMessage()
{
    if (!g_ShowReminderMessage)
    {
        return;
    }

    ImGui::SetNextWindowSize(ImVec2(460.0f, 0.0f), ImGuiCond_Appearing);
    ImGui::SetNextWindowPos(ImVec2(40.0f, 120.0f), ImGuiCond_Appearing);

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(18.0f, 16.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(10.0f, 10.0f));

    if (ImGui::Begin(
        "Legendary Impact - Eventmanager###LegendaryImpactReminder",
        &g_ShowReminderMessage,
        ImGuiWindowFlags_AlwaysAutoResize |
        ImGuiWindowFlags_NoCollapse
    ))
    {
        ImGui::TextColored(ImVec4(1.0f, 0.82f, 0.35f, 1.0f), "Reminder: Ein Event startet bald!");
        ImGui::Separator();

        ImGui::TextDisabled("Event:");
        ImGui::SameLine();
        ImGui::TextWrapped("%s", g_ReminderTitle.c_str());

        ImGui::TextDisabled("Datum:");
        ImGui::SameLine();
        ImGui::TextWrapped("%s", g_ReminderDate.c_str());

        ImGui::TextDisabled("Zeit bis Start:");
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.95f, 0.80f, 0.35f, 1.0f), "%s", g_ReminderTimeLeft.c_str());

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        float buttonWidth = 120.0f;
        float windowWidth = ImGui::GetWindowSize().x;
        float cursorX = (windowWidth - buttonWidth) * 0.5f;

        if (cursorX > 0.0f)
        {
            ImGui::SetCursorPosX(cursorX);
        }

        if (ImGui::Button("OK", ImVec2(buttonWidth, 0.0f)))
        {
            g_ShowReminderMessage = false;
        }
    }

    ImGui::End();

    ImGui::PopStyleVar(2);
}

void FetchEvents()
{
    if (g_Fetching.exchange(true)) return;

    auto config = std::atomic_load(&g_Config);

    std::string body;
    std::string error;
    std::string url = BuildEventsUrl(*config);

    if (!HttpGet(url, body, error))
    {
        StoreError(error);
        g_Fetching = false;
        return;
    }

    try
    {
        json data = json::parse(body);
        auto nextState = std::make_shared<PluginState>();

        nextState->lastSync = FormatLocalNow();

        if (data.contains("viewer") && data["viewer"].is_object())
        {
            nextState->viewerUsername = UiText(JsonString(data["viewer"], "username"));
            nextState->viewerGw2Account = UiText(JsonString(data["viewer"], "gw2Account"));
        }

        if (data.contains("events") && data["events"].is_array())
        {
            for (const auto& item : data["events"])
            {
                EventItem event;

                event.id = JsonString(item, "id");
                event.title = UiText(JsonString(item, "title", "Unbenannt"));
                event.description = UiText(JsonString(item, "description"));
                event.location = UiText(JsonString(item, "location"));
                event.start = JsonString(item, "start");
                event.end = JsonString(item, "end");
                event.tag = JsonString(item, "tag");
                event.url = JsonString(item, "url");
                event.isViewerAttending = item.value("isViewerAttending", false);

                if (item.contains("creator") && item["creator"].is_object())
                {
                    event.leaderName = UiText(JsonString(item["creator"], "username"));
                    event.leaderAccount = UiText(JsonString(item["creator"], "gw2Account"));
                }

                if (item.contains("_count") && item["_count"].contains("attendees"))
                {
                    event.attendeeCount = item["_count"].value("attendees", 0);
                }

                if (item.contains("roleSlots") && item["roleSlots"].is_array())
                {
                    for (const auto& slot : item["roleSlots"])
                    {
                        event.slotCount += slot.value("count", 0);
                    }
                }

                if (item.contains("attendees") && item["attendees"].is_array())
                {
                    for (const auto& attendeeJson : item["attendees"])
                    {
                        EventAttendee attendee;

                        if (attendeeJson.contains("user") && attendeeJson["user"].is_object())
                        {
                            attendee.username = UiText(JsonString(attendeeJson["user"], "username"));
                            attendee.gw2Account = UiText(JsonString(attendeeJson["user"], "gw2Account"));
                        }

                        if (attendeeJson.contains("slot") && attendeeJson["slot"].is_object())
                        {
                            attendee.role = JsonString(attendeeJson["slot"], "group");
                        }

                        attendee.boon = JsonString(attendeeJson, "selectedBoon");

                        if (attendeeJson.contains("flexRoles") && attendeeJson["flexRoles"].is_array())
                        {
                            for (const auto& flexJson : attendeeJson["flexRoles"])
                            {
                                std::string group = JsonString(flexJson, "group");
                                std::string boon = JsonString(flexJson, "boon");

                                if (!group.empty() || !boon.empty())
                                {
                                    EventFlexRole flexRole;
                                    flexRole.role = group;
                                    flexRole.boon = boon;
                                    attendee.flexRoles.push_back(flexRole);
                                }
                            }
                        }

                        event.attendees.push_back(attendee);
                    }
                }

                nextState->events.push_back(event);
            }
        }

        std::atomic_store(&g_State, nextState);
    }
    catch (const std::exception& ex)
    {
        StoreError(std::string("JSON Fehler: ") + ex.what());
    }

    g_Fetching = false;
}

void WorkerLoop()
{
    FetchEvents();

    while (g_Running)
    {
        auto config = std::atomic_load(&g_Config);
        int minutes = config ? config->refreshMinutes : 5;
        if (minutes < 5) minutes = 5;

        auto nextWake =
            std::chrono::steady_clock::now() +
            std::chrono::minutes(minutes);

        std::unique_lock<std::mutex> lock(g_WorkerMutex);

        g_WorkerWake.wait_until(
            lock,
            nextWake,
            []()
            {
                return !g_Running.load() || g_ManualSyncRequested.load();
            }
        );

        if (!g_Running)
        {
            break;
        }

        g_ManualSyncRequested = false;

        lock.unlock();

        FetchEvents();
    }
}

void RenderRoleWithBoon(const std::string& role, const std::string& boon)
{
    ImGui::TextColored(RoleColor(role), "%s", RoleLabel(role).c_str());

    if (!boon.empty())
    {
        ImGui::SameLine();
        ImGui::TextColored(BoonColor(boon), "[%s]", BoonLabel(boon).c_str());
    }
}

void RenderEventAttendeesTable(const EventItem& event)
{
    if (event.attendees.empty())
    {
        ImGui::TextDisabled("Keine Teilnehmerdaten vorhanden.");
        return;
    }

    if (ImGui::BeginTable(
        "attendeesTable",
        4,
        ImGuiTableFlags_Borders |
        ImGuiTableFlags_RowBg |
        ImGuiTableFlags_SizingStretchProp
    ))
    {
        ImGui::TableSetupColumn("Spieler");
        ImGui::TableSetupColumn("Rolle");
        ImGui::TableSetupColumn("Boon");
        ImGui::TableSetupColumn("Flex");
        ImGui::TableHeadersRow();

        for (const auto& attendee : event.attendees)
        {
            ImGui::TableNextRow();

            ImGui::TableSetColumnIndex(0);
            ImGui::TextUnformatted(DisplayUser(attendee.username, attendee.gw2Account).c_str());

            ImGui::TableSetColumnIndex(1);
            ImGui::TextColored(RoleColor(attendee.role), "%s", RoleLabel(attendee.role).c_str());

            ImGui::TableSetColumnIndex(2);

            if (!attendee.boon.empty())
            {
                ImGui::TextColored(BoonColor(attendee.boon), "%s", BoonLabel(attendee.boon).c_str());
            }
            else
            {
                ImGui::TextDisabled("-");
            }

            ImGui::TableSetColumnIndex(3);

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

std::string ViewerLabel(const PluginState& state)
{
    if (!state.viewerGw2Account.empty())
    {
        if (!state.viewerUsername.empty())
        {
            return state.viewerUsername + " [" + state.viewerGw2Account + "]";
        }

        return state.viewerGw2Account;
    }

    return "Public";
}

void RenderEventsWindow()
{
    auto state = std::atomic_load(&g_State);
    if (!state) return;

    ImGui::TextUnformatted("Angemeldet als:");
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(0.95f, 0.80f, 0.35f, 1.0f), "%s", ViewerLabel(*state).c_str());

    ImGui::Text("Letzter Sync: %s", state->lastSync.c_str());

    if (!g_Fetching && ImGui::Button("Jetzt synchronisieren"))
    {
        RequestSyncNow();
    }

    ImGui::SameLine();

    if (g_Fetching)
    {
        ImGui::TextDisabled("Synchronisiere...");
    }
    else
    {
        ImGui::TextDisabled("Auto Sync aktiv");
    }

    if (state->events.empty())
    {
        ImGui::TextDisabled("Keine Events vorhanden.");
        return;
    }

    if (ImGui::BeginTable(
        "events",
        8,
        ImGuiTableFlags_Borders |
        ImGuiTableFlags_RowBg |
        ImGuiTableFlags_Resizable |
        ImGuiTableFlags_SizingStretchProp
    ))
    {
        ImGui::TableSetupColumn("Event", ImGuiTableColumnFlags_WidthStretch, 2.6f);
        ImGui::TableSetupColumn("Zeit", ImGuiTableColumnFlags_WidthStretch, 1.2f);
        ImGui::TableSetupColumn("Ort", ImGuiTableColumnFlags_WidthStretch, 1.2f);
        ImGui::TableSetupColumn("Leiter", ImGuiTableColumnFlags_WidthStretch, 1.3f);
        ImGui::TableSetupColumn("Tag", ImGuiTableColumnFlags_WidthFixed, 90.0f);
        ImGui::TableSetupColumn("Teilnehmer", ImGuiTableColumnFlags_WidthStretch, 2.2f);
        ImGui::TableSetupColumn("Angemeldet", ImGuiTableColumnFlags_WidthFixed, 95.0f);
        ImGui::TableSetupColumn("Aktionen", ImGuiTableColumnFlags_WidthFixed, 170.0f);
        ImGui::TableHeadersRow();

        for (const auto& event : state->events)
        {
            ImGui::PushID(event.id.c_str());
            ImGui::TableNextRow();

            ImGui::TableSetColumnIndex(0);

            if (ImGui::TreeNodeEx("event", ImGuiTreeNodeFlags_SpanFullWidth, "%s", event.title.c_str()))
            {
                std::string cleanDescription = CleanEventDescription(event.description);

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
            ImGui::TextWrapped("%s", FormatGermanDateTime(event.start).c_str());

            ImGui::TableSetColumnIndex(2);

            if (!event.location.empty())
            {
                ImGui::TextWrapped("%s", event.location.c_str());
            }
            else
            {
                ImGui::TextDisabled("-");
            }

            ImGui::TableSetColumnIndex(3);

            if (!event.leaderName.empty() || !event.leaderAccount.empty())
            {
                ImGui::TextWrapped("%s", DisplayUser(event.leaderName, event.leaderAccount).c_str());
            }
            else
            {
                ImGui::TextDisabled("-");
            }

            ImGui::TableSetColumnIndex(4);
            ImGui::TextColored(TagColor(event.tag), "%s", TagLabel(event.tag).c_str());

            ImGui::TableSetColumnIndex(5);

            std::string attendeeLabel = std::to_string(event.attendeeCount) + "/" + std::to_string(event.slotCount);

            if (ImGui::TreeNodeEx(attendeeLabel.c_str(), ImGuiTreeNodeFlags_SpanFullWidth))
            {
                RenderEventAttendeesTable(event);
                ImGui::TreePop();
            }

            ImGui::TableSetColumnIndex(6);

            if (event.isViewerAttending)
            {
                ImGui::TextColored(ImVec4(0.20f, 0.90f, 0.30f, 1.0f), "Ja");
            }
            else
            {
                ImGui::TextColored(ImVec4(1.0f, 0.25f, 0.25f, 1.0f), "Nein");
            }

            ImGui::TableSetColumnIndex(7);

            if (!event.url.empty() && ImGui::Button("Web"))
            {
                ShellExecuteA(nullptr, "open", event.url.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
            }

            if (!event.leaderAccount.empty())
            {
                ImGui::SameLine();

                if (ImGui::Button("SqJoin"))
                {
                    CopyToClipboard("/sqjoin " + event.leaderAccount);
                }
            }

            ImGui::PopID();
        }

        ImGui::EndTable();
    }
}

void AddonRender()
{
    auto state = std::atomic_load(&g_State);
    if (state)
    {
        CheckEventReminders(*state);
    }

    RenderReminderMessage();

    bool show = g_ShowWindow.load();
    if (!show) return;

    std::string title = "Legendary Impact - Eventmanager###LegendaryImpactEventmanagerWindow";

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(14.0f, 14.0f));

    if (!ImGui::Begin(title.c_str(), &show))
    {
        g_ShowWindow = show;
        ImGui::End();
        ImGui::PopStyleVar();
        return;
    }

    g_ShowWindow = show;
    RenderEventsWindow();

    ImGui::End();
    ImGui::PopStyleVar();
}

void ApplySettings()
{
    auto nextConfig = std::make_shared<PluginConfig>();

    nextConfig->token = g_EditToken;
    nextConfig->refreshMinutes = g_EditRefreshMinutes < 5 ? 5 : g_EditRefreshMinutes;

    nextConfig->reminderEnabled = g_EditReminderEnabled;
    nextConfig->reminderMinutesBefore = g_EditReminderMinutesBefore < 1 ? 1 : g_EditReminderMinutesBefore;
    nextConfig->reminderRepeatMinutes = g_EditReminderRepeatMinutes < 1 ? 1 : g_EditReminderRepeatMinutes;

    std::atomic_store(&g_Config, nextConfig);
    SaveSettings();

    RequestSyncNow();
}

void AddonOptions()
{
    ImGui::Separator();
    ImGui::Text("Legendary Impact - Eventmanager");

    auto state = std::atomic_load(&g_State);

    if (state)
    {
        ImGui::TextUnformatted("Angemeldet als:");
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.95f, 0.80f, 0.35f, 1.0f), "%s", ViewerLabel(*state).c_str());
    }

    bool show = g_ShowWindow.load();

    if (ImGui::Checkbox("Fenster anzeigen", &show))
    {
        g_ShowWindow = show;
        SaveSettings();
    }

    ImGui::InputText("Legendary Impact Token", g_EditToken, IM_ARRAYSIZE(g_EditToken), ImGuiInputTextFlags_Password);

    ImGui::SliderInt("Auto Sync Intervall Minuten", &g_EditRefreshMinutes, 5, 60);
    if (g_EditRefreshMinutes < 5) g_EditRefreshMinutes = 5;

    ImGui::TextDisabled("Keybind: bitte in den Nexus Keybind-Einstellungen fuer Legendary Impact - Eventmanager setzen.");

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Text("Reminder");

    ImGui::Checkbox("Reminder aktivieren", &g_EditReminderEnabled);

    ImGui::SliderInt("Reminder Minuten vor Event", &g_EditReminderMinutesBefore, 1, 120);
    if (g_EditReminderMinutesBefore < 1) g_EditReminderMinutesBefore = 1;

    ImGui::SliderInt("Reminder wiederholen alle Minuten", &g_EditReminderRepeatMinutes, 1, 60);
    if (g_EditReminderRepeatMinutes < 1) g_EditReminderRepeatMinutes = 1;

    if (ImGui::Button("Test Reminder"))
    {
        ShowReminder("Wing 4 Fullclear (Auch fuer Anfaenger)", FormatLocalNow(), 15);
    }

    ImGui::Spacing();

    if (ImGui::Button("Einstellungen speichern"))
    {
        ApplySettings();
    }
}

void OnInputBind(const char* aIdentifier, bool aIsRelease)
{
    if (aIsRelease) return;
    if (!aIdentifier) return;

    if (std::strcmp(aIdentifier, KB_ID) != 0) return;

    g_ShowWindow = !g_ShowWindow.load();
    SaveSettings();
}

void AddonLoad(AddonAPI* aApi)
{
    APIDefs = aApi;

    ImGui::SetCurrentContext((ImGuiContext*)APIDefs->ImguiContext);

    ImGui::SetAllocatorFunctions(
        (void* (*)(size_t, void*))APIDefs->ImguiMalloc,
        (void (*)(void*, void*))APIDefs->ImguiFree
    );

    NexusLink = (NexusLinkData*)APIDefs->DataLink.Get("DL_NEXUS_LINK");
    MumbleLink = (Mumble::Data*)APIDefs->DataLink.Get("DL_MUMBLE_LINK");

    APIDefs->Textures.LoadFromResource(ICON_ID, IDB_PNG1, hSelf, nullptr);
    APIDefs->Textures.LoadFromResource(ICON_HOVER_ID, IDB_PNG2, hSelf, nullptr);

    APIDefs->InputBinds.RegisterWithString(KB_ID, OnInputBind, "F8");

    APIDefs->QuickAccess.Add(
        QA_ID,
        ICON_ID,
        ICON_HOVER_ID,
        KB_ID,
        "Legendary Impact - Eventmanager"
    );

    APIDefs->Renderer.Register(ERenderType_Render, AddonRender);
    APIDefs->Renderer.Register(ERenderType_OptionsRender, AddonOptions);

    LoadSettings();

    g_Running = true;
    g_Worker = std::thread(WorkerLoop);

    APIDefs->Log(ELogLevel_DEBUG, name, "<c=#00ff00>Legendary Impact - Eventmanager</c> was loaded.");
}

void AddonUnload()
{
    SaveSettings();

    g_Running = false;
    g_ManualSyncRequested = true;
    g_WorkerWake.notify_one();

    if (g_Worker.joinable())
    {
        g_Worker.join();
    }

    APIDefs->QuickAccess.Remove(QA_ID);
    APIDefs->InputBinds.Deregister(KB_ID);

    APIDefs->Renderer.Deregister(AddonRender);
    APIDefs->Renderer.Deregister(AddonOptions);

    APIDefs->Log(
        ELogLevel_DEBUG,
        name,
        "Signing off <c=#ff0000>Legendary Impact - Eventmanager</c>, it was an honor commander."
    );
}
