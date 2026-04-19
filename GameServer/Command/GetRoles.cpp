#include "CommandTargetUtils.h"
#include "GetRoles.h"
#include "../Server/GameServer.h"

const CommandInfo& GetRoles::GetInfo()
{
    static CommandInfo info =
    {
        "/getroles <target|#userid>",
        "Get role list from a player",
        ROLE_PERM_COMMAND_ROLEMGMT,
        {
            CompileTimeHashString("getroles")
        }
    };

    return info;
}

void GetRoles::Execute(GamePlayer* pPlayer, std::vector<string>& args)
{
    if(!pPlayer || args.empty() || !CheckPerm(pPlayer)) {
        return;
    }

    if(args.size() < 2) {
        pPlayer->SendOnConsoleMessage("Usage: " + GetInfo().usage);
        return;
    }

    CommandTargetSpec targetSpec;
    string targetError;
    if(!ParseCommandTargetArg(args[1], true, targetSpec, targetError)) {
        pPlayer->SendOnConsoleMessage(targetError);
        return;
    }

    auto matches = ResolveLocalTargets(targetSpec, false, 0);
    if(matches.empty()) {
        pPlayer->SendOnConsoleMessage("Player '" + targetSpec.query + "' not found or offline");
        return;
    }

    if(!targetSpec.exactMatch && !targetSpec.byUserID && matches.size() > 1) {
        SendAmbiguousLocalTargetList(pPlayer, targetSpec.query, matches, "server");
        return;
    }

    GamePlayer* pTarget = matches[0];
    if(!pTarget || !pTarget->GetRole()) {
        pPlayer->SendOnConsoleMessage("Roles of " + args[1] + " (0):");
        return;
    }

    pPlayer->SendOnConsoleMessage("Roles of " + pTarget->GetRawName() + " (1): " + pTarget->GetRole()->GetName());
}