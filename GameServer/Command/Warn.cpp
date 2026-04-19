#include "Warn.h"
#include "Utils/StringUtils.h"
#include "CommandTargetUtils.h"
#include "../Server/GameServer.h"
#include "../Server/MasterBroadway.h"

const CommandInfo& Warn::GetInfo()
{
    static CommandInfo info =
    {
        "/warn <target|#userid> <reason>",
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

    if(args.size() < 2) {
        pPlayer->SendOnConsoleMessage("Usage: " + GetInfo().usage);
        return;
    }

    if(args.size() < 3) {
        pPlayer->SendOnConsoleMessage("Usage: /warn <target|#userid> <reason>");
        return;
    }

    CommandTargetSpec targetSpec;
    string targetError;
    if(!ParseCommandTargetArg(args[1], true, targetSpec, targetError)) {
        pPlayer->SendOnConsoleMessage(targetError);
        return;
    }

    string reason = JoinString(args, " ", 2);
    RemoveExtraWhiteSpaces(reason);
    if(reason.empty()) {
        pPlayer->SendOnConsoleMessage("Usage: /warn <target|#userid> <reason>");
        return;
    }

    if(reason.size() > 120) {
        reason.resize(120);
    }

    auto matches = ResolveLocalTargets(targetSpec, false, 0);
    if(matches.empty()) {
        GetMasterBroadway()->SendCrossServerActionRequest(
            TCP_CROSS_ACTION_WARN,
            pPlayer->GetUserID(),
            pPlayer->GetRawName(),
            targetSpec.query,
            targetSpec.exactMatch,
            reason,
            0);
        return;
    }

    if(!targetSpec.exactMatch && !targetSpec.byUserID && matches.size() > 1) {
        SendAmbiguousLocalTargetList(pPlayer, targetSpec.query, matches, "server");
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
