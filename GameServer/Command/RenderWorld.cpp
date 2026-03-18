#include "Utils/StringUtils.h"
#include "RenderWorld.h"
#include "../Server/MasterBroadway.h"

const CommandInfo& RenderWorld::GetInfo()
{
    static CommandInfo info =
    {
        "/renderworld",
        "View world as image",
        ROLE_PERM_NONE,
        {
            CompileTimeHashString("renderworld")
        }
    };

    return info;
}

void RenderWorld::Execute(GamePlayer* pPlayer, std::vector<string>& args)
{
    if(!pPlayer || args.empty() || !CheckPerm(pPlayer)) {
        return;
    }

    if(pPlayer->GetCurrentWorld() == 0) {
        return;
    }

    if(pPlayer->HasState(PLAYER_STATE_RENDERING_WORLD)) {
        pPlayer->SendOnConsoleMessage("`4OOPS! `oYou already requested for world rendering, you should wait!");
        return;
    }

    pPlayer->SetState(PLAYER_STATE_RENDERING_WORLD);
    GetMasterBroadway()->SendRenderWorldRequest(pPlayer->GetCurrentWorld(), pPlayer->GetUserID());
}
