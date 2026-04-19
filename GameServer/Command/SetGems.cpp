#include "SetGems.h"
#include "Utils/StringUtils.h"
#include "../Server/GameServer.h"

const CommandInfo& SetGems::GetInfo()
{
    static CommandInfo info =
    {
        "/setgems <name> <new_gems>",
        "Set player's gems",
        ROLE_PERM_COMMAND_GIVEITEM,
        {
            CompileTimeHashString("setgems")
        }
    };

    return info;
}

void SetGems::Execute(GamePlayer* pPlayer, std::vector<string>& args)
{
    if(!pPlayer || args.empty() || !CheckPerm(pPlayer)) {
        return;
    }

    World* pWorld = GetWorldManager()->GetWorldByID(pPlayer->GetCurrentWorld());
    if(!pWorld) {
        return;
    }

    if(args.size() != 3) {
        pPlayer->SendOnConsoleMessage("Command incomplete. Here's the argument list:");
        pPlayer->SendOnConsoleMessage("-> /setgems <name> <new gems>");
        return;
    }

    int32 gems = 0;
    if(ToInt(args[2], gems) != TO_INT_SUCCESS) {
        pPlayer->SendOnConsoleMessage("`4New gems must be a number.");
        return;
    }

    string query = ToLower(args[1]);
    auto players = GetGameServer()->FindPlayersByNamePrefix(query, false, 0);
    if(players.empty()) {
        pPlayer->SendOnConsoleMessage("Player '" + args[1] + "' not found or offline");
        return;
    }

    if(players.size() > 1) {
        pPlayer->SendOnConsoleMessage("`oThere are multiple players starting with `w" + query + "`o, be more specific.");
        return;
    }

    GamePlayer* pTarget = players[0];
    if(!pTarget) {
        pPlayer->SendOnConsoleMessage("Player '" + args[1] + "' not found or offline");
        return;
    }

    pTarget->SetGems(gems);
    pTarget->SendOnSetBux();

    pPlayer->SendOnConsoleMessage("`oSet `$" + pTarget->GetRawName() + "`o's Gems to `$" + ToString(pTarget->GetGems()) + "``");
}
