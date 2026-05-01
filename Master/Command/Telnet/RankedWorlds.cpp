#include "RankedWorlds.h"
#include "../../Context.h"
#include "../../World/WorldManager.h"

struct RankedWorldEntry
{
    string name;
    uint32 worldID = 0;
    uint32 serverID = 0;
    size_t pendingCount = 0;
};

const TelnetCommandInfo& RankedWorlds::GetInfo()
{
    static TelnetCommandInfo info =
    {
        "/rankedworlds",
        "(Not implemented) Show ranked worlds by popularity",
        2,
        {
            CompileTimeHashString("rankedworlds")
        }
    };

    return info;
}

void RankedWorlds::Execute(TelnetClient* pNetClient, std::vector<string>& args)
{
    if(!pNetClient || !TelnetCommandBase<RankedWorlds>::CheckPerm(pNetClient)) return;

    std::vector<RankedWorldEntry> worlds;
    GetWorldManager()->ForEachWorldSession([&](const WorldSession& session) {
        worlds.push_back(RankedWorldEntry{session.worldName, session.worldID, session.serverID, session.pendingPlayers.size()});
    });

    std::sort(worlds.begin(), worlds.end(), [](const RankedWorldEntry& lhs, const RankedWorldEntry& rhs) {
        if(lhs.pendingCount != rhs.pendingCount) return lhs.pendingCount > rhs.pendingCount;
        return lhs.name < rhs.name;
    });

    pNetClient->SendMessage("Active worlds:", true);
    if(worlds.empty()) {
        pNetClient->SendMessage("No active worlds.", true);
        return;
    }

    size_t index = 0;
    for(const auto& entry : worlds) {
        pNetClient->SendMessage(
            string("#") + ToString(++index) + " " + entry.name + " [worldID=" + ToString(entry.worldID) + ", server=" + ToString(entry.serverID) + ", pending=" + ToString(entry.pendingCount) + "]",
            true
        );
    }
}
