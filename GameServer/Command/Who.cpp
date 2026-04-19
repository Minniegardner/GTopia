#include "Who.h"
#include "../Server/GameServer.h"
#include "Utils/StringUtils.h"

const CommandInfo& Who::GetInfo()
{
    static CommandInfo info =
    {
        "/who",
        "List players in your current world",
        ROLE_PERM_NONE,
        {
            CompileTimeHashString("who")
        }
    };

    return info;
}

void Who::Execute(GamePlayer* pPlayer, std::vector<string>& args)
{
    if(!pPlayer || args.empty() || !CheckPerm(pPlayer)) {
        return;
    }

    if(pPlayer->GetCurrentWorld() == 0) {
        return;
    }

    auto players = GetGameServer()->FindPlayersByNamePrefix("", true, pPlayer->GetCurrentWorld());
    if(players.empty()) {
        pPlayer->SendOnConsoleMessage("`oNo players found in this world.``");
        return;
    }

    World* pWorld = GetWorldManager()->GetWorldByID(pPlayer->GetCurrentWorld());
    const string worldName = pWorld ? pWorld->GetWorlName() : "UNKNOWN";
    pPlayer->SendOnConsoleMessage("`oWho's in `o" + worldName + "``: `w" + ToString((int)players.size()) + "``");
    for(GamePlayer* pTarget : players) {
        if(!pTarget) {
            continue;
        }

        pPlayer->SendOnConsoleMessage(
            "`w" + pTarget->GetRawName() + "`` (UID: `o" + ToString(pTarget->GetUserID()) +
            "`` | NID: `o" + ToString(pTarget->GetNetID()) + "``)"
        );
    }
}
