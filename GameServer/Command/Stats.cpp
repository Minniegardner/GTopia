#include "Stats.h"
#include "../Server/GameServer.h"
#include "../World/WorldManager.h"
#include "Utils/StringUtils.h"

const CommandInfo& Stats::GetInfo()
{
    static CommandInfo info =
    {
        "/stats",
        "Show server statistics",
        ROLE_PERM_NONE,
        {
            CompileTimeHashString("stats")
        }
    };

    return info;
}

void Stats::Execute(GamePlayer* pPlayer, std::vector<string>& args)
{
    if(!pPlayer || args.empty() || !CheckPerm(pPlayer)) {
        return;
    }

    pPlayer->SendOnConsoleMessage("`5Server Statistics``");
    pPlayer->SendOnConsoleMessage("`oOnline Players: `w" + ToString((int)GetGameServer()->GetOnlineCount()) + "``");
    pPlayer->SendOnConsoleMessage("`oLoaded Worlds: `w" + ToString((int)GetWorldManager()->GetWorldCount()) + "``");
}
