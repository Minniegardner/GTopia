#include "KickAll.h"
#include "Utils/StringUtils.h"
#include "../Server/GameServer.h"

const CommandInfo& KickAll::GetInfo()
{
    static CommandInfo info =
    {
        "/kickall",
        "Kick all players in your current world",
        ROLE_PERM_COMMAND_KICK,
        {
            CompileTimeHashString("kickall")
        }
    };

    return info;
}

void KickAll::Execute(GamePlayer* pPlayer, std::vector<string>& args)
{
    if(!pPlayer || args.empty() || !CheckPerm(pPlayer)) {
        return;
    }

    if(pPlayer->GetCurrentWorld() == 0) {
        return;
    }

    auto players = GetPlayerManager()->FindPlayersByNamePrefix("", true, pPlayer->GetCurrentWorld());
    uint32 kicked = 0;

    for(GamePlayer* pTarget : players) {
        if(!pTarget || pTarget == pPlayer) {
            continue;
        }

        pTarget->SendOnConsoleMessage("`4You were kicked by ``" + pPlayer->GetDisplayName() + "``.");
        pTarget->LogOff();
        ++kicked;
    }

    pPlayer->SendOnConsoleMessage("`oKicked `w" + ToString(kicked) + "`` player(s) from this world.");
}
