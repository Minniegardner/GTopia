#include "Kick.h"
#include "Utils/StringUtils.h"
#include "../Server/GameServer.h"

const CommandInfo& Kick::GetInfo()
{
    static CommandInfo info =
    {
        "/kick <player_prefix>",
        "Kick an online player",
        ROLE_PERM_COMMAND_KICK,
        {
            CompileTimeHashString("kick")
        }
    };

    return info;
}

void Kick::Execute(GamePlayer* pPlayer, std::vector<string>& args)
{
    if(!pPlayer || args.empty() || !CheckPerm(pPlayer)) {
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

    auto matches = GetGameServer()->FindPlayersByNamePrefix(query, false, 0);
    if(matches.empty()) {
        pPlayer->SendOnConsoleMessage("`4Oops:`` There is nobody currently in this server with a name starting with `w" + query + "``.");
        return;
    }

    if(matches.size() > 1) {
        pPlayer->SendOnConsoleMessage("`oThere are more than two players in the server starting with `w" + query + " `obe more specific!");
        return;
    }

    GamePlayer* pTarget = matches[0];
    if(pTarget == pPlayer) {
        pPlayer->SendOnConsoleMessage("Nope, you can't use that on yourself.");
        return;
    }

    pTarget->SendOnConsoleMessage("`4You were kicked by ``" + pPlayer->GetDisplayName() + "``.");
    pPlayer->SendOnConsoleMessage("`oKicked ``" + pTarget->GetDisplayName() + "``.");
    pTarget->LogOff();
}
