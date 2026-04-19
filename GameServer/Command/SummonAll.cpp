#include "SummonAll.h"
#include "../Server/GameServer.h"
#include "../Server/MasterBroadway.h"
#include "../World/WorldManager.h"

const CommandInfo& SummonAll::GetInfo()
{
    static CommandInfo info =
    {
        "/summonall",
        "Summon all online players to your current world",
        ROLE_PERM_COMMAND_PULL,
        {
            CompileTimeHashString("summonall")
        }
    };

    return info;
}

void SummonAll::Execute(GamePlayer* pPlayer, std::vector<string>& args)
{
    if(!pPlayer || args.empty() || !CheckPerm(pPlayer)) {
        return;
    }

    if(pPlayer->GetCurrentWorld() == 0) {
        return;
    }

    World* pInvokerWorld = GetWorldManager()->GetWorldByID(pPlayer->GetCurrentWorld());
    if(!pInvokerWorld) {
        pPlayer->SendOnConsoleMessage("`4Oops:`` Could not locate your current world.");
        return;
    }

    const string worldName = pInvokerWorld->GetWorlName();
    for(GamePlayer* pTarget : GetGameServer()->FindPlayersByNamePrefix("", false, 0)) {
        if(!pTarget || pTarget == pPlayer) {
            continue;
        }

        pTarget->SendOnConsoleMessage("`oYou were summoned by ``" + pPlayer->GetDisplayName() + "`` to `w" + worldName + "``.");
        GetWorldManager()->PlayerJoinRequest(pTarget, worldName);
    }

    GetMasterBroadway()->SendCrossServerActionRequest(
        TCP_CROSS_ACTION_SUMMON_ALL,
        pPlayer->GetUserID(),
        pPlayer->GetRawName(),
        "ALL",
        true,
        worldName,
        0);

    pPlayer->SendOnConsoleMessage("`oSummon-all sent to all online players.");
}