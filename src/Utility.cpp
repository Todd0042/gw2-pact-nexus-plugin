#include "Utility.h"
#include <cstdio>
#include <cstring>
#include <string>
#include <algorithm>
#include <cctype>
#include <cstdint>

namespace LegendaryImpactEventmanager::Utility
{
    void ReplaceAll(std::string& value, const std::string& from, const std::string& to)
    {
        size_t pos = 0;
        while ((pos = value.find(from, pos)) != std::string::npos)
        {
            value.replace(pos, from.length(), to);
            pos += to.length();
        }
    }

    std::string StripEmojis(const std::string& input)
    {
        std::string out;
        out.reserve(input.size());

        for (size_t i = 0; i < input.size();)
        {
            unsigned char c = static_cast<unsigned char>(input[i]);

            uint32_t cp = 0;
            size_t len = 0;

            if (c < 0x80) { cp = c; len = 1; }
            else if ((c & 0xE0) == 0xC0 && i + 1 < input.size())
            {
                cp = ((c & 0x1F) << 6) |
                    (static_cast<unsigned char>(input[i + 1]) & 0x3F);
                len = 2;
            }
            else if ((c & 0xF0) == 0xE0 && i + 2 < input.size())
            {
                cp = ((c & 0x0F) << 12) |
                    ((static_cast<unsigned char>(input[i + 1]) & 0x3F) << 6) |
                    (static_cast<unsigned char>(input[i + 2]) & 0x3F);
                len = 3;
            }
            else if ((c & 0xF8) == 0xF0 && i + 3 < input.size())
            {
                cp = ((c & 0x07) << 18) |
                    ((static_cast<unsigned char>(input[i + 1]) & 0x3F) << 12) |
                    ((static_cast<unsigned char>(input[i + 2]) & 0x3F) << 6) |
                    (static_cast<unsigned char>(input[i + 3]) & 0x3F);
                len = 4;
            }
            else
            {
                ++i;
                continue;
            }

            bool isEmoji =
                (cp >= 0x1F000 && cp <= 0x1FFFF) ||
                (cp >= 0x2600 && cp <= 0x27BF) ||
                (cp >= 0x2300 && cp <= 0x23FF) ||
                (cp >= 0x2B00 && cp <= 0x2BFF) ||
                (cp >= 0xFE00 && cp <= 0xFE0F) ||
                (cp >= 0x1F3FB && cp <= 0x1F3FF) ||
                (cp == 0x200D);

            if (!isEmoji)
                out.append(input, i, len);

            i += len;
        }

        return out;
    }

    std::string FixSpacesAfterEmojiStrip(std::string text)
    {
        std::string out;
        out.reserve(text.size());

        bool lineStart = true;
        bool lastWasSpace = false;

        for (char ch : text)
        {
            if (ch == '\r')
                continue;

            if (ch == '\n')
            {
                while (!out.empty() && out.back() == ' ')
                    out.pop_back();

                out += '\n';
                lineStart = true;
                lastWasSpace = false;
                continue;
            }

            if (ch == ' ')
            {
                if (lineStart)
                    continue;

                if (!lastWasSpace)
                {
                    out += ' ';
                    lastWasSpace = true;
                }

                continue;
            }

            out += ch;
            lineStart = false;
            lastWasSpace = false;
        }

        while (!out.empty() && out.back() == ' ')
            out.pop_back();

        return out;
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
        text = StripEmojis(text);
        text = FixSpacesAfterEmojiStrip(text);

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

    bool ParseIsoUtc(const std::string& value, std::time_t& out)
    {
        int y = 0, mon = 0, d = 0, h = 0, min = 0, sec = 0;
        if (sscanf_s(value.c_str(), "%d-%d-%dT%d:%d:%d", &y, &mon, &d, &h, &min, &sec) < 5) return false;
        std::tm tm = {};
        tm.tm_year = y - 1900; tm.tm_mon = mon - 1; tm.tm_mday = d;
        tm.tm_hour = h; tm.tm_min = min; tm.tm_sec = sec; tm.tm_isdst = -1;
        out = _mkgmtime(&tm);
        return out != -1;
    }

    std::string FormatGermanDateTime(const std::string& value)
    {
        static const char* months[] = { "Jan.", "Feb.", "Maerz", "Apr.", "Mai", "Juni", "Juli", "Aug.", "Sept.", "Okt.", "Nov.", "Dez." };
        std::time_t utc = 0;
        if (!ParseIsoUtc(value, utc)) return value;
        std::tm local = {};
        localtime_s(&local, &utc);
        char buffer[128] = {};
        sprintf_s(buffer, "%02d. %s %04d - %02d:%02d Uhr", local.tm_mday, months[local.tm_mon], local.tm_year + 1900, local.tm_hour, local.tm_min);
        return buffer;
    }

    std::string FormatLocalNow()
    {
        std::time_t now = std::time(nullptr);
        std::tm utc = {};
        gmtime_s(&utc, &now);
        char iso[64] = {};
        sprintf_s(iso, "%04d-%02d-%02dT%02d:%02d:%02d", utc.tm_year + 1900, utc.tm_mon + 1, utc.tm_mday, utc.tm_hour, utc.tm_min, utc.tm_sec);
        return FormatGermanDateTime(iso);
    }

    bool IsEventActive(const EventItem& event)
    {
        std::time_t startTime = 0, endTime = 0;
        if (!ParseIsoUtc(event.start, startTime) || !ParseIsoUtc(event.end, endTime)) return false;
        std::time_t now = std::time(nullptr);
        return now >= startTime && now <= endTime;
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

    std::string StripSimpleMarkdown(std::string text)
    {
        const char* tokens[] = { "**", "__", "`" };
        for (const char* token : tokens)
        {
            size_t pos = 0; size_t len = std::strlen(token);
            while ((pos = text.find(token, pos)) != std::string::npos) text.erase(pos, len);
        }
        return text;
    }

    void CopyToClipboard(const std::string& text)
    {
        if (!OpenClipboard(nullptr)) return;
       
        EmptyClipboard();
        HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, text.size() + 1);

        if (!hMem)
        {
            CloseClipboard();
            return;
        }

        void* ptr = GlobalLock(hMem);

        if (!ptr)
        {
            GlobalFree(hMem);
            CloseClipboard();
            return;
        }

        memcpy(ptr, text.c_str(), text.size() + 1);
        GlobalUnlock(hMem);

        if (!SetClipboardData(CF_TEXT, hMem))
        {
            GlobalFree(hMem);
        }

        CloseClipboard();
    }
}
