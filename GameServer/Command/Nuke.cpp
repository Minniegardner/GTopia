#include "Nuke.h"
#include "Utils/StringUtils.h"
#include "Player/Role.h"
#include "../Server/GameServer.h"
#include "../Server/MasterBroadway.h"
#include "../World/WorldManager.h"

const CommandInfo& Nuke::GetInfo()
{
    static CommandInfo info =
    {
        "/nuke",
        "Toggle nuked state for your current world",
        ROLE_PERM_COMMAND_BAN,
        {
            CompileTimeHashString("nuke")
        }
    };

    return info;
}

void Nuke::Execute(GamePlayer* pPlayer, std::vector<string>& args)
{
    if(!pPlayer || args.empty() || !CheckPerm(pPlayer)) {
        return;
    }

    if(args.size() != 1) {
        pPlayer->SendOnConsoleMessage("Usage: " + GetInfo().usage);
        return;
    }

    if(pPlayer->GetCurrentWorld() == 0) {
        return;
    }

    World* pWorld = GetWorldManager()->GetWorldByID(pPlayer->GetCurrentWorld());
    if(!pWorld) {
        return;
    }

    if(pWorld->HasWorldFlag(WORLD_FLAG_NUKED)) {
        pWorld->SetWorldFlag(WORLD_FLAG_NUKED, false);
        for(GamePlayer* pWorldPlayer : pWorld->GetPlayers()) {
            if(pWorldPlayer) {
                pWorldPlayer->SendOnConsoleMessage("`5World has been un-nuked.");
            }
        }
        return;
    }

    pWorld->SetWorldFlag(WORLD_FLAG_NUKED, true);

    std::vector<GamePlayer*> targets;
    targets.reserve(pWorld->GetPlayers().size());
    for(GamePlayer* pWorldPlayer : pWorld->GetPlayers()) {
        if(pWorldPlayer) {
            targets.push_back(pWorldPlayer);
        }
    }

    for(GamePlayer* pTarget : targets) {
        if(!pTarget) {
            continue;
        }

        Role* pRole = pTarget->GetRole();
        if(pRole && (pRole->HasPerm(ROLE_PERM_MSTATE) || pRole->HasPerm(ROLE_PERM_SMSTATE))) {
            continue;
        }

        pWorld->SendConsoleMessageToAll("`w" + pPlayer->GetRawName() + " `4world bans `w" + pTarget->GetRawName() + " `ofrom `w" + pWorld->GetWorlName() + "`o!");
         pTarget->PlaySFX("repair.wav");
        pWorld->ForceLeavePlayer(pTarget);
    }

    const string nukeBroadcast =
        "`o>>`4" + pWorld->GetWorlName() +
        " `4was nuked from orbit`o. It's the only way to be sure. Play nice, everybody!";

    for(GamePlayer* pOnline : GetGameServer()->FindPlayersByNamePrefix("", false, 0)) {
        if(!pOnline || !pOnline->HasState(PLAYER_STATE_IN_GAME)) {
            continue;
        }

        pOnline->SendOnConsoleMessage(nukeBroadcast);
    }

    GetMasterBroadway()->SendCrossServerActionRequest(
        TCP_CROSS_ACTION_SUPER_BROADCAST,
        pPlayer->GetUserID(),
        pPlayer->GetRawName(),
        "*",
        true,
        nukeBroadcast,
        0
    );
}
