#include "Ping.h"

const CommandInfo& Ping::GetInfo()
{
    static CommandInfo info =
    {
        "/ping",
        "Check connection status",
        ROLE_PERM_NONE,
        {
            CompileTimeHashString("ping")
        }
    };

    return info;
}

void Ping::Execute(GamePlayer* pPlayer, std::vector<string>& args)
{
    if(!pPlayer || args.empty() || !CheckPerm(pPlayer)) {
        return;
    }

    pPlayer->SendFakePingReply();
    pPlayer->SendOnConsoleMessage("`oPong!``");
}
