#include "ListRoles.h"
#include "../../Source/Player/RoleManager.h"
#include "Utils/StringUtils.h"

const CommandInfo& ListRoles::GetInfo()
{
    static CommandInfo info =
    {
        "/listroles",
        "List available role codes",
        ROLE_PERM_COMMAND_ROLEMGMT,
        {
            CompileTimeHashString("listroles")
        }
    };

    return info;
}

void ListRoles::Execute(GamePlayer* pPlayer, std::vector<string>& args)
{
    if(!pPlayer || args.empty() || !CheckPerm(pPlayer)) {
        return;
    }

    string roles;
    uint32 roleCount = 0;
    for(const auto& [roleID, pRole] : GetRoleManager()->GetRoles()) {
        if(!pRole) {
            continue;
        }

        ++roleCount;
        const string entry = ToString(roleID) + ":" + pRole->GetName();
        if(roles.empty()) {
            roles = entry;
        }
        else {
            roles += ", " + entry;
        }
    }

    pPlayer->SendOnConsoleMessage("List of Available Role Codes (" + ToString(roleCount) + "): " + roles);
}