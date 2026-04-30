#include "Summon.h"
#include "Utils/StringUtils.h"
#include "../Server/GameServer.h"
#include "../Server/MasterBroadway.h"
#include "../World/WorldManager.h"
#include <algorithm>

const CommandInfo& Summon::GetInfo()
{
    static CommandInfo info =
    {
        "/summon <player_prefix>",
        "Summon player to your current world",
        ROLE_PERM_COMMAND_PULL,
        {
            CompileTimeHashString("summon")
        }
    };

    return info;
}

void Summon::Execute(GamePlayer* pPlayer, std::vector<string>& args)
{
    if(!pPlayer || args.empty() || !CheckPerm(pPlayer)) {
        return;
    }

    if(pPlayer->GetCurrentWorld() == 0) {
        return;
    }

    if(args.size() < 2 || args[1].size() < 3) {
        pPlayer->SendOnConsoleMessage("`oUsage: " + GetInfo().usage);
        return;
    }

    string query = args[1];
    bool exactMatch = false;
    if(!query.empty() && query[0] == '/') {
        exactMatch = true;
        query.erase(query.begin());
    }

    auto matches = GetPlayerManager()->FindPlayersByNamePrefix(query, false, 0);
    if(matches.empty()) {
        World* pInvokerWorld = GetWorldManager()->GetWorldByID(pPlayer->GetCurrentWorld());
        if(!pInvokerWorld) {
            pPlayer->SendOnConsoleMessage("`4Oops:`` Could not locate your current world.");
            return;
        }

        GetMasterBroadway()->SendCrossServerActionRequest(
            TCP_CROSS_ACTION_SUMMON,
            pPlayer->GetUserID(),
            pPlayer->GetRawName(),
            query,
            exactMatch,
            pInvokerWorld->GetWorlName(),
            0);
        return;
    }

    if(!exactMatch && matches.size() > 1) {
        pPlayer->SendOnConsoleMessage("`oThere are multiple players starting with `w" + query + "`o, be more specific.");
        return;
    }

    GamePlayer* pTarget = matches[0];
    if(!pTarget || pTarget == pPlayer) {
        pPlayer->SendOnConsoleMessage("Nope, you can't use that on yourself.");
        return;
    }

    World* pInvokerWorld = GetWorldManager()->GetWorldByID(pPlayer->GetCurrentWorld());
    if(!pInvokerWorld) {
        pPlayer->SendOnConsoleMessage("`4Oops:`` Could not locate your current world.");
        return;
    }

    const string targetWorldName = pInvokerWorld->GetWorlName();

    if(pTarget->GetCurrentWorld() != pPlayer->GetCurrentWorld()) {
        pTarget->SendOnConsoleMessage("`oYou were summoned by ``" + pPlayer->GetDisplayName() + "`` to `w" + targetWorldName + "``.");
        GetWorldManager()->PlayerJoinRequest(pTarget, targetWorldName);
    }

    const Vector2Float srcPos = pPlayer->GetWorldPos();
    const float summonX = std::max(0.0f, srcPos.x + 32.0f);
    const float summonY = std::max(0.0f, srcPos.y);

    pTarget->SetWorldPos(summonX, summonY);
    pTarget->SetRespawnPos(summonX, summonY);
    pTarget->SendPositionToWorldPlayers();

    pPlayer->SendOnConsoleMessage("`oSummoned ``" + pTarget->GetDisplayName() + "``.");
}
