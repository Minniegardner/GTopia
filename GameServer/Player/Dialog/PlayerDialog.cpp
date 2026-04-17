#include "PlayerDialog.h"
#include "../GamePlayer.h"
#include "Item/ItemInfoManager.h"
#include "World/TileInfo.h"
#include "Utils/DialogBuilder.h"
#include "../../Server/MasterBroadway.h"

#include "SignDialog.h"
#include "LockDialog.h"
#include "DoorDialog.h"
#include "MachineDialog.h"
#include "ChemsynthDialog.h"

namespace {

constexpr uint16 kTelephoneItemID = 3898;

void ShowTelephoneDialog(GamePlayer* pPlayer, TileInfo* pTile)
{
    if(!pPlayer || !pTile) {
        return;
    }

    DialogBuilder db;
    db.SetDefaultColor('o')
        ->AddLabelWithIcon("`wTelephone``", kTelephoneItemID, true, true)
        ->AddLabel("Dial a number to call somebody in Growtopia. Phone numbers have 5 digits, like 12345.")
        ->AddTextInput("Number", "Phone #", "", 5)
        ->EmbedData("tilex", pTile->GetPos().x)
        ->EmbedData("tiley", pTile->GetPos().y)
        ->EmbedData("TileItemID", pTile->GetDisplayedItem())
        ->EndDialog("TelephoneEdit", "Dial", "Hang Up");

    pPlayer->SendOnDialogRequest(db.Get());
}

}

void PlayerDialog::Handle(GamePlayer* pPlayer, TileInfo* pTile)
{
    if(!pTile) {
        return;
    }

    ItemInfo* pItem = GetItemInfoManager()->GetItemByID(pTile->GetDisplayedItem());
    if(!pItem) {
        return;
    }

    if(pItem->type == ITEM_TYPE_SIGN) {
        SignDialog::Request(pPlayer, pTile);
        return;
    }

    if(pItem->id == kTelephoneItemID) {
        ShowTelephoneDialog(pPlayer, pTile);
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

    if(
        pItem->type == ITEM_TYPE_CHEMSYNTH || pItem->type == ITEM_TYPE_CHEMTANK ||
        pItem->id == ITEM_ID_CHEMSYNTH_PROCESSOR || pItem->id == ITEM_ID_CHEMSYNTH_TANK
    ) {
        ChemsynthDialog::Request(pPlayer, pTile);
        return;
    }
}
