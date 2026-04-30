#include "Uba.h"
#include "Utils/StringUtils.h"
#include "../Server/GameServer.h"
#include "../World/WorldManager.h"

const CommandInfo& Uba::GetInfo()
{
    static CommandInfo info =
    {
        "/uba <player_prefix>",
        "Unban a player from your current world",
        ROLE_PERM_COMMAND_BAN,
        {
            CompileTimeHashString("uba"),
            CompileTimeHashString("unban")
        }
    };

    return info;
}

void Uba::Execute(GamePlayer* pPlayer, std::vector<string>& args)
{
    if(!pPlayer || args.empty() || !CheckPerm(pPlayer)) {
        return;
    }

    if(pPlayer->GetCurrentWorld() == 0) {
        return;
    }

    if(args.size() < 2 || args[1].size() < 3) {
        pPlayer->SendOnConsoleMessage("`oUsage: " + GetInfo().usage);
        return;
    }

    string query = args[1];
    bool exactMatch = false;
    if(!query.empty() && query[0] == '/') {
        exactMatch = true;
        query.erase(query.begin());
    }

    auto matches = GetPlayerManager()->FindPlayersByNamePrefix(query, true, pPlayer->GetCurrentWorld());
    if(matches.empty()) {
        pPlayer->SendOnConsoleMessage("`4Oops:`` There is nobody currently in this world with a name starting with `w" + query + "``.");
        return;
    }

    std::vector<GamePlayer*> filtered;
    filtered.reserve(matches.size());
    for(GamePlayer* pMatch : matches) {
        if(pMatch && pMatch != pPlayer) {
            filtered.push_back(pMatch);
        }
    }

    if(filtered.empty()) {
        pPlayer->SendOnConsoleMessage("`4Oops:`` There is nobody currently in this world with a name starting with `w" + query + "``.");
        return;
    }

    if(!exactMatch && filtered.size() > 1) {
        pPlayer->SendOnConsoleMessage("`oThere are multiple players starting with `w" + query + "`o, be more specific.");
        return;
    }

    GamePlayer* pTarget = filtered[0];
    World* pWorld = GetWorldManager()->GetWorldByID(pPlayer->GetCurrentWorld());
    if(!pWorld) {
        pPlayer->SendOnConsoleMessage("`4Error: Could not find world for unban.");
        return;
    }

    pWorld->RemoveBannedPlayer(pTarget->GetUserID());
    pPlayer->SendOnConsoleMessage("`oRemoved world ban for ``" + pTarget->GetDisplayName() + "``.");
}
