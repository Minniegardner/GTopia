#include "Kick.h"
#include "Utils/StringUtils.h"
#include "../Server/GameServer.h"
#include "../Server/MasterBroadway.h"

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
        GetMasterBroadway()->SendCrossServerActionRequest(
            TCP_CROSS_ACTION_KICK,
            pPlayer->GetUserID(),
            pPlayer->GetRawName(),
            query,
            false,
            "",
            0);
        return;
    }

    if(matches.size() > 1) {
        pPlayer->SendOnConsoleMessage("`oThere are more than two players starting with `w" + query + "`o, be more specific.");
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
