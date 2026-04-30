#include "State.h"
#include "Item/ItemInfoManager.h"
#include "Utils/Timer.h"

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
        pPlayer->GetCharData().SetCharFlag(CHARACTER_FLAG_FACING_LEFT);
    }
    else {
        pPlayer->GetCharData().RemoveCharFlag(CHARACTER_FLAG_FACING_LEFT);
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