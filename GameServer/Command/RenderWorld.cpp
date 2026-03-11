#include "Utils/StringUtils.h"
#include "RenderWorld.h"

const CommandInfo& RenderWorld::GetInfo()
{
    static CommandInfo info =
    {
        "renderworld",
        "View world as image",
        ROLE_PERM_MSTATE,
        {
            CompileTimeHashString("renderworld")
        }
    };

    return info;
}

void RenderWorld::Execute(GamePlayer* pPlayer, std::vector<string>& args)
{
    if(!pPlayer || args.empty()) {
        return;
    }

    pPlayer->SendOnConsoleMessage("hello from renderworld");
}
