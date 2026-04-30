#include "RandomPull.h"
#include "../Server/GameServer.h"
#include "../World/WorldManager.h"
#include "Math/Random.h"

const CommandInfo& RandomPull::GetInfo()
{
    static CommandInfo info =
    {
        "/randompull",
        "Pull a random player in your world",
        ROLE_PERM_COMMAND_RANDOMPULL,
        {
            CompileTimeHashString("randompull")
        }
    };

    return info;
}

void RandomPull::Execute(GamePlayer* pPlayer, std::vector<string>& args)
{
    if(!pPlayer || args.empty() || !CheckPerm(pPlayer)) {
        return;
    }

    if(pPlayer->GetCurrentWorld() == 0) {
        return;
    }

    auto matches = GetPlayerManager()->FindPlayersByNamePrefix("", true, pPlayer->GetCurrentWorld());

    std::vector<GamePlayer*> candidates;
    candidates.reserve(matches.size());
    for(GamePlayer* pTarget : matches) {
        if(pTarget && pTarget != pPlayer) {
            candidates.push_back(pTarget);
        }
    }

    if(candidates.empty()) {
        pPlayer->SendOnConsoleMessage("`4Oops:`` There's no other player in the world to random pull!");
        return;
    }

    const int32 pickedIdx = RandomRangeInt(0, static_cast<int32>(candidates.size()) - 1);
    GamePlayer* pTarget = candidates[pickedIdx];
    if(!pTarget) {
        pPlayer->SendOnConsoleMessage("`4Oops:`` Player already left.");
        return;
    }

    const Vector2Float srcPos = pPlayer->GetWorldPos();
    const float pullX = std::max(0.0f, srcPos.x + 32.0f);
    const float pullY = std::max(0.0f, srcPos.y);

    pTarget->SetWorldPos(pullX, pullY);
    pTarget->SetRespawnPos(pullX, pullY);
    pTarget->SendPositionToWorldPlayers();

    World* pWorld = GetWorldManager()->GetWorldByID(pPlayer->GetCurrentWorld());
    if(pWorld) {
        pWorld->SendConsoleMessageToAll(pPlayer->GetDisplayName() + " `5pulls `w" + pTarget->GetDisplayName() + "`o!");
    }
}
