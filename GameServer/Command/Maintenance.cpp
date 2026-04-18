#include "Maintenance.h"
#include "Utils/StringUtils.h"
#include "../Server/GameServer.h"

const CommandInfo& Maintenance::GetInfo()
{
    static CommandInfo info =
    {
        "/maintenance [message]",
        "Start a server maintenance countdown across all subservers",
        ROLE_PERM_COMMAND_MAINTENANCE,
        {
            CompileTimeHashString("maintenance"),
            CompileTimeHashString("restart")
        }
    };

    return info;
}

void Maintenance::Execute(GamePlayer* pPlayer, std::vector<string>& args)
{
    if(!pPlayer || args.empty() || !CheckPerm(pPlayer)) {
        return;
    }

    string message;
    if(args.size() > 1) {
        message = JoinString(args, " ", 1);
        RemoveExtraWhiteSpaces(message);
    }

    if(message.empty()) {
        message = "Restarting server for update";
    }

    if(!GetGameServer()->InitiateMaintenance(message, pPlayer->GetUserID(), pPlayer->GetRawName())) {
        pPlayer->SendOnConsoleMessage("`4Maintenance is already running.``");
        return;
    }

    pPlayer->SendOnConsoleMessage("`oMaintenance countdown started for all subservers.``");
}