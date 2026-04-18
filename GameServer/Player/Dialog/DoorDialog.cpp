#include "DoorDialog.h"

#include "Item/ItemInfoManager.h"
#include "Utils/DialogBuilder.h"
#include "Utils/StringUtils.h"
#include "World/TileInfo.h"
#include "../GamePlayer.h"
#include "../../World/WorldManager.h"

namespace {

bool ParseCheckboxValue(ParsedTextPacket<8>& packet, const string& key, bool defaultValue)
{
    auto pField = packet.Find(HashString(key));
    if(!pField) {
        return defaultValue;
    }

    int32 value = 0;
    if(ToInt(string(pField->value, pField->size), value) != TO_INT_SUCCESS) {
        return defaultValue;
    }

    return value == 1;
}

}

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
    if(!pItem || (pItem->type != ITEM_TYPE_DOOR && pItem->type != ITEM_TYPE_USER_DOOR && pItem->type != ITEM_TYPE_PORTAL && pItem->type != ITEM_TYPE_SUNGATE && pItem->type != ITEM_TYPE_FRIENDS_ENTRANCE)) {
        return;
    }

    DialogBuilder db;
    db.SetDefaultColor('o')
        ->AddLabelWithIcon("`wEdit " + pItem->name + "``", pItem->id, true)
        ->AddSpacer()
        ->AddTextInput("DoorLabel", "Door Label", pDoor->name, 100)
        ->AddTextInput("DoorTargetID", "Destination Door ID", pDoor->text, 24)
        ->AddTextBox("Enter a Destination in this format: `2WORLDNAME:ID``")
        ->AddTextBox("Leave `2WORLDNAME`` blank (:ID) to go to the door with `2ID`` in the `2Current World``")
        ->AddTextInput("DoorID", "This Door's ID", pDoor->id, 24)
        ->AddTextBox("Set a unique `2ID`` to target this door as a Destination from another!")
        ->AddCheckBox("IsPublic", "Is open to public", pTile->HasFlag(TILE_FLAG_IS_OPEN_TO_PUBLIC))
        ->EmbedData("tilex", pTile->GetPos().x)
        ->EmbedData("tiley", pTile->GetPos().y)
        ->EndDialog("door_edit", "OK", "Cancel");

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

    auto pDoorName = packet.Find(CompileTimeHashString("DoorLabel"));
    if(!pDoorName) {
        pDoorName = packet.Find(CompileTimeHashString("DoorName"));
    }

    auto pDoorID = packet.Find(CompileTimeHashString("DoorID"));
    auto pDoorTarget = packet.Find(CompileTimeHashString("DoorTargetID"));
    if(!pDoorTarget) {
        pDoorTarget = packet.Find(CompileTimeHashString("DoorTarget"));
    }

    if(pDoorName) {
        pDoor->name.assign(pDoorName->value, pDoorName->size);
        RemoveExtraWhiteSpaces(pDoor->name);
        if(pDoor->name.size() > 100) {
            pDoor->name.resize(100);
        }
    }

    if(pDoorID) {
        pDoor->id.assign(pDoorID->value, pDoorID->size);
        RemoveAllSpaces(pDoor->id);
        pDoor->id = ToUpper(pDoor->id);
        if(pDoor->id.size() > 24) {
            pDoor->id.resize(24);
        }
    }

    if(pDoorTarget) {
        pDoor->text.assign(pDoorTarget->value, pDoorTarget->size);
        RemoveAllSpaces(pDoor->text);
        pDoor->text = ToUpper(pDoor->text);
        if(pDoor->text.size() > 24) {
            pDoor->text.resize(24);
        }
    }

    bool isPublic = ParseCheckboxValue(packet, "IsPublic", pTile->HasFlag(TILE_FLAG_IS_OPEN_TO_PUBLIC));
    if(isPublic) {
        pTile->SetFlag(TILE_FLAG_IS_OPEN_TO_PUBLIC);
    }
    else {
        pTile->RemoveFlag(TILE_FLAG_IS_OPEN_TO_PUBLIC);
    }

    pWorld->SendTileUpdate(pTile);
}
