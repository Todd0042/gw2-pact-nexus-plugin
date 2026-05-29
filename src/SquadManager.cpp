#include "SquadManager.h"
#include <algorithm>
#include <cctype>

namespace LegendaryImpactEventmanager
{
    SquadManager::SquadManager(SharedState& sharedState)
        : m_SharedState(sharedState) {}

    void SquadManager::UpdateMember(RTAPI::GroupMember* groupMember)
    {
        if (!groupMember) return;

        SquadMember member;
        member.accountName = groupMember->AccountName;
        member.characterName = groupMember->CharacterName;
        member.subgroup = groupMember->Subgroup;
        member.profession = groupMember->Profession;
        member.eliteSpecialization = groupMember->EliteSpecialization;
        member.isCommander = groupMember->IsCommander;
        member.isLieutenant = groupMember->IsLieutenant;
        member.isSelf = groupMember->IsSelf;
        member.isInInstance = groupMember->IsInInstance;
        member.normalizedAccountName = NormalizeAccountName(member.accountName);

        UpdateMember(member);
    }

    void SquadManager::UpdateMember(const SquadMember& member)
    {
        if (member.normalizedAccountName.empty()) return;

        m_SharedState.UpdateState([&](PluginState& state) {
            const std::string& accountName = member.normalizedAccountName;

            auto it = std::find_if(
                state.squadMembers.begin(),
                state.squadMembers.end(),
                [&](const SquadMember& existing) {
                    return existing.normalizedAccountName == accountName;
                });

            if (it == state.squadMembers.end())
            {
                state.squadMembers.push_back(member);
                return;
            }

            *it = member;
            });
    }

    void SquadManager::RemoveMember(RTAPI::GroupMember* groupMember)
    {
        if (!groupMember) return;
        RemoveMemberByAccount(groupMember->AccountName);
    }

    void SquadManager::RemoveMemberByAccount(const std::string& accountName)
    {
        const std::string normalized = NormalizeAccountName(accountName);
        if (normalized.empty()) return;

        m_SharedState.UpdateState([&](PluginState& state) {
            std::erase_if(state.squadMembers, [&](const SquadMember& member) {
                return member.normalizedAccountName == normalized;
                });
            });
    }

    void SquadManager::Clear()
    {
        m_SharedState.UpdateState([](PluginState& state) {
            state.squadMembers.clear();
            });
    }

    bool SquadManager::IsInSquad(const std::string& accountName) const
    {
        const std::string normalized = NormalizeAccountName(accountName);
        bool found = false;

        m_SharedState.WithStateRead([&](const PluginState& state) {
            found = std::any_of(
                state.squadMembers.begin(),
                state.squadMembers.end(),
                [&](const SquadMember& member) {
                    return member.normalizedAccountName == normalized;
                });
            });

        return found;
    }

    std::string SquadManager::NormalizeAccountName(std::string value)
    {
        if (!value.empty() && value[0] == ':')
        {
            value.erase(0, 1);
        }

        std::transform(
            value.begin(),
            value.end(),
            value.begin(),
            [](unsigned char c) {
                return static_cast<char>(std::tolower(c));
            });

        return value;
    }
}