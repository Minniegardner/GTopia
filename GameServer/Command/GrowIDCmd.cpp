#include "GrowIDCmd.h"

#include "../Player/Dialog/RegisterDialog.h"

const CommandInfo& GrowIDCmd::GetInfo()
{
    static CommandInfo info =
    {
        "/growid",
        "Open GrowID registration dialog",
        ROLE_PERM_NONE,
        {
            CompileTimeHashString("growid")
        }
    };

    return info;
}

void GrowIDCmd::Execute(GamePlayer* pPlayer, std::vector<string>& args)
{
    if(!pPlayer || args.empty() || !CheckPerm(pPlayer)) {
        return;
    }

    if(pPlayer->HasGrowID()) {
        pPlayer->SendOnConsoleMessage("`oYou already have a GrowID.``");
        return;
    }

    if(pPlayer->HasState(PLAYER_STATE_CREATING_GROWID)) {
        pPlayer->SendOnConsoleMessage("`oPlease wait, your GrowID request is being processed.``");
        return;
    }

    RegisterDialog::Request(pPlayer);
}
