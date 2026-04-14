#include "UseDoorRequest.h"

#include "Item/ItemInfoManager.h"
#include "Utils/StringUtils.h"
#include "World/TileInfo.h"
#include "../../../World/WorldPathfinding.h"
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

    TileInfo* pTile = pWorld->GetTileManager()->GetTile(pPacket->tileX, pPacket->tileY);
    if(!pTile) {
        return;
    }

    ItemInfo* pItem = GetItemInfoManager()->GetItemByID(pTile->GetDisplayedItem());
    if(!pItem || (pItem->type != ITEM_TYPE_DOOR && pItem->type != ITEM_TYPE_USER_DOOR && pItem->type != ITEM_TYPE_PORTAL && pItem->type != ITEM_TYPE_SUNGATE)) {
        return;
    }

    const Vector2Float currentPos = pPlayer->GetWorldPos();
    const Vector2Float pathTargetPos = {
        static_cast<float>((pTile->GetPos().x * 32) + 2),
        static_cast<float>(pTile->GetPos().y * 32)
    };
    if(!WorldPathfinding::HasPath(pWorld, pPlayer, currentPos, pathTargetPos)) {
        pPlayer->SendOnTalkBubble("Something is blocking the way, get closer.", true);
        pPlayer->SendFakePingReply();
        return;
    }

    TileExtra_Door* pDoor = pTile->GetExtra<TileExtra_Door>();
    if(!pDoor) {
        return;
    }

    if(!pWorld->CanBreak(pPlayer, pTile) && !pTile->HasFlag(TILE_FLAG_IS_OPEN_TO_PUBLIC)) {
        pPlayer->PlaySFX("punch_locked.wav");
        pPlayer->SendOnTalkBubble("``The door is `4locked``!", true);
        return;
    }

    Vector2Int targetPos = pTile->GetPos();
    if(!pDoor->text.empty()) {
        string target = pDoor->text;
        RemoveAllSpaces(target);
        target = ToUpper(target);

        if(target.find(':') == string::npos) {
            pPlayer->SetPendingDoorWarpID("");
            GetWorldManager()->PlayerJoinRequest(pPlayer, target);
            return;
        }

        auto splitPos = target.find(':');
        if(splitPos != string::npos) {
            string worldName = target.substr(0, splitPos);
            string doorID = target.substr(splitPos + 1);
            if(!worldName.empty()) {
                pPlayer->SetPendingDoorWarpID(ToUpper(doorID));
                GetWorldManager()->PlayerJoinRequest(pPlayer, worldName);
                return;
            }

            if(!doorID.empty()) {
                TileInfo* pTargetDoor = FindDoorByID(pWorld, ToUpper(doorID));
                if(pTargetDoor) {
                    targetPos = pTargetDoor->GetPos();
                }
                else {
                    TileInfo* pMainDoorTile = pWorld->GetTileManager()->GetKeyTile(KEY_TILE_MAIN_DOOR);
                    if(pMainDoorTile) {
                        targetPos = pMainDoorTile->GetPos();
                    }
                }
            }
        }
    }

    pPlayer->SetWorldPos(targetPos.x * 32.0f, targetPos.y * 32.0f);
    pPlayer->SetRespawnPos(targetPos.x * 32.0f, targetPos.y * 32.0f);
    pPlayer->PlaySFX("door_open.wav", 200);
    pPlayer->SendOnSetPos(targetPos.x * 32.0f, targetPos.y * 32.0f);
}
