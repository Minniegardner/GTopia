#include "KickAll.h"
#include "Utils/StringUtils.h"
#include "../Server/GameServer.h"
#include "../World/WorldManager.h"
#include "../Component/KickAllComponent.h"

const CommandInfo& KickAll::GetInfo()
{
    static CommandInfo info =
    {
        "/kickall",
        "Kick all players in your current world",
        ROLE_PERM_COMMAND_KICK,
        {
            CompileTimeHashString("kickall")
        }
    };

    return info;
}

void KickAll::Execute(GamePlayer* pPlayer, std::vector<string>& args)
{
    if(!pPlayer || args.empty() || !CheckPerm(pPlayer)) {
        return;
    }

    if(pPlayer->GetCurrentWorld() == 0) {
        return;
    }

    World* pWorld = GetWorldManager()->GetWorldByID(pPlayer->GetCurrentWorld());
    if(!pWorld) {
        return;
    }

    uint32 kicked = 0;
    string resultMessage;
    KickAllComponent::Execute(pPlayer, pWorld, kicked, resultMessage);

    if(!resultMessage.empty()) {
        pPlayer->SendOnConsoleMessage(resultMessage);
    }
}
