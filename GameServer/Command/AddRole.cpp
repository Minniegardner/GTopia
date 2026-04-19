#include "AddRole.h"
#include "Utils/StringUtils.h"
#include "CommandTargetUtils.h"
#include "../Server/GameServer.h"
#include "../World/WorldManager.h"
#include "../../Source/Player/RoleManager.h"

namespace {

string BuildRoleList()
{
    string roles;
    for(const auto& [roleID, pRole] : GetRoleManager()->GetRoles()) {
        if(!pRole) {
            continue;
        }

        const string entry = ToString(roleID) + ":" + pRole->GetName();
        if(roles.empty()) {
            roles = entry;
        }
        else {
            roles += ", " + entry;
        }
    }

    return roles;
}

Role* ResolveRoleArgument(const string& roleArg)
{
    int32 roleID = 0;
    if(ToInt(roleArg, roleID) == TO_INT_SUCCESS) {
        return GetRoleManager()->GetRole(roleID);
    }

    return GetRoleManager()->FindRoleByName(roleArg);
}

}

const CommandInfo& AddRole::GetInfo()
{
    static CommandInfo info =
    {
        "/addrole <target|#userid> <role id|role name>",
        "Give a player a role",
        ROLE_PERM_COMMAND_ROLEMGMT,
        {
            CompileTimeHashString("addrole")
        }
    };

    return info;
}

void AddRole::Execute(GamePlayer* pPlayer, std::vector<string>& args)
{
    if(!pPlayer || args.empty() || !CheckPerm(pPlayer)) {
        return;
    }

    if(args.size() < 3) {
        pPlayer->SendOnConsoleMessage("Usage: " + GetInfo().usage);
        pPlayer->SendOnConsoleMessage("`oAvailable roles: `w" + BuildRoleList() + "``");
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
        pPlayer->SendOnConsoleMessage("`4Oops:`` There is nobody currently in this server with a name starting with `w" + targetSpec.query + "``.");
        return;
    }

    const string roleArg = JoinString(args, " ", 2);
    Role* pRole = ResolveRoleArgument(roleArg);
    if(!pRole) {
        pPlayer->SendOnConsoleMessage("`4Role not found: `w" + roleArg + "``");
        pPlayer->SendOnConsoleMessage("`oAvailable roles: `w" + BuildRoleList() + "``");
        return;
    }

    if(!pTarget->SetRoleByID((int32)pRole->GetID())) {
        pPlayer->SendOnConsoleMessage("`4Failed to apply role.``");
        return;
    }

    World* pWorld = GetWorldManager()->GetWorldByID(pTarget->GetCurrentWorld());
    if(pWorld) {
        pWorld->SendNameChangeToAll(pTarget);
        pWorld->SendSetCharPacketToAll(pTarget);
    }

    pTarget->SaveToDatabase();

    pPlayer->SendOnConsoleMessage("`oGiven role `w" + pRole->GetName() + "`` to ``" + pTarget->GetRawName() + "``.");
    pTarget->SendOnConsoleMessage("`oYou were given role `w" + pRole->GetName() + "`` by ``" + pPlayer->GetDisplayName() + "``.");
}