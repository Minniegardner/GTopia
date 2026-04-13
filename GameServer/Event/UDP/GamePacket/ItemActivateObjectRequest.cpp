#include "ItemActivateObjectRequest.h"

#include "Item/ItemInfoManager.h"
#include "Math/Rect.h"
#include <cmath>

void ItemActivateObjectRequest::Execute(GamePlayer* pPlayer, World* pWorld, GameUpdatePacket* pPacket)
{
    if(!pPlayer || !pWorld || !pPacket) {
        return;
    }

    if(!pPlayer->CanCollectObjectNow()) {
        return;
    }

    if(pPacket->worldObjectID < 0) {
        pPlayer->SendFakePingReply();
        return;
    }

    WorldObject* pObject = pWorld->GetObjectManager()->GetObjectByID((uint32)pPacket->worldObjectID);
    if(!pObject || pObject->count == 0) {
        pPlayer->SendFakePingReply();
        return;
    }

    Vector2Float playerPos = pPlayer->GetWorldPos();
    const float dx = pObject->pos.x - playerPos.x;
    const float dy = pObject->pos.y - playerPos.y;
    const float distance = std::sqrt((dx * dx) + (dy * dy));

    if(distance > 96.0f) {
        pPlayer->SendFakePingReply();
        return;
    }

    pPlayer->ResetCollectObjectTime();

    if(pObject->itemID == ITEM_ID_GEMS) {
        pPlayer->AddGems(pObject->count);
        pPlayer->SendOnSetBux();
        pWorld->RemoveObject(pObject->objectID);
        return;
    }

    ItemInfo* pItemInfo = GetItemInfoManager()->GetItemByID(pObject->itemID);
    if(!pItemInfo) {
        pPlayer->SendFakePingReply();
        return;
    }

    const uint8 owned = pPlayer->GetInventory().GetCountOfItem(pObject->itemID);
    bool canAccept = false;
    uint8 freeSpace = 0;

    if(owned > 0) {
        if(owned < pItemInfo->maxCanHold) {
            canAccept = true;
            freeSpace = (uint8)(pItemInfo->maxCanHold - owned);
        }
    }
    else {
        if(pPlayer->GetInventory().HaveRoomForItem(pObject->itemID, 1)) {
            canAccept = true;
            freeSpace = pItemInfo->maxCanHold;
        }
    }

    if(!canAccept || freeSpace == 0) {
        pPlayer->SendFakePingReply();
        return;
    }

    const uint8 collected = std::min<uint8>(pObject->count, freeSpace);
    if(collected == 0) {
        pPlayer->SendFakePingReply();
        return;
    }

    pPlayer->ModifyInventoryItem(pObject->itemID, (int16)collected);

    if(collected >= pObject->count) {
        pWorld->RemoveObject(pObject->objectID);
    }
    else {
        pObject->count -= collected;
        pWorld->ModifyObject(*pObject);
    }
}
