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
        pPlayer->SendOnTalkBubble("Too far away to pick that up.", true);
        pPlayer->SendFakePingReply();
        return;
    }

    pPlayer->ResetCollectObjectTime();

    if(pObject->itemID == ITEM_ID_GEMS) {
        GameUpdatePacket pickupAnim;
        pickupAnim.type = NET_GAME_PACKET_ITEM_ACTIVATE_OBJECT_REQUEST;
        pickupAnim.netID = pPlayer->GetNetID();
        pickupAnim.worldObjectID = pObject->objectID;
        pickupAnim.itemID = pObject->itemID;
        pickupAnim.worldObjectCount = pObject->count;
        pickupAnim.flags = NET_GAME_PACKET_FLAGS_PICKUP;
        pWorld->SendGamePacketToAll(&pickupAnim);

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
        pPlayer->SendOnTalkBubble("Your inventory is full.", true);
        pPlayer->SendFakePingReply();
        return;
    }

    const uint8 collected = std::min<uint8>(pObject->count, freeSpace);
    if(collected == 0) {
        pPlayer->SendFakePingReply();
        return;
    }

    GameUpdatePacket pickupAnim;
    pickupAnim.type = NET_GAME_PACKET_ITEM_ACTIVATE_OBJECT_REQUEST;
    pickupAnim.netID = pPlayer->GetNetID();
    pickupAnim.worldObjectID = pObject->objectID;
    pickupAnim.itemID = pObject->itemID;
    pickupAnim.worldObjectCount = collected;
    pickupAnim.flags = NET_GAME_PACKET_FLAGS_PICKUP;
    pWorld->SendGamePacketToAll(&pickupAnim);

    pPlayer->ModifyInventoryItem(pObject->itemID, (int16)collected);

    if(pObject->itemID == ITEM_ID_COMET_DUST && collected > 0) {
        pPlayer->GiveAchievement("Sparkly Dust (Classic)");
    }

    if(pItemInfo->rarity >= 999) {
        pPlayer->SendOnConsoleMessage("Collected `w" + std::to_string((int32)collected) + " " + pItemInfo->name + "``.");
    }
    else {
        pPlayer->SendOnConsoleMessage(
            "Collected `w" + std::to_string((int32)collected) + " " + pItemInfo->name + "``. Rarity: `w" + std::to_string((int32)pItemInfo->rarity) + "``"
        );
    }

    if(collected > 0) {
        pPlayer->PlaySFX("pickup.wav", 0);
    }

    if(collected >= pObject->count) {
        pWorld->RemoveObject(pObject->objectID);
    }
    else {
        pObject->count -= collected;
        pWorld->ModifyObject(*pObject);
    }
}
