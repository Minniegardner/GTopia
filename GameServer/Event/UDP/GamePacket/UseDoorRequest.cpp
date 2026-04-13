#include "UseDoorRequest.h"

#include "Item/ItemInfoManager.h"
#include "Utils/StringUtils.h"
#include "World/TileInfo.h"
#include "../../../World/WorldManager.h"

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

void UseDoorRequest::Execute(GamePlayer* pPlayer, World* pWorld, GameUpdatePacket* pPacket)
{
    if(!pPlayer || !pWorld || !pPacket) {
        return;
    }

    if(pPacket->tileX == 0 && pPacket->tileY == 0) {
        return;
    }

    TileInfo* pTile = pWorld->GetTileManager()->GetTile(pPacket->tileX, pPacket->tileY);
    if(!pTile) {
        return;
    }

    ItemInfo* pItem = GetItemInfoManager()->GetItemByID(pTile->GetDisplayedItem());
    if(!pItem || (pItem->type != ITEM_TYPE_DOOR && pItem->type != ITEM_TYPE_USER_DOOR && pItem->type != ITEM_TYPE_PORTAL && pItem->type != ITEM_TYPE_SUNGATE)) {
        return;
    }

    if(!pWorld->PlayerHasAccessOnTile(pPlayer, pTile) && pItem->type != ITEM_TYPE_PORTAL && pItem->type != ITEM_TYPE_SUNGATE) {
        pPlayer->SendFakePingReply();
        return;
    }

    if(!pWorld->CanBreak(pPlayer, pTile)) {
        pPlayer->SendFakePingReply();
        return;
    }

    TileExtra_Door* pDoor = pTile->GetExtra<TileExtra_Door>();
    if(!pDoor) {
        return;
    }

    Vector2Int targetPos = pTile->GetPos();
    if(!pDoor->text.empty()) {
        string target = pDoor->text;
        RemoveExtraWhiteSpaces(target);
        target = ToUpper(target);

        auto splitPos = target.find(':');
        if(splitPos != string::npos) {
            string worldName = target.substr(0, splitPos);
            string doorID = target.substr(splitPos + 1);
            if(!worldName.empty()) {
                GetWorldManager()->PlayerJoinRequest(pPlayer, worldName);
                return;
            }

            if(!doorID.empty()) {
                TileInfo* pTargetDoor = FindDoorByID(pWorld, ToUpper(doorID));
                if(pTargetDoor) {
                    targetPos = pTargetDoor->GetPos();
                }
            }
        }
    }

    pPlayer->SetWorldPos(targetPos.x * 32.0f, targetPos.y * 32.0f);
    pPlayer->SetRespawnPos(targetPos.x * 32.0f, targetPos.y * 32.0f);
    pPlayer->SendOnSetPos(targetPos.x * 32.0f, targetPos.y * 32.0f);
}
