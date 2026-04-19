#include "ServerCmd.h"
#include "Utils/StringUtils.h"
#include "../Server/MasterBroadway.h"

const CommandInfo& ServerCmd::GetInfo()
{
    static CommandInfo info =
    {
        "/server <server_id>",
        "Redirect yourself to a specific subserver",
        ROLE_PERM_COMMAND_WARPTO,
        {
            CompileTimeHashString("server")
        }
    };

    return info;
}

void ServerCmd::Execute(GamePlayer* pPlayer, std::vector<string>& args)
{
    if(!pPlayer || args.empty() || !CheckPerm(pPlayer)) {
        return;
    }

    if(args.size() != 2) {
        pPlayer->SendOnConsoleMessage("Server requires 1 argument (Server ID)");
        return;
    }

    uint32 serverID = 0;
    if(ToUInt(args[1], serverID) != TO_INT_SUCCESS || serverID == 0) {
        pPlayer->SendOnConsoleMessage("Server requires 1 argument (Server ID)");
        return;
    }

    GetMasterBroadway()->SendCrossServerActionRequest(
        TCP_CROSS_ACTION_SERVER_SWITCH,
        pPlayer->GetUserID(),
        pPlayer->GetRawName(),
        pPlayer->GetRawName(),
        true,
        ToString(serverID),
        0
    );

    pPlayer->SendOnConsoleMessage("`oChecking destination server context...``");
}