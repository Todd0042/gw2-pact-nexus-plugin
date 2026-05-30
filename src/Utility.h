#pragma once
#include "Models.h"
#include <ctime>
#include <string>
#include <Windows.h>
#include "imgui/imgui.h"

namespace LegendaryImpactEventmanager::Utility
{
    void ReplaceAll(std::string& value, const std::string& from, const std::string& to);
    std::string StripUnsupportedEmoji(const std::string& input);
    std::string DisplayUser(const std::string& username, const std::string& account);
    std::string CleanEventDescription(std::string text);
    std::wstring ToWide(const std::string& value);
    bool ParseIsoUtc(const std::string& value, std::time_t& out);
    std::string FormatGermanDateTime(const std::string& value);
    std::string FormatLocalNow();
    bool IsEventActive(const EventItem& event);
    std::string RoleLabel(const std::string& role);
    std::string BoonLabel(const std::string& boon);
    std::string TagLabel(const std::string& tag);
    ImVec4 TagColor(const std::string& tag);
    std::string StripSimpleMarkdown(std::string text);
    void CopyToClipboard(const std::string& text);
}
