#pragma once

#include "Models.h"
#include "SharedState.h"
#include "RTAPI/RTAPI.hpp"

#include <string>

namespace LegendaryImpactEventmanager
{
    class SquadManager
    {
    public:
        explicit SquadManager(SharedState& sharedState);

        void UpdateMember(RTAPI::GroupMember* groupMember);
        void UpdateMember(const SquadMember& member);
        void RemoveMember(RTAPI::GroupMember* groupMember);
        void RemoveMemberByAccount(const std::string& accountName);
        void Clear();

        bool IsInSquad(const std::string& accountName) const;

        static std::string NormalizeAccountName(std::string value);

    private:
        SharedState& m_SharedState;
    };
}