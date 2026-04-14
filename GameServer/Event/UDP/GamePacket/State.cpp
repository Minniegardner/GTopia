#include "State.h"
#include "Item/ItemInfoManager.h"
#include "Utils/Timer.h"

namespace {

void SendFreezeState(GamePlayer* pPlayer, bool freeze, int32 delayMS = -1)
{
    if(!pPlayer) {
        return;
    }

    VariantVector data(2);
    data[0] = "OnSetFreezeState";
    data[1] = freeze ? 1 : 0;
    pPlayer->SendCallFunctionPacket(data, -1, delayMS);
}

void SendDelayedSetPos(GamePlayer* pPlayer, const Vector2Float& pos, int32 delayMS)
{
    if(!pPlayer) {
        return;
    }

    VariantVector data(2);
    data[0] = "OnSetPos";
    data[1] = Vector2Float(pos.x, pos.y);
    pPlayer->SendCallFunctionPacket(data, -1, delayMS);
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

    if(pPacket->HasFlag(NET_GAME_PACKET_FLAGS_DEATH) || pPacket->HasFlag(NET_GAME_PACKET_FLAGS_RESPAWN)) {
        Vector2Float respawnPos = pPlayer->GetRespawnPos();
        if(respawnPos.x <= 0.0f && respawnPos.y <= 0.0f) {
            TileInfo* pMainDoor = pWorld->GetTileManager()->GetKeyTile(KEY_TILE_MAIN_DOOR);
            if(pMainDoor) {
                const Vector2Int mainDoorPos = pMainDoor->GetPos();
                respawnPos = Vector2Float(mainDoorPos.x * 32.0f, mainDoorPos.y * 32.0f);
            }
        }

        if(pPlayer->IsTrading()) {
            pPlayer->CancelTrade(false);
        }

        VariantVector killedData(2);
        killedData[0] = "OnKilled";
        killedData[1] = 1;
        pPlayer->SendCallFunctionPacket(killedData);

        SendFreezeState(pPlayer, true);
        SendDelayedSetPos(pPlayer, respawnPos, 1700);
        SendFreezeState(pPlayer, false, 1800);
        pPlayer->PlaySFX("teleport.wav", 1700);

        pPlayer->SetWorldPos(respawnPos.x, respawnPos.y);
        pPlayer->SendPositionToWorldPlayers();
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