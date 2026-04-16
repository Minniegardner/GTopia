#include "Online.h"
#include "../Server/GameServer.h"

const CommandInfo& Online::GetInfo()
{
    static CommandInfo info =
    {
        "/online",
        "Show online player count",
        ROLE_PERM_NONE,
        {
            CompileTimeHashString("online")
        }
    };

    return info;
}

void Online::Execute(GamePlayer* pPlayer, std::vector<string>& args)
{
    if(!pPlayer || args.empty() || !CheckPerm(pPlayer)) {
        return;
    }

    pPlayer->SendOnConsoleMessage("`oThere are currently `w" + ToString((int)GetGameServer()->GetOnlineCount()) + "`` players online.");
}
