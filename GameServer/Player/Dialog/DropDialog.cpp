#include "DropDialog.h"

#include "../GamePlayer.h"
#include "Utils/DialogBuilder.h"
#include "Item/ItemInfoManager.h"
#include "../../World/WorldManager.h"

void DropDialog::Request(GamePlayer* pPlayer, uint16 itemID)
{
    if(!pPlayer) {
        return;
    }

    ItemInfo* pItem = GetItemInfoManager()->GetItemByID(itemID);
    if(!pItem) {
        return;
    }

    if(itemID == ITEM_ID_FIST || itemID == ITEM_ID_WRENCH || pItem->HasFlag(ITEM_FLAG_UNTRADEABLE)) {
        pPlayer->PlaySFX("cant_place_tile.wav");
        pPlayer->SendOnTextOverlay("You can't drop that.");
        return;
    }

    uint8 invItemCount = pPlayer->GetInventory().GetCountOfItem(itemID);
    if(invItemCount == 0) {
        return;
    }

    DialogBuilder db;
    db.SetDefaultColor('o')
    ->AddLabelWithIcon("`wDrop " + pItem->name + "``", pItem->id, true)
    ->AddTextBox("How many to drop?")
    ->AddTextInput("count", "", ToString(invItemCount), 5)
    ->EmbedData("itemID", itemID)
    ->EndDialog("drop_item", "OK", "Cancel");

    pPlayer->SendOnDialogRequest(db.Get());
}

void DropDialog::Handle(GamePlayer* pPlayer, uint16 itemID, int16 itemCount)
{
    if(!pPlayer || pPlayer->GetCurrentWorld() == 0) {
        return;
    }

    ItemInfo* pItem = GetItemInfoManager()->GetItemByID(itemID);
    if(!pItem || pItem->HasFlag(ITEM_FLAG_UNTRADEABLE) || itemID == ITEM_ID_FIST || itemID == ITEM_ID_WRENCH) {
        return;
    }

    if(itemCount <= 0) {
        return;
    }

    uint8 invCount = pPlayer->GetInventory().GetCountOfItem(itemID);
    if(invCount == 0) {
        return;
    }

    if(itemCount > invCount) {
        itemCount = invCount;
    }

    World* pWorld = GetWorldManager()->GetWorldByID(pPlayer->GetCurrentWorld());
    if(!pWorld) {
        return;
    }

    Vector2Float playerPos = pPlayer->GetWorldPos();
    int32 tileX = (int32)(playerPos.x / 32.0f);
    int32 tileY = (int32)(playerPos.y / 32.0f);

    if(pPlayer->GetCharData().HasPlayerFlag(PLAYER_FLAG_FACING_LEFT)) {
        tileX -= 1;
    }
    else {
        tileX += 1;
    }

    TileInfo* pDropTile = pWorld->GetTileManager()->GetTile(tileX, tileY);
    if(!pDropTile) {
        pDropTile = pWorld->GetTileManager()->GetTile((int32)(playerPos.x / 32.0f), (int32)(playerPos.y / 32.0f));
    }

    if(!pDropTile) {
        return;
    }

    pPlayer->ModifyInventoryItem(itemID, -itemCount);

    WorldObject dropObj;
    dropObj.itemID = itemID;
    dropObj.count = (uint8)itemCount;
    pWorld->DropObject(pDropTile, dropObj, true);
}