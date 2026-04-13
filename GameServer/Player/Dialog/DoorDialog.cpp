#include "DoorDialog.h"

#include "Item/ItemInfoManager.h"
#include "Utils/DialogBuilder.h"
#include "Utils/StringUtils.h"
#include "World/TileInfo.h"
#include "../GamePlayer.h"
#include "../../World/WorldManager.h"

void DoorDialog::Request(GamePlayer* pPlayer, TileInfo* pTile)
{
    if(!pPlayer || !pTile || pPlayer->GetCurrentWorld() == 0) {
        return;
    }

    TileExtra_Door* pDoor = pTile->GetExtra<TileExtra_Door>();
    if(!pDoor) {
        return;
    }

    World* pWorld = GetWorldManager()->GetWorldByID(pPlayer->GetCurrentWorld());
    if(!pWorld || !pWorld->PlayerHasAccessOnTile(pPlayer, pTile)) {
        pPlayer->SendOnTalkBubble("`wI can't edit this door.``", true);
        return;
    }

    ItemInfo* pItem = GetItemInfoManager()->GetItemByID(pTile->GetDisplayedItem());
    if(!pItem || (pItem->type != ITEM_TYPE_DOOR && pItem->type != ITEM_TYPE_USER_DOOR && pItem->type != ITEM_TYPE_PORTAL && pItem->type != ITEM_TYPE_SUNGATE)) {
        return;
    }

    DialogBuilder db;
    db.SetDefaultColor('o')
        ->AddLabelWithIcon("`wEdit Door``", pItem->id, true)
        ->AddTextInput("DoorName", "Door Label", pDoor->name, 36)
        ->AddTextInput("DoorID", "Door ID (for local warp)", pDoor->id, 24)
        ->AddTextInput("DoorTarget", "Target: WORLD or WORLD:DOORID or :DOORID", pDoor->text, 48)
        ->EmbedData("tilex", pTile->GetPos().x)
        ->EmbedData("tiley", pTile->GetPos().y)
        ->EndDialog("door_edit", "Save", "Cancel");

    pPlayer->SendOnDialogRequest(db.Get());
}

void DoorDialog::Handle(GamePlayer* pPlayer, ParsedTextPacket<8>& packet)
{
    if(!pPlayer || pPlayer->GetCurrentWorld() == 0) {
        return;
    }

    auto pTileX = packet.Find(CompileTimeHashString("tilex"));
    auto pTileY = packet.Find(CompileTimeHashString("tiley"));
    if(!pTileX || !pTileY) {
        return;
    }

    int32 tileX = 0;
    int32 tileY = 0;
    if(ToInt(string(pTileX->value, pTileX->size), tileX) != TO_INT_SUCCESS || ToInt(string(pTileY->value, pTileY->size), tileY) != TO_INT_SUCCESS) {
        return;
    }

    World* pWorld = GetWorldManager()->GetWorldByID(pPlayer->GetCurrentWorld());
    if(!pWorld) {
        return;
    }

    TileInfo* pTile = pWorld->GetTileManager()->GetTile(tileX, tileY);
    if(!pTile) {
        return;
    }

    TileExtra_Door* pDoor = pTile->GetExtra<TileExtra_Door>();
    if(!pDoor || !pWorld->PlayerHasAccessOnTile(pPlayer, pTile)) {
        pPlayer->SendOnTalkBubble("`wI can't edit this door.``", true);
        return;
    }

    auto pDoorName = packet.Find(CompileTimeHashString("DoorName"));
    auto pDoorID = packet.Find(CompileTimeHashString("DoorID"));
    auto pDoorTarget = packet.Find(CompileTimeHashString("DoorTarget"));

    if(pDoorName) {
        pDoor->name.assign(pDoorName->value, pDoorName->size);
        RemoveExtraWhiteSpaces(pDoor->name);
        if(pDoor->name.size() > 36) {
            pDoor->name.resize(36);
        }
    }

    if(pDoorID) {
        pDoor->id.assign(pDoorID->value, pDoorID->size);
        RemoveExtraWhiteSpaces(pDoor->id);
        pDoor->id = ToUpper(pDoor->id);
        if(pDoor->id.size() > 24) {
            pDoor->id.resize(24);
        }
    }

    if(pDoorTarget) {
        pDoor->text.assign(pDoorTarget->value, pDoorTarget->size);
        RemoveExtraWhiteSpaces(pDoor->text);
        pDoor->text = ToUpper(pDoor->text);
        if(pDoor->text.size() > 48) {
            pDoor->text.resize(48);
        }
    }

    pWorld->SendTileUpdate(pTile);
}
