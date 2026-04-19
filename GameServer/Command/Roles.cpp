#include "Roles.h"
#include "CommandTargetUtils.h"
#include "CommandTargetUtils.h"
#include "Utils/StringUtils.h"
#include "../Server/GameServer.h"

const CommandInfo& Roles::GetInfo()
{
    static CommandInfo info =
    {
        "/roles [target|#userid]",
        "Show your role or a target player's role",
        ROLE_PERM_COMMAND_ROLEMGMT,
        {
            CompileTimeHashString("roles")
        }
    };

    return info;
}

void Roles::Execute(GamePlayer* pPlayer, std::vector<string>& args)
{
    if(!pPlayer || args.empty() || !CheckPerm(pPlayer)) {
        return;
    }

    if(args.size() == 1) {
        Role* pRole = pPlayer->GetRole();
        if(!pRole) {
            pPlayer->SendOnConsoleMessage("You have no roles");
            return;
        }

        pPlayer->SendOnConsoleMessage("Your Roles: " + pRole->GetName());
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
        pPlayer->SendOnConsoleMessage(pTarget ? (pTarget->GetRawName() + " has no role") : "Player has no role");
        return;
    }

    pPlayer->SendOnConsoleMessage(pTarget->GetRawName() + "'s Roles: " + pTarget->GetRole()->GetName());
}