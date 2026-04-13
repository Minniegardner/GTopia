#include "Warn.h"
#include "Utils/StringUtils.h"
#include "../Server/GameServer.h"
#include "../Server/MasterBroadway.h"

const CommandInfo& Warn::GetInfo()
{
    static CommandInfo info =
    {
        "/warn <player_prefix> <reason>",
        "Warn a player with a reason",
        ROLE_PERM_COMMAND_WARN,
        {
            CompileTimeHashString("warn")
        }
    };

    return info;
}

void Warn::Execute(GamePlayer* pPlayer, std::vector<string>& args)
{
    if(!pPlayer || args.empty() || !CheckPerm(pPlayer)) {
        return;
    }

    if(args.size() < 2 || args[1].size() < 3) {
        pPlayer->SendOnConsoleMessage("`6>> `4Oops: `6Enter at least the `#first three characters `6of the persons name.``");
        return;
    }

    if(args.size() < 3) {
        pPlayer->SendOnConsoleMessage("Command incomplete. Here's the argument list:");
        pPlayer->SendOnConsoleMessage("-> /warn <name> <reason>");
        return;
    }

    string query = args[1];
    bool exactMatch = false;
    if(!query.empty() && query[0] == '/') {
        exactMatch = true;
        query.erase(query.begin());
    }

    string reason = JoinString(args, " ", 2);
    RemoveExtraWhiteSpaces(reason);
    if(reason.empty()) {
        pPlayer->SendOnConsoleMessage("Command incomplete. Here's the argument list:");
        pPlayer->SendOnConsoleMessage("-> /warn <name> <reason>");
        return;
    }

    if(reason.size() > 120) {
        reason.resize(120);
    }

    auto matches = GetGameServer()->FindPlayersByNamePrefix(query, false, 0);
    if(matches.empty()) {
        GetMasterBroadway()->SendCrossServerActionRequest(
            TCP_CROSS_ACTION_WARN,
            pPlayer->GetUserID(),
            pPlayer->GetRawName(),
            query,
            exactMatch,
            reason,
            0);
        return;
    }

    if(!exactMatch && matches.size() > 1) {
        pPlayer->SendOnConsoleMessage("`oThere are more than two players in the server starting with `w" + query + " `obe more specific!");
        return;
    }

    GamePlayer* pTarget = matches[0];
    if(!pTarget || pTarget == pPlayer) {
        pPlayer->SendOnConsoleMessage("Nope, you can't use that on yourself.");
        return;
    }

    pTarget->SendOnTextOverlay("`4WARNING:`` " + reason);
    pTarget->SendOnConsoleMessage("`4Warning from ``" + pPlayer->GetDisplayName() + "``: " + reason);
    pPlayer->SendOnConsoleMessage("`oWarned ``" + pTarget->GetDisplayName() + "``: " + reason);
}
