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

    std::vector<GamePlayer*> matches = GetGameServer()->FindPlayersByNamePrefix(args[1], true, pPlayer->GetCurrentWorld());
    if(matches.empty()) {
        pPlayer->SendOnConsoleMessage("`4Player not found in this world.``");
        return;
    }

    GamePlayer* pTarget = nullptr;
    for(GamePlayer* candidate : matches) {
        if(candidate && candidate != pPlayer) {
            pTarget = candidate;
            break;
        }
    }

    if(!pTarget) {
        pPlayer->SendOnConsoleMessage("`4You can't trade with yourself.``");
        return;
    }

    if(matches.size() > 1) {
        pPlayer->SendOnConsoleMessage("`6Multiple players matched. Using the closest name match: `w" + pTarget->GetRawName() + "``");
    }

    pPlayer->StartTrade(pTarget);
}