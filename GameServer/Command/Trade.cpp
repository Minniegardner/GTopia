#include "Utils/StringUtils.h"
#include "Trade.h"
#include "../Server/GameServer.h"

const CommandInfo& Trade::GetInfo()
{
    static CommandInfo info =
    {
        "/trade <player_name>",
        "Start a trade with a player in the same world",
        ROLE_PERM_NONE,
        {
            CompileTimeHashString("trade")
        }
    };

    return info;
}

void Trade::Execute(GamePlayer* pPlayer, std::vector<string>& args)
{
    if(!pPlayer) {
        return;
    }

    if(args.size() < 2) {
        pPlayer->SendOnConsoleMessage("`oUsage: " + GetInfo().usage);
        return;
    }

    if(!CheckPerm(pPlayer)) {
        return;
    }

    if(pPlayer->GetCurrentWorld() == 0) {
        return;
    }

    GamePlayer* pTarget = GetGameServer()->GetPlayerByRawName(args[1]);
    if(!pTarget || pTarget == pPlayer || pTarget->GetCurrentWorld() != pPlayer->GetCurrentWorld()) {
        pPlayer->SendOnConsoleMessage("`4Player not found.``");
        return;
    }

    pPlayer->StartTrade(pTarget);
}