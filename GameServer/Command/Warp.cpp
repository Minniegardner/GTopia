#include "Warp.h"
#include "../World/WorldManager.h"
#include "Utils/StringUtils.h"

const CommandInfo& Warp::GetInfo()
{
    static CommandInfo info =
    {
        "/warp <world_name>",
        "Warp to another world",
        ROLE_PERM_NONE,
        {
            CompileTimeHashString("warp")
        }
    };

    return info;
}

void Warp::Execute(GamePlayer* pPlayer, std::vector<string>& args)
{
    if(!pPlayer || args.empty() || !CheckPerm(pPlayer)) {
        return;
    }

    if(args.size() < 2) {
        pPlayer->SendOnConsoleMessage("You must specify a world name");
        return;
    }

    string worldName = ToUpper(args[1]);
    if(worldName.size() < 1 || worldName.size() > 24) {
        pPlayer->SendOnConsoleMessage("`4Invalid world name.``");
        return;
    }

    pPlayer->SendOnConsoleMessage("`oWarping to `w" + worldName + "``...");
    GetWorldManager()->PlayerJoinRequest(pPlayer, worldName);
}
