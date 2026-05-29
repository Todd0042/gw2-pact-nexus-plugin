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

        m_SharedState.UpdateState([&](PluginState& state) {
            const std::string accountName = NormalizeAccountName(member.accountName);

            auto it = std::find_if(
                state.squadMembers.begin(),
                state.squadMembers.end(),
                [&](const SquadMember& existing) {
                    return NormalizeAccountName(existing.accountName) == accountName;
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

        const std::string accountName = groupMember->AccountName;

        m_SharedState.UpdateState([&](PluginState& state) {
            const std::string normalized = NormalizeAccountName(accountName);

            std::erase_if(state.squadMembers, [&](const SquadMember& member) {
                return NormalizeAccountName(member.accountName) == normalized;
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
        auto state = m_SharedState.GetState();
        if (!state) return false;

        const std::string normalized = NormalizeAccountName(accountName);

        return std::any_of(
            state->squadMembers.begin(),
            state->squadMembers.end(),
            [&](const SquadMember& member) {
                return NormalizeAccountName(member.accountName) == normalized;
            });
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