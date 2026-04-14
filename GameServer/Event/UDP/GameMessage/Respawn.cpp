#include "Respawn.h"

#include "../../../World/WorldManager.h"

namespace {

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

}

void Respawn::Execute(GamePlayer* pPlayer, ParsedTextPacket<8>& packet)
{
    if(!pPlayer || !pPlayer->HasState(PLAYER_STATE_IN_GAME)) {
        return;
    }

    World* pWorld = GetWorldManager()->GetWorldByID(pPlayer->GetCurrentWorld());
    if(!pWorld) {
        return;
    }

    bool spikeRespawn = false;
    auto pAction = packet.Find(CompileTimeHashString("action"));
    if(pAction && pAction->size > 0) {
        string action(pAction->value, pAction->size);
        spikeRespawn = (action == "respawn_spike");
    }

    if(!spikeRespawn) {
        VariantVector data(2);
        data[0] = "OnKilled";
        data[1] = 1;
        pPlayer->SendCallFunctionPacket(data, pPlayer->GetNetID());
        SendFreezeState(pPlayer, 2);
    }

    if(pPlayer->IsTrading()) {
        pPlayer->CancelTrade(false);
    }

    Vector2Float respawnPos = pPlayer->GetRespawnPos();
    if(respawnPos.x == 0.0f && respawnPos.y == 0.0f) {
        TileInfo* pDoor = pWorld->GetTileManager()->GetKeyTile(KEY_TILE_MAIN_DOOR);
        if(pDoor) {
            const Vector2Int doorPos = pDoor->GetPos();
            respawnPos = { doorPos.x * 32.0f, doorPos.y * 32.0f };
        }
    }

    pPlayer->SetWorldPos(respawnPos.x, respawnPos.y);
    SendSetPos(pPlayer, respawnPos, 2000);
    SendFreezeState(pPlayer, 0, 2000);

    pWorld->PlaySFXForEveryone("teleport.wav", 2000);
}
