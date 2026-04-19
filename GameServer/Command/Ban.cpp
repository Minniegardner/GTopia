#include "Ban.h"
#include "Utils/StringUtils.h"
#include "../Server/GameServer.h"
#include "../World/WorldManager.h"

const CommandInfo& Ban::GetInfo()
{
    static CommandInfo info =
    {
        "/ban <player_prefix> [hours]",
        "Ban a player from your current world",
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
        pPlayer->SendOnConsoleMessage("Usage: " + GetInfo().usage);
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
        pPlayer->SendOnConsoleMessage("`oThere are more than two players in the server starting with `w" + query + " `obe more specific!");
        return;
    }

    GamePlayer* pTarget = filtered[0];
    if(pTarget == pPlayer) {
        pPlayer->SendOnConsoleMessage("Nope, you can't use that on yourself.");
        return;
    }

    uint32 banHours = 1;
    if(args.size() >= 3) {
        int32 parsedHours = 0;
        if(ToInt(args[2], parsedHours) == TO_INT_SUCCESS && parsedHours > 0) {
            if(parsedHours > 720) {
                parsedHours = 720;
            }
            banHours = (uint32)parsedHours;
        }
    }

    World* pWorld = GetWorldManager()->GetWorldByID(pPlayer->GetCurrentWorld());
    if(!pWorld) {
        pPlayer->SendOnConsoleMessage("`4Error: Could not find world for ban.");
        return;
    }

    if(!pWorld->CanBan(pTarget, pPlayer)) {
        pPlayer->SendOnConsoleMessage("`4Oops:`` You don't have permission to ban `w" + pTarget->GetRawName() + "!``");
        return;
    }

    pWorld->BanPlayer(pTarget, pPlayer, banHours);
    pWorld->ForceLeavePlayer(pTarget);
}
