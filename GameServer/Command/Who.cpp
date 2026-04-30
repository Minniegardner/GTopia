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

    auto players = GetPlayerManager()->FindPlayersByNamePrefix("", true, pPlayer->GetCurrentWorld());
    if(players.empty()) {
        pPlayer->SendOnConsoleMessage("`oNo players found in this world.``");
        return;
    }

    pPlayer->SendOnConsoleMessage("`oPlayers here (`w" + ToString((int)players.size()) + "``):");
    for(GamePlayer* pTarget : players) {
        if(!pTarget) {
            continue;
        }

        pPlayer->SendOnConsoleMessage("`w" + pTarget->GetDisplayName() + "`` (`o" + pTarget->GetRawName() + "``)");
    }
}
