#include "PInfo.h"
#include "Utils/StringUtils.h"
#include "../Server/GameServer.h"

const CommandInfo& PInfo::GetInfo()
{
    static CommandInfo info =
    {
        "/pinfo <name>",
        "Lookup online player info",
        ROLE_PERM_COMMAND_MOD,
        {
            CompileTimeHashString("pinfo")
        }
    };

    return info;
}

void PInfo::Execute(GamePlayer* pPlayer, std::vector<string>& args)
{
    if(!pPlayer || args.empty() || !CheckPerm(pPlayer)) {
        return;
    }

    World* pWorld = GetWorldManager()->GetWorldByID(pPlayer->GetCurrentWorld());
    if(!pWorld) {
        return;
    }

    if(args.size() != 2) {
        pPlayer->SendOnConsoleMessage("Command incomplete. Here's the argument list:");
        pPlayer->SendOnConsoleMessage("-> /pinfo <name>");
        return;
    }

    if(args[1].size() < 3) {
        pPlayer->SendOnConsoleMessage("`6>> `4Oops:`` Enter at least the `#first 3 characters`` of the persons name.``");
        return;
    }

    pPlayer->SendOnConsoleMessage("Searching for `w" + args[1] + "``...");

    auto players = GetPlayerManager()->FindPlayersByNamePrefix(args[1], false, 0);
    if(players.empty()) {
        pPlayer->SendOnConsoleMessage("`4Oops: ``No Players Found.");
        return;
    }

    pPlayer->SendOnConsoleMessage("Found `w" + ToString(players.size()) + "`` Players:");
    for(GamePlayer* pTarget : players) {
        if(!pTarget) {
            continue;
        }

        const string worldName = pTarget->GetCurrentWorld() ? (GetWorldManager()->GetWorldByID(pTarget->GetCurrentWorld()) ? GetWorldManager()->GetWorldByID(pTarget->GetCurrentWorld())->GetWorlName() : "UNKNOWN") : "EXIT";
        pPlayer->SendOnConsoleMessage(
            "`w" + pTarget->GetRawName() + "`` (ID: `o" + ToString(pTarget->GetUserID()) +
            "``) - LV `o" + ToString(pTarget->GetLevel()) +
            "`` - Gems `o" + ToString(pTarget->GetGems()) +
            "`` - World `o" + worldName + "``"
        );
    }
}
