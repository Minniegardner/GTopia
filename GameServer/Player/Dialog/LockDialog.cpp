#include "LockDialog.h"
#include "../GamePlayer.h"
#include "Utils/DialogBuilder.h"
#include "Item/ItemInfoManager.h"
#include "../../World/WorldManager.h"

void LockDialog::Request(GamePlayer* pPlayer, TileInfo* pTile)
{
    if(!pPlayer || !pTile || pPlayer->GetCurrentWorld() == 0) {
        return;
    }

    TileExtra_Lock* pTileExtra = pTile->GetExtra<TileExtra_Lock>();
    if(!pTileExtra) {
        return;
    }

    World* pWorld = GetWorldManager()->GetWorldByID(pPlayer->GetCurrentWorld());
    if(!pWorld) {
        return;
    }

    if(!pWorld->PlayerHasAccessOnTile(pPlayer, pTile)) {
        return;
    }

    ItemInfo* pItem = GetItemInfoManager()->GetItemByID(pTile->GetDisplayedItem());
    if(!pItem || pItem->type != ITEM_TYPE_LOCK) {
        return;
    }

    DialogBuilder db;
    db.SetDefaultColor('o')
    ->AddLabelWithIcon("`wEdit " + pItem->name, pItem->id, true);

    if(!IsWorldLock(pItem->id)) {
        bool ignoreAir = pTileExtra->HasFlag(TILE_EXTRA_LOCK_FLAG_IGNORE_AIR);
        db.AddCheckBox("checkbox_ignore", "Ignore empty air", ignoreAir)
        ->AddButton("recalcLock", "`wRe-apply lock``");
    }

    db.EndDialog("lock_edit", "OK", "Cancel");
    pPlayer->SendOnDialogRequest(db.Get());
}