#include "WarpTo.h"
#include "Utils/StringUtils.h"
#include "CommandTargetUtils.h"
#include "../Server/GameServer.h"
#include "../Server/MasterBroadway.h"
#include "../World/WorldManager.h"

const CommandInfo& WarpTo::GetInfo()
{
    static CommandInfo info =
    {
        "/warpto <target|#userid>",
        "Warp to a player's current world",
        ROLE_PERM_COMMAND_WARPTO,
        {
            CompileTimeHashString("warpto")
        }
    };

    return info;
}

void WarpTo::Execute(GamePlayer* pPlayer, std::vector<string>& args)
{
    if(!pPlayer || args.empty() || !CheckPerm(pPlayer)) {
        return;
    }

    if(args.size() < 2) {
        pPlayer->SendOnConsoleMessage("Usage: " + GetInfo().usage);
        return;
    }

    CommandTargetSpec targetSpec;
    string targetError;
    if(!ParseCommandTargetArg(args[1], true, targetSpec, targetError)) {
        pPlayer->SendOnConsoleMessage(targetError);
        return;
    }

    auto matches = ResolveLocalTargets(targetSpec, false, 0);
    if(matches.empty()) {
        GetMasterBroadway()->SendCrossServerActionRequest(
            TCP_CROSS_ACTION_WARPTO,
            pPlayer->GetUserID(),
            pPlayer->GetRawName(),
            targetSpec.query,
            targetSpec.exactMatch,
            string(),
            0);
        pPlayer->SendOnConsoleMessage("`oSearching target globally and preparing warp...");
        return;
    }

    if(!targetSpec.exactMatch && !targetSpec.byUserID && matches.size() > 1) {
        SendAmbiguousLocalTargetList(pPlayer, targetSpec.query, matches, "server");
        return;
    }

    GamePlayer* pTarget = matches[0];
    if(!pTarget || pTarget == pPlayer) {
        pPlayer->SendOnConsoleMessage("Nope, you can't use that on yourself.");
        return;
    }

    const uint32 targetWorldID = pTarget->GetCurrentWorld();
    if(targetWorldID == 0) {
        pPlayer->SendOnConsoleMessage("`4Oops:`` Target player is not in a world right now.");
        return;
    }

    World* pWorld = GetWorldManager()->GetWorldByID(targetWorldID);
    if(!pWorld) {
        pPlayer->SendOnConsoleMessage("`4Oops:`` Unable to locate that world right now.");
        return;
    }

    const string worldName = pWorld->GetWorlName();
    pPlayer->SendOnConsoleMessage("`oWarping to `w" + pTarget->GetRawName() + "`o at `w" + worldName + "``...");
    pPlayer->SendOnTextOverlay("Warping to `w" + pTarget->GetRawName() + "`` at (`2" + worldName + "``)..");

    GetWorldManager()->PlayerJoinRequest(pPlayer, worldName);
}
