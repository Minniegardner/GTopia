#include "LockDialog.h"
#include "../GamePlayer.h"
#include "Utils/DialogBuilder.h"
#include "Item/ItemInfoManager.h"
#include "../../World/WorldManager.h"
#include "../../Server/GameServer.h"
#include "../../../Source/Math/Math.h"
#include <algorithm>

namespace {

string GetNameByUserID(int32 userID)
{
    GamePlayer* pPlayer = GetGameServer()->GetPlayerByUserID((uint32)userID);
    if(pPlayer) {
        return pPlayer->GetDisplayName();
    }

    return "DeletedUser";
}

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
        pPlayer->SendOnTalkBubble("`wI'm `4unable`w to pick the lock!", true);
        return;
    }

    ItemInfo* pItem = GetItemInfoManager()->GetItemByID(pTile->GetDisplayedItem());
    if(!pItem || pItem->type != ITEM_TYPE_LOCK) {
        return;
    }

    if(pTileExtra->ownerID != pPlayer->GetUserID()) {
        DialogBuilder db;
        db.SetDefaultColor('o')
        ->AddTextBox("This lock is owned by ``" + GetNameByUserID(pTileExtra->ownerID) + "`o, but I have access on it.")
        ->EmbedData("tilex", pTile->GetPos().x)
        ->EmbedData("tiley", pTile->GetPos().y)
        ->EndDialog("lock_remove_self", "Remove My Access", "Cancel");

        pPlayer->SendOnDialogRequest(db.Get());
        return;
    }

    DialogBuilder db;
    db.SetDefaultColor('o')
    ->AddLabelWithIcon("`wEdit " + pItem->name + "``", pItem->id, true)
    ->AddTextBox("`wAccess List:")
    ->AddSpacer()
    ->EmbedData("tilex", pTile->GetPos().x)
    ->EmbedData("tiley", pTile->GetPos().y);

    bool hasAccess = false;
    for(int32 userID : pTileExtra->accessList) {
        if(userID <= 0) {
            continue;
        }

        if(!pTileExtra->HasAccess(userID)) {
            continue;
        }

        hasAccess = true;
        db.AddCheckBox("LockAdmin#" + ToString(userID), GetNameByUserID(userID), true);
    }

    if(!hasAccess) {
        db.AddTextBox("Currently, you're the only one with access.``");
    }

    db.AddSpacer()
    ->AddPlayerPicker("NewAdminNetID", "Add");

    if(IsWorldLock(pItem->id)) {
        db.AddCheckBox("WorldPublic", "Allow anyone to build and break", pTile->HasFlag(TILE_FLAG_IS_OPEN_TO_PUBLIC))
        ->AddTextInput("EntryLevel", "Minimum entry level", ToString(std::max(1, pTileExtra->minEntryLevel)), 3);
    }
    else {
        db.AddCheckBox("AreaPublic", "Allow anyone to build and break", pTile->HasFlag(TILE_FLAG_IS_OPEN_TO_PUBLIC));

        if(pItem->id == ITEM_ID_BUILDERS_LOCK) {
            db.AddSpacer()
            ->AddTextBox("This lock allows Building or Breaking.")
            ->AddTextBox("Leaving this unchecked only allows Breaking.")
            ->AddCheckBox("BuildingOnly", "Only Allow Building!", pTileExtra->HasFlag(TILE_EXTRA_LOCK_FLAG_BUILDING_ONLY))
            ->AddCheckBox("RestrictAdmins", "Admins Are Limited", pTileExtra->HasFlag(TILE_EXTRA_LOCK_FLAG_RESTRICT_ADMINS));
        }

        db.AddCheckBox("IgnoreEmptyAir", "Ignore empty air", pTileExtra->HasFlag(TILE_EXTRA_LOCK_FLAG_IGNORE_AIR))
        ->AddCheckBox("AirOnly", "Lock air only", pTileExtra->HasFlag(TILE_EXTRA_LOCK_FLAG_AIR_ONLY))
        ->AddButton("Reapply", "Re-apply lock");
    }

    db.EndDialog("lock_edit", "Okay", "Cancel");
    pPlayer->SendOnDialogRequest(db.Get());
}

