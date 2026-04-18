#include "KickAllComponent.h"

#include "../Player/GamePlayer.h"
#include "../World/World.h"
#include "World/TileInfo.h"

namespace {

constexpr uint64 kKickAllCooldownMS = 10ull * 60ull * 1000ull;

void SendFreezeState(GamePlayer* pPlayer, int32 freezeState, int32 delayMS = -1)
{
    if(!pPlayer) {
        return;
    }

    VariantVector data(2);
    data[0] = "OnSetFreezeState";
    data[1] = freezeState;
    pPlayer->SendCallFunctionPacket(data, pPlayer->GetNetID(), delayMS);
}

void SendSetPos(GamePlayer* pPlayer, const Vector2Float& pos, int32 delayMS)
{
    if(!pPlayer) {
        return;
    }

    VariantVector data(2);
    data[0] = "OnSetPos";
    data[1] = pos;
    pPlayer->SendCallFunctionPacket(data, pPlayer->GetNetID(), delayMS);
}

Vector2Float ResolveRespawnPosition(World* pWorld, GamePlayer* pTarget)
{
    if(!pWorld || !pTarget) {
        return { 0.0f, 0.0f };
    }

    Vector2Float respawnPos = pTarget->GetRespawnPos();
    if(respawnPos.x != 0.0f || respawnPos.y != 0.0f) {
        return respawnPos;
    }

    TileInfo* pDoor = pWorld->GetTileManager()->GetKeyTile(KEY_TILE_MAIN_DOOR);
    if(!pDoor) {
        return { 0.0f, 0.0f };
    }

    const Vector2Int doorPos = pDoor->GetPos();
    return { doorPos.x * 32.0f, doorPos.y * 32.0f };
}

void ForceRespawnPlayer(World* pWorld, GamePlayer* pTarget)
{
    if(!pWorld || !pTarget) {
        return;
    }

    VariantVector killed(2);
    killed[0] = "OnKilled";
    killed[1] = 1;
    pTarget->SendCallFunctionPacket(killed, pTarget->GetNetID());
    SendFreezeState(pTarget, 2);

    const Vector2Float respawnPos = ResolveRespawnPosition(pWorld, pTarget);
    pTarget->SetWorldPos(respawnPos.x, respawnPos.y);
    pTarget->SetRespawnPos(respawnPos.x, respawnPos.y);

    SendSetPos(pTarget, respawnPos, 2000);
    SendFreezeState(pTarget, 0, 2000);
}

}

bool KickAllComponent::Execute(GamePlayer* pInvoker, World* pWorld, uint32& outAffected, string& outMessage)
{
    outAffected = 0;
    outMessage.clear();

    if(!pInvoker || !pWorld) {
        outMessage = "Invalid context for /kickall.";
        return false;
    }

    const uint64 nowMS = Time::GetSystemTime();
    const uint64 lastKickAllMS = pWorld->GetLastKickAllMS();
    if(lastKickAllMS > 0 && nowMS < (lastKickAllMS + kKickAllCooldownMS)) {
        const uint64 remainingMS = (lastKickAllMS + kKickAllCooldownMS) - nowMS;
        const uint64 remainingSec = (remainingMS + 999ull) / 1000ull;
        outMessage = "`4KickAll is on cooldown for `w" + ToString(remainingSec) + "`4 more second(s).``";
        return false;
    }

    const auto players = pWorld->GetPlayers();
    for(GamePlayer* pTarget : players) {
        if(!pTarget || pTarget == pInvoker || !pTarget->HasState(PLAYER_STATE_IN_GAME)) {
            continue;
        }

        pTarget->SendOnConsoleMessage("`4(KICKALL ACTIVATED!)``");
        ForceRespawnPlayer(pWorld, pTarget);
        ++outAffected;
    }

    if(outAffected > 0) {
        pWorld->PlaySFXForEveryone("teleport.wav", 0);
    }

    pWorld->SetLastKickAllMS(nowMS);
    outMessage = "`oKickAll activated. Affected `w" + ToString(outAffected) + "`` player(s).";
    return true;
}
