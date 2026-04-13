#include "Ban.h"
#include "Utils/StringUtils.h"
#include "../Server/GameServer.h"

const CommandInfo& Ban::GetInfo()
{
    static CommandInfo info =
    {
        "/ban <player_prefix>",
        "Ban (disconnect) a player",
        ROLE_PERM_COMMAND_BAN,
        {
            CompileTimeHashString("ban")
        }
    };

    return info;
}

void Ban::Execute(GamePlayer* pPlayer, std::vector<string>& args)
{
    if(!pPlayer || args.empty() || !CheckPerm(pPlayer)) {
        return;
    }

    if(pPlayer->GetCurrentWorld() == 0) {
        return;
    }

    if(args.size() < 2) {
        pPlayer->SendOnConsoleMessage("`oUsage: " + GetInfo().usage);
        return;
    }

    const string query = args[1];
    if(query.size() < 3) {
        pPlayer->SendOnConsoleMessage("You'll need to enter at least the first three characters of the person's name.");
        return;
    }

    auto matches = GetGameServer()->FindPlayersByNamePrefix(query, true, pPlayer->GetCurrentWorld());
    if(matches.empty()) {
        pPlayer->SendOnConsoleMessage("`4Oops:`` There is nobody currently in this server with a name starting with `w" + query + "``.");
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
        pPlayer->SendOnConsoleMessage("`4Oops:`` There is nobody currently in this server with a name starting with `w" + query + "``.");
        return;
    }

    if(filtered.size() > 1) {
        pPlayer->SendOnConsoleMessage("`oThere are more than two players in the server starting with `w" + query + " `obe more specific!");
        return;
    }

    GamePlayer* pTarget = filtered[0];
    
    // Get the current world and ban the player from it with 1 hour duration
    World* pWorld = GetGameServer()->GetWorld(pPlayer->GetCurrentWorld());
    if(pWorld) {
        pWorld->BanPlayer(pTarget, pPlayer, 1);  // Ban for 1 hour
        pPlayer->SendOnConsoleMessage("`oBanned ``" + pTarget->GetDisplayName() + "`` from this world for 1 hour.");
    }
    else {
        pPlayer->SendOnConsoleMessage("`4Error: Could not find world for ban.");
    }
}
