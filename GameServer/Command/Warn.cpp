#include "Warn.h"
#include "Utils/StringUtils.h"
#include "../Server/GameServer.h"

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

    auto matches = GetGameServer()->FindPlayersByNamePrefix(query, false, 0);
    if(matches.empty()) {
        pPlayer->SendOnConsoleMessage("`4Oops:`` There is nobody currently in this server with a name starting with `w" + query + "``.");
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

    pTarget->SendOnTextOverlay("`4WARNING:`` " + reason);
    pTarget->SendOnConsoleMessage("`4Warning from ``" + pPlayer->GetDisplayName() + "``: " + reason);
    pPlayer->SendOnConsoleMessage("`oWarned ``" + pTarget->GetDisplayName() + "``: " + reason);
}
