#include "PathMarkerDialog.h"

#include "../GamePlayer.h"
#include "Item/ItemInfoManager.h"
#include "Utils/DialogBuilder.h"
#include "../../World/WorldManager.h"

namespace {

bool IsPathMarkerItem(uint16 itemID)
{
    return itemID == ITEM_ID_OBJECTIVE_MARKER || itemID == ITEM_ID_PATH_MARKER || itemID == ITEM_ID_CARNIVAL_LANDING;
}

}

void PathMarkerDialog::Request(GamePlayer* pPlayer, TileInfo* pTile)
{
    if(!pPlayer || !pTile || pPlayer->GetCurrentWorld() == 0) {
        return;
    }

    TileExtra_Sign* pTileExtra = pTile->GetExtra<TileExtra_Sign>();
    if(!pTileExtra) {
        return;
    }

    ItemInfo* pItem = GetItemInfoManager()->GetItemByID(pTile->GetDisplayedItem());
    if(!pItem || pItem->type != ITEM_TYPE_SIGN || !IsPathMarkerItem(pItem->id)) {
        return;
    }

    World* pWorld = GetWorldManager()->GetWorldByID(pPlayer->GetCurrentWorld());
    if(!pWorld || !pWorld->PlayerHasAccessOnTile(pPlayer, pTile)) {
        pPlayer->SendOnTalkBubble("`wI can't edit this sign.``", true);
        return;
    }

    DialogBuilder db;
    db.SetDefaultColor('o')
        ->AddLabelWithIcon("`wEdit " + pItem->name, pItem->id, true)
        ->AddSpacer()
        ->AddLabel("Enter an ID. You can use this as a destination for Doors.")
        ->AddTextInput("Text", "", pTileExtra->text, 128)
        ->EmbedData("tilex", pTile->GetPos().x)
        ->EmbedData("tiley", pTile->GetPos().y)
        ->AddQuickExit()
        ->EndDialog("SignEdit", "OK", "Cancel");

    pPlayer->SendOnDialogRequest(db.Get());
}

void PathMarkerDialog::Handle(GamePlayer* pPlayer, const string& text, int32 tileX, int32 tileY)
{
    if(!pPlayer || pPlayer->GetCurrentWorld() == 0) {
        return;
    }

    string written = text;
    RemoveExtraWhiteSpaces(written);

    if(written.size() > 128) {
        pPlayer->SendOnTalkBubble("That text is too long!", false);
        return;
    }

    World* pWorld = GetWorldManager()->GetWorldByID(pPlayer->GetCurrentWorld());
    if(!pWorld) {
        return;
    }

    TileInfo* pTile = pWorld->GetTileManager()->GetTile(tileX, tileY);
    if(!pTile) {
        pPlayer->SendOnTalkBubble("Huh? The sign is gone!", false);
        return;
    }

    ItemInfo* pItem = GetItemInfoManager()->GetItemByID(pTile->GetDisplayedItem());
    if(!pItem || pItem->type != ITEM_TYPE_SIGN || !IsPathMarkerItem(pItem->id)) {
        pPlayer->SendOnTalkBubble("Huh? The sign is gone!", false);
        return;
    }

    if(!pWorld->PlayerHasAccessOnTile(pPlayer, pTile)) {
        pPlayer->SendOnTalkBubble("`wI can't edit this sign.``", true);
        return;
    }

    TileExtra_Sign* pTileExtra = pTile->GetExtra<TileExtra_Sign>();
    if(!pTileExtra) {
        pPlayer->SendOnTalkBubble("Huh? The sign is gone!", false);
        return;
    }

    pTileExtra->text = written;
    pWorld->SendTileUpdate(tileX, tileY);
}