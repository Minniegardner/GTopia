#include "PInfo.h"
#include "Utils/StringUtils.h"
#include "CommandTargetUtils.h"
#include "../Context.h"
#include "../Server/GameServer.h"
#include "../Server/MasterBroadway.h"
#include "../World/WorldManager.h"

namespace {

void SendPlayerInfoLine(GamePlayer* pViewer, GamePlayer* pTarget)
{
    if(!pViewer || !pTarget) {
        return;
    }

    World* pWorld = GetWorldManager()->GetWorldByID(pTarget->GetCurrentWorld());
    const string worldName = pWorld ? pWorld->GetWorlName() : "EXIT";
    pViewer->SendOnConsoleMessage(
        "`w" + pTarget->GetRawName() + "`` (UID: `o" + ToString(pTarget->GetUserID()) +
        "`` | NID: `o" + ToString(pTarget->GetNetID()) +
        "``) - LV `o" + ToString(pTarget->GetLevel()) +
        "`` - Gems `o" + ToString(pTarget->GetGems()) +
        "`` - Server `o" + ToString(GetContext()->GetID()) +
        "`` - World `o" + worldName + "``"
    );
}

}

const CommandInfo& PInfo::GetInfo()
{
    static CommandInfo info =
    {
        "/pinfo <target|#userid>",
        "Lookup online player info",
        ROLE_PERM_COMMAND_MOD,
        {
            CompileTimeHashString("pinfo")
        }
    };

    return info;
}

void PInfo::Execute(GamePlayer* pPlayer, std::vector<string>& args)
{
    if(!pPlayer || args.empty() || !CheckPerm(pPlayer)) {
        return;
    }

    World* pWorld = GetWorldManager()->GetWorldByID(pPlayer->GetCurrentWorld());
    if(!pWorld) {
        return;
    }

    if(args.size() != 2) {
        pPlayer->SendOnConsoleMessage("Usage: " + GetInfo().usage);
        return;
    }

    CommandTargetSpec targetSpec;
    string targetError;
    if(!ParseCommandTargetArg(args[1], true, targetSpec, targetError)) {
        pPlayer->SendOnConsoleMessage(targetError);
        return;
    }

    pPlayer->SendOnConsoleMessage("Searching for `w" + targetSpec.query + "``...");

    auto players = ResolveLocalTargets(targetSpec, false, 0);
    if(players.empty()) {
        GetMasterBroadway()->SendCrossServerActionRequest(
            TCP_CROSS_ACTION_PINFO,
            pPlayer->GetUserID(),
            pPlayer->GetRawName(),
            targetSpec.query,
            targetSpec.exactMatch,
            string(),
            0);
        return;
    }

    if(!targetSpec.exactMatch && !targetSpec.byUserID && players.size() > 1) {
        SendAmbiguousLocalTargetList(pPlayer, targetSpec.query, players, "server");
        return;
    }

    for(GamePlayer* pTarget : players) {
        if(!pTarget) {
            continue;
        }

        SendPlayerInfoLine(pPlayer, pTarget);
    }
}
