#include "BlockActivate.h"

#include "Item/ItemInfoManager.h"
#include "Utils/StringUtils.h"
#include "../../../World/WorldPathfinding.h"
#include "World/World.h"
#include "World/TileInfo.h"

namespace {

TileInfo* FindDoorByID(World* pWorld, const string& doorID)
{
    if(!pWorld || doorID.empty()) {
        return nullptr;
    }

    Vector2Int size = pWorld->GetTileManager()->GetSize();
    for(int32 y = 0; y < size.y; ++y) {
        for(int32 x = 0; x < size.x; ++x) {
            TileInfo* pTile = pWorld->GetTileManager()->GetTile(x, y);
            if(!pTile) {
                continue;
            }

            ItemInfo* pItem = GetItemInfoManager()->GetItemByID(pTile->GetDisplayedItem());
            if(!pItem || (pItem->type != ITEM_TYPE_DOOR && pItem->type != ITEM_TYPE_USER_DOOR && pItem->type != ITEM_TYPE_PORTAL && pItem->type != ITEM_TYPE_SUNGATE)) {
                continue;
            }

            TileExtra_Door* pDoor = pTile->GetExtra<TileExtra_Door>();
            if(pDoor && ToUpper(pDoor->id) == doorID) {
                return pTile;
            }
        }
    }

    return nullptr;
}

}

void BlockActivate::Execute(GamePlayer* pPlayer, World* pWorld, GameUpdatePacket* pPacket)
{
    if(!pPlayer || !pWorld || !pPacket) {
        return;
    }

    TileInfo* pTile = pWorld->GetTileManager()->GetTile(pPacket->tileX, pPacket->tileY);
    if(!pTile) {
        return;
    }

    ItemInfo* pItem = GetItemInfoManager()->GetItemByID(pTile->GetDisplayedItem());
    if(!pItem) {
        return;
    }

    const Vector2Float currentPos = pPlayer->GetWorldPos();
    const Vector2Float targetPos = {
        static_cast<float>((pTile->GetPos().x * 32) + 2),
        static_cast<float>(pTile->GetPos().y * 32)
    };

    if(!WorldPathfinding::HasPath(pWorld, pPlayer, currentPos, targetPos)) {
        pPlayer->SendOnTalkBubble("Something is blocking the way, get closer.", true);
        pPlayer->SendFakePingReply();
        return;
    }

    switch(pItem->type) {
        case ITEM_TYPE_PORTAL:
        case ITEM_TYPE_DOOR:
        {
            TileExtra_Door* pDoor = pTile->GetExtra<TileExtra_Door>();
            if(!pDoor) {
                return;
            }

            if(!pWorld->CanBreak(pPlayer, pTile) && !pTile->HasFlag(TILE_FLAG_IS_OPEN_TO_PUBLIC)) {
                pPlayer->PlaySFX("punch_locked.wav");
                pPlayer->SendOnTalkBubble("``The door is `4locked``!", true);
                return;
            }

            string doorDest = pDoor->id.empty() ? pDoor->text : pDoor->text;
            RemoveAllSpaces(doorDest);
            doorDest = ToUpper(doorDest);

            auto splitPos = doorDest.find(':');
            if(splitPos == string::npos) {
                pPlayer->SetPendingDoorWarpID("");
                GetWorldManager()->PlayerJoinRequest(pPlayer, doorDest);
                return;
            }

            string worldName = doorDest.substr(0, splitPos);
            string doorID = doorDest.substr(splitPos + 1);
            if(!worldName.empty()) {
                pPlayer->SetPendingDoorWarpID(ToUpper(doorID));
                GetWorldManager()->PlayerJoinRequest(pPlayer, worldName);
                return;
            }

            if(!doorID.empty()) {
                TileInfo* pTargetDoor = FindDoorByID(pWorld, ToUpper(doorID));
                if(pTargetDoor) {
                    pPlayer->SetWorldPos((float)pTargetDoor->GetPos().x * 32.0f, (float)pTargetDoor->GetPos().y * 32.0f);
                    pPlayer->SetRespawnPos((float)pTargetDoor->GetPos().x * 32.0f, (float)pTargetDoor->GetPos().y * 32.0f);
                    pPlayer->PlaySFX("door_open.wav", 200);
                    pPlayer->SendOnSetPos((float)pTargetDoor->GetPos().x * 32.0f, (float)pTargetDoor->GetPos().y * 32.0f);
                    return;
                }
            }

            pPlayer->PlaySFX("door_open.wav", 200);
            pPlayer->SendOnSetPos((float)pTile->GetPos().x * 32.0f, (float)pTile->GetPos().y * 32.0f);
            return;
        }
        case ITEM_TYPE_CHECKPOINT:
        {
            pPlayer->SetRespawnPos({static_cast<float>((pTile->GetPos().x * 32) + 2), static_cast<float>(pTile->GetPos().y * 32)});
            pPlayer->SendOnSetPos((float)pTile->GetPos().x * 32.0f, (float)pTile->GetPos().y * 32.0f);
            return;
        }
        case ITEM_TYPE_BEDROCK:
        case ITEM_TYPE_NORMAL:
        default:
            return;
    }
}