void LockDialog::Handle(GamePlayer* pPlayer, ParsedTextPacket<8>& packet)
{
    if(pPlayer->GetCurrentWorld() == 0) {
        return;
    }

    auto pTileX = packet.Find(CompileTimeHashString("tilex"));
    auto pTileY = packet.Find(CompileTimeHashString("tiley"));

    if(!pTileX || !pTileY) {
        return;
    }

    int32 tileX = 0;
    if(ToInt(string(pTileX->value, pTileX->size), tileX) != TO_INT_SUCCESS) {
        return;
    }

    int32 tileY = 0;
    if(ToInt(string(pTileY->value, pTileY->size), tileY) != TO_INT_SUCCESS) {
        return;
    }

    World* pWorld = GetWorldManager()->GetWorldByID(pPlayer->GetCurrentWorld());
    if(!pWorld) {
        return;
    }

    TileInfo* pTile = pWorld->GetTileManager()->GetTile(tileX, tileY);
    if(!pTile) {
        pPlayer->SendOnTalkBubble("I was looking at a lock but now it's gone.  Magic is real!", false);
        return;
    }

    ItemInfo* pItem = GetItemInfoManager()->GetItemByID(pTile->GetFG());
    if(!pItem || pItem->type != ITEM_TYPE_LOCK) {
        return;
    }

    TileExtra_Lock* pTileExtra = pTile->GetExtra<TileExtra_Lock>();
    if(!pTileExtra) {
        return;
    }

    auto pDialogName = packet.Find(CompileTimeHashString("dialog_name"));
    string dialogName = pDialogName ? string(pDialogName->value, pDialogName->size) : "";

    if(dialogName == "lock_remove_self") {
        auto pButtonClicked = packet.Find(CompileTimeHashString("buttonClicked"));
        if(!pButtonClicked || string(pButtonClicked->value, pButtonClicked->size) != "Remove My Access") {
            return;
        }

        if(!pTileExtra->HasAccess(pPlayer->GetUserID()) || pTileExtra->ownerID == pPlayer->GetUserID()) {
            return;
        }

        pTileExtra->accessList.erase(
            std::remove(pTileExtra->accessList.begin(), pTileExtra->accessList.end(), (int32)pPlayer->GetUserID()),
            pTileExtra->accessList.end()
        );

        pPlayer->SendOnTalkBubble("`4Access removed.", true);
        pWorld->SendTileUpdate(pTile);
        return;
    }

    if(!pTileExtra->HasAccess(pPlayer->GetUserID())) {
        pPlayer->SendOnTalkBubble("`wI'm `4unable`w to pick the lock!", true);
        return;
    }

    if(pTileExtra->ownerID != pPlayer->GetUserID()) {
        pPlayer->SendOnTalkBubble("`wI'm `4unable`w to pick the lock!", true);
        return;
    }

    auto pAdminNetID = packet.Find(HashString("NewAdminNetID"));
    if(pAdminNetID) {
        int32 netID = 0;
        if(ToInt(string(pAdminNetID->value, pAdminNetID->size), netID) == TO_INT_SUCCESS && netID >= 0) {
            GamePlayer* pNewAdmin = GetGameServer()->GetPlayerByNetID((uint32)netID);
            if(pNewAdmin && pNewAdmin->GetCurrentWorld() == pPlayer->GetCurrentWorld() &&
                !pTileExtra->HasAccess(pNewAdmin->GetUserID()) && pNewAdmin->GetUserID() != pTileExtra->ownerID)
            {
                pTileExtra->accessList.push_back((int32)pNewAdmin->GetUserID());
            }
        }
    }

    std::vector<int32> removedAdmins;
    for(int32 userID : pTileExtra->accessList) {
        if(userID <= 0) {
            continue;
        }

        const string key = "LockAdmin#" + ToString(userID);
        if(!ParseCheckboxValue(packet, key, true)) {
            removedAdmins.push_back(userID);
        }
    }

    for(int32 userID : removedAdmins) {
        pTileExtra->accessList.erase(
            std::remove(pTileExtra->accessList.begin(), pTileExtra->accessList.end(), userID),
            pTileExtra->accessList.end()
        );
    }

    if(IsWorldLock(pItem->id)) {
        bool worldPublic = ParseCheckboxValue(packet, "WorldPublic", pTile->HasFlag(TILE_FLAG_IS_OPEN_TO_PUBLIC));
        if(worldPublic) {
            pTile->SetFlag(TILE_FLAG_IS_OPEN_TO_PUBLIC);
        }
        else {
            pTile->RemoveFlag(TILE_FLAG_IS_OPEN_TO_PUBLIC);
        }

        auto pEntryLevel = packet.Find(HashString("EntryLevel"));
        if(pEntryLevel) {
            int32 minEntryLevel = pTileExtra->minEntryLevel;
            if(ToInt(string(pEntryLevel->value, pEntryLevel->size), minEntryLevel) == TO_INT_SUCCESS) {
                pTileExtra->minEntryLevel = Clamp(minEntryLevel, 1, 999);
            }
        }
    }
    else {
        bool areaPublic = ParseCheckboxValue(packet, "AreaPublic", pTile->HasFlag(TILE_FLAG_IS_OPEN_TO_PUBLIC));
        if(areaPublic) {
            pTile->SetFlag(TILE_FLAG_IS_OPEN_TO_PUBLIC);
        }
        else {
            pTile->RemoveFlag(TILE_FLAG_IS_OPEN_TO_PUBLIC);
        }

        bool ignoreAir = ParseCheckboxValue(packet, "IgnoreEmptyAir", pTileExtra->HasFlag(TILE_EXTRA_LOCK_FLAG_IGNORE_AIR));
        bool airOnly = ParseCheckboxValue(packet, "AirOnly", pTileExtra->HasFlag(TILE_EXTRA_LOCK_FLAG_AIR_ONLY));
        bool buildingOnly = ParseCheckboxValue(packet, "BuildingOnly", pTileExtra->HasFlag(TILE_EXTRA_LOCK_FLAG_BUILDING_ONLY));
        bool restrictAdmins = ParseCheckboxValue(packet, "RestrictAdmins", pTileExtra->HasFlag(TILE_EXTRA_LOCK_FLAG_RESTRICT_ADMINS));

        if(ignoreAir) pTileExtra->SetFlag(TILE_EXTRA_LOCK_FLAG_IGNORE_AIR);
        else pTileExtra->RemoveFlag(TILE_EXTRA_LOCK_FLAG_IGNORE_AIR);

        if(airOnly) pTileExtra->SetFlag(TILE_EXTRA_LOCK_FLAG_AIR_ONLY);
        else pTileExtra->RemoveFlag(TILE_EXTRA_LOCK_FLAG_AIR_ONLY);

        if(buildingOnly) pTileExtra->SetFlag(TILE_EXTRA_LOCK_FLAG_BUILDING_ONLY);
        else pTileExtra->RemoveFlag(TILE_EXTRA_LOCK_FLAG_BUILDING_ONLY);

        if(restrictAdmins) pTileExtra->SetFlag(TILE_EXTRA_LOCK_FLAG_RESTRICT_ADMINS);
        else pTileExtra->RemoveFlag(TILE_EXTRA_LOCK_FLAG_RESTRICT_ADMINS);

        auto pButtonClicked = packet.Find(CompileTimeHashString("buttonClicked"));
        if(pButtonClicked && string(pButtonClicked->value, pButtonClicked->size) == "Reapply") {
            std::vector<TileInfo*> lockedTiles;
            bool lockSuccsess = pWorld->GetTileManager()->ApplyLockTiles(
                pTile,
                GetMaxTilesToLock(pItem->id),
                pTileExtra->HasFlag(TILE_EXTRA_LOCK_FLAG_IGNORE_AIR),
                lockedTiles
            );

            if(!lockSuccsess) {
                pPlayer->SendOnTalkBubble("Something went wrong, unable to re-calc lock", true);
                return;
            }

            pWorld->SendLockPacketToAll(pPlayer->GetUserID(), pItem->id, lockedTiles, pTile);
        }
    }

    pWorld->SendTileUpdate(pTile);
    Request(pPlayer, pTile);
}

