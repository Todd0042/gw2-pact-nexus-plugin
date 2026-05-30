#pragma once
#include <string>
#include <vector>

namespace LegendaryImpactEventmanager
{
    struct PluginConfig
    {
        std::string token = "";
        int refreshMinutes = 5;
        bool reminderEnabled = true;
        bool announceNewEventsEnabled = true;
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
        bool isPublic = false;
        bool isViewerAttending = false;
        int attendeeCount = 0;
        int slotCount = 0;
        std::vector<EventAttendee> attendees;
    };

    struct SquadMember {
        std::string accountName;
        std::string characterName;
        uint32_t subgroup = 0;            // 0 for parties, 1-15 according to the squad's subgroup
        uint32_t profession = 0;          // 1-9 = Profession; 0 Unknown -> e.g. on loading screen or logged out
        uint32_t eliteSpecialization = 0; // Third Spec ID, not nec
        bool isCommander = false;
        bool isLieutenant = false;
        bool isSelf = false;
        bool isInInstance = false;
        std::string normalizedAccountName;
    };

    struct PluginState
    {
        std::string lastSync = "-";
        std::string viewerUsername = "";
        std::string viewerGw2Account = "";
        std::vector<EventItem> events;
        std::vector<std::string> newEventIds;
        std::vector<SquadMember> squadMembers;
    };
}
