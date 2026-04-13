#include "PlayerDialog.h"
#include "../GamePlayer.h"
#include "Item/ItemInfoManager.h"
#include "World/TileInfo.h"

#include "SignDialog.h"
#include "LockDialog.h"
#include "DoorDialog.h"
#include "MachineDialog.h"

void PlayerDialog::Handle(GamePlayer* pPlayer, TileInfo* pTile)
{
    if(!pTile) {
        return;
    }

    ItemInfo* pItem = GetItemInfoManager()->GetItemByID(pTile->GetDisplayedItem());

    if(pItem->type == ITEM_TYPE_SIGN) {
        SignDialog::Request(pPlayer, pTile);
        return;
    }

    if(pItem->type == ITEM_TYPE_LOCK) {
        LockDialog::Request(pPlayer, pTile);
        return;
    }

    if(pItem->type == ITEM_TYPE_DOOR || pItem->type == ITEM_TYPE_USER_DOOR || pItem->type == ITEM_TYPE_PORTAL || pItem->type == ITEM_TYPE_SUNGATE) {
        DoorDialog::Request(pPlayer, pTile);
        return;
    }

    if(pItem->type == ITEM_TYPE_VENDING || pItem->id == ITEM_ID_MAGPLANT_5000 || pItem->id == ITEM_ID_UNSTABLE_TESSERACT || pItem->id == ITEM_ID_GAIAS_BEACON) {
        MachineDialog::Request(pPlayer, pTile);
        return;
    }
}
