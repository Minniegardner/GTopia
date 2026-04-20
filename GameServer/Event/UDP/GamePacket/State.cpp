#include "State.h"
#include "Item/ItemInfoManager.h"
#include "Utils/Timer.h"

namespace {

Vector2Float ResolveRespawnPosition(World* pWorld, GamePlayer* pPlayer)
{
    if(!pWorld || !pPlayer) {
        return { 0.0f, 0.0f };
    }

    Vector2Float respawnPos = pPlayer->GetRespawnPos();
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

void RespawnPlayerFromHealthLoss(World* pWorld, GamePlayer* pPlayer)
{
    if(!pWorld || !pPlayer) {
        return;
    }

    VariantVector killedData(2);
    killedData[0] = "OnKilled";
    killedData[1] = 1;
    pPlayer->SendCallFunctionPacket(killedData, pPlayer->GetNetID());

    const Vector2Float respawnPos = ResolveRespawnPosition(pWorld, pPlayer);
    pPlayer->SetHealth(200);
    pPlayer->SetLastDamageMS(Time::GetSystemTime());
    pPlayer->SetWorldPos(respawnPos.x, respawnPos.y);

    VariantVector setPosData(2);
    setPosData[0] = "OnSetPos";
    setPosData[1] = respawnPos;
    pPlayer->SendCallFunctionPacket(setPosData, pPlayer->GetNetID(), 2000);

    VariantVector freezeData(2);
    freezeData[0] = "OnSetFreezeState";
    freezeData[1] = 0;
    pPlayer->SendCallFunctionPacket(freezeData, pPlayer->GetNetID(), 2000);
}

}

void State::Execute(GamePlayer* pPlayer, World* pWorld, GameUpdatePacket* pPacket)
{
    if(!pPlayer || !pWorld || !pPacket) {
        return;
    }

    if(pPacket->posX == 0 || pPacket->posY == 0) {
        return;
    }

    Vector2Int worldSize = pWorld->GetTileManager()->GetSize();
    const float worldMaxX = static_cast<float>(worldSize.x * 32);
    const float worldMaxY = static_cast<float>(worldSize.y * 32);

    if(pPacket->posX < 0 || pPacket->posY < 0 || pPacket->posX > worldMaxX || pPacket->posY > worldMaxY) {
        pPlayer->SendFakePingReply();
        return;
    }

    if(!pPlayer->CanProcessMovePacket(pPacket->posX, pPacket->posY, Time::GetSystemTime())) {
        pPlayer->SendFakePingReply();
        return;
    }

    if(pPacket->HasFlag(NET_GAME_PACKET_FLAGS_FACINGLEFT)) {
        pPlayer->GetCharData().SetPlayerFlag(PLAYER_FLAG_FACING_LEFT);
    }
    else {
        pPlayer->GetCharData().RemovePlayerFlag(PLAYER_FLAG_FACING_LEFT);
    }

    pPlayer->SetWorldPos(pPacket->posX, pPacket->posY);
    pPlayer->SendPositionToWorldPlayers();

    if(pPacket->HasFlag(NET_GAME_PACKET_FLAGS_LAVA) || pPacket->HasFlag(NET_GAME_PACKET_FLAGS_ACID) || pPacket->HasFlag(NET_GAME_PACKET_FLAGS_DEATH)) {
        if(Time::GetSystemTime() - pPlayer->GetLastDamageMS() >= 3000) {
            pPlayer->SetHealth(200);
        }

        if(pPlayer->GetHealth() > 40) {
            pPlayer->SetHealth(pPlayer->GetHealth() - 40);
        }
        else {
            pPlayer->SetHealth(0);
        }

        if(pPlayer->GetHealth() == 0) {
            RespawnPlayerFromHealthLoss(pWorld, pPlayer);
        }

        pPlayer->SetLastDamageMS(Time::GetSystemTime());
    }

    const uint64 nowMS = Time::GetSystemTime();
    Vector2Int currentTilePos((int32)(pPacket->posX / 32.0f), (int32)(pPacket->posY / 32.0f));
    TileInfo* pSteppedTile = pWorld->GetTileManager()->GetTile(currentTilePos.x, currentTilePos.y);
    if(pSteppedTile) {
        ItemInfo* pSteppedItem = GetItemInfoManager()->GetItemByID(pSteppedTile->GetDisplayedItem());
        if(pSteppedItem && pSteppedItem->type == ITEM_TYPE_CHECKPOINT) {
            pPlayer->SetRespawnPos(pPacket->posX, pPacket->posY);
        }

        uint16 steppedFG = pSteppedTile->GetFG();
        if(
            (steppedFG == ITEM_ID_STEAM_STOMPER || steppedFG == ITEM_ID_STEAM_REVOLVER)
            && pPlayer->CanTriggerSteamByStep(currentTilePos, nowMS)
        ) {
            pWorld->TriggerSteamPulse(pSteppedTile);
        }
    }

    pPacket->netID = pPlayer->GetNetID();
    pWorld->SendGamePacketToAll(pPacket);
}