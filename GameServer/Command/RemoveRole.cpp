#include "RemoveRole.h"
#include "Utils/StringUtils.h"
#include "CommandTargetUtils.h"
#include "../Server/GameServer.h"
#include "../World/WorldManager.h"
#include "../../Source/Player/RoleManager.h"

namespace {

}

const CommandInfo& RemoveRole::GetInfo()
{
    static CommandInfo info =
    {
        "/removerole <target|#userid> [role id|role name]",
        "Remove role from player (fallback to default role)",
        ROLE_PERM_COMMAND_ROLEMGMT,
        {
            CompileTimeHashString("removerole")
        }
    };

    return info;
}

void RemoveRole::Execute(GamePlayer* pPlayer, std::vector<string>& args)
{
    if(!pPlayer || args.empty() || !CheckPerm(pPlayer)) {
        return;
    }

    if(args.size() < 2) {
        pPlayer->SendOnConsoleMessage("You must specify the name of the player to add roles on");
        return;
    }

    CommandTargetSpec targetSpec;
    string targetError;
    if(!ParseCommandTargetArg(args[1], true, targetSpec, targetError)) {
        pPlayer->SendOnConsoleMessage(targetError);
        return;
    }

    auto matches = ResolveLocalTargets(targetSpec, false, 0);
    if(!targetSpec.exactMatch && !targetSpec.byUserID && matches.size() > 1) {
        SendAmbiguousLocalTargetList(pPlayer, targetSpec.query, matches, "server");
        return;
    }

    GamePlayer* pTarget = matches.empty() ? nullptr : matches[0];
    if(!pTarget) {
        pPlayer->SendOnConsoleMessage("Player '" + targetSpec.query + "' not found or offline");
        return;
    }

    Role* pCurrentRole = pTarget->GetRole();
    Role* pRequested = nullptr;
    if(args.size() >= 3) {
        const string roleArg = JoinString(args, " ", 2);
        int32 roleID = 0;
        if(ToInt(roleArg, roleID) == TO_INT_SUCCESS) {
            pRequested = GetRoleManager()->GetRole(roleID);
        }
        else {
            pRequested = GetRoleManager()->FindRoleByName(roleArg);
        }

        if(!pRequested) {
            pPlayer->SendOnConsoleMessage("Invalid Role: " + roleArg);
            return;
        }

        if(!pCurrentRole || pCurrentRole->GetID() != pRequested->GetID()) {
            pPlayer->SendOnConsoleMessage("Player " + pTarget->GetRawName() + " has no role '" + pRequested->GetName() + "'");
            return;
        }
    }

    Role* pDefault = GetRoleManager()->GetRole(GetRoleManager()->GetDefaultRoleID());
    if(!pDefault || !pTarget->SetRoleByID((int32)pDefault->GetID())) {
        pPlayer->SendOnConsoleMessage("`4Failed to apply default role.``");
        return;
    }

    World* pWorld = GetWorldManager()->GetWorldByID(pTarget->GetCurrentWorld());
    if(pWorld) {
        pWorld->SendNameChangeToAll(pTarget);
        pWorld->SendSetCharPacketToAll(pTarget);
    }

    pTarget->SaveToDatabase();
    pPlayer->SendOnConsoleMessage("Successfuly took role from " + pTarget->GetRawName());
}