#include "Pull.h"
#include "Utils/StringUtils.h"
#include "../Server/GameServer.h"
#include "../World/WorldManager.h"

const CommandInfo& Pull::GetInfo()
{
    static CommandInfo info =
    {
        "/pull <player_prefix>",
        "Pull a player to your position",
        ROLE_PERM_COMMAND_PULL,
        {
            CompileTimeHashString("pull")
        }
    };

    return info;
}

void Pull::Execute(GamePlayer* pPlayer, std::vector<string>& args)
{
    if(!pPlayer || args.empty() || !CheckPerm(pPlayer)) {
        return;
    }

    if(args.size() < 2) {
        pPlayer->SendOnConsoleMessage("Usage: " + GetInfo().usage);
        return;
    }

    if(pPlayer->GetCurrentWorld() == 0) {
        return;
    }

    const string query = args[1];
    if(query.size() < 3) {
        pPlayer->SendOnConsoleMessage("`6>> `4Oops: `6Enter at least the `#first three characters `6of the persons name.``");
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
        pPlayer->SendOnConsoleMessage("`oThere are multiple players in the server starting with `w" + query + " `obe more specific.");
        return;
    }

    GamePlayer* pTarget = filtered[0];
    if(pTarget == pPlayer) {
        pPlayer->SendOnConsoleMessage("Nope, you can't use that on yourself.");
        return;
    }

    World* pWorld = GetWorldManager()->GetWorldByID(pPlayer->GetCurrentWorld());
    if(!pWorld) {
        return;
    }

    if(!pWorld->CanPull(pTarget, pPlayer)) {
        pPlayer->SendOnConsoleMessage("`4Oops:`` You don't have permission to pull `w" + pTarget->GetRawName() + "!``");
        return;
    }

    pWorld->PullPlayer(pTarget, pPlayer);
}
