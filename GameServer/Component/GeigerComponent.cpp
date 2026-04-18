#include "GeigerComponent.h"

#include "../Player/GamePlayer.h"
#include "../World/World.h"
#include "../Server/MasterBroadway.h"
#include "Item/ItemInfo.h"
#include "World/TileInfo.h"
#include "Utils/DialogBuilder.h"

namespace {

constexpr const char* kGeigerChargeStat = "GEIGER_CHARGE";
constexpr const char* kGeigerNextScanStat = "GEIGER_NEXT_SCAN_MS";
constexpr const char* kGeigerXPStat = "GEIGER_XP";
constexpr const char* kGeigerLevelStat = "GEIGER_LVL";

bool HasWorkingCounter(GamePlayer* pPlayer)
{
    return pPlayer && pPlayer->GetInventory().GetCountOfItem(ITEM_ID_GEIGER_COUNTER) > 0;
}

bool HasDeadCounter(GamePlayer* pPlayer)
{
    return pPlayer && pPlayer->GetInventory().GetCountOfItem(ITEM_ID_DEAD_GEIGER_COUNTER) > 0;
}

bool IsRadioactiveDrop(uint16 itemID)
{
    return
        itemID == ITEM_ID_FOSSIL ||
        itemID == ITEM_ID_POLISHED_FOSSIL ||
        itemID == ITEM_ID_FOSSIL_ROCK ||
        itemID == ITEM_ID_DEEP_FOSSIL_ROCK ||
        itemID == ITEM_ID_ALIEN_FOSSIL_ROCK ||
        itemID == ITEM_ID_IGNEOUS_FOSSIL_ROCK;
}

bool IsRadioactiveBreakSource(uint16 itemID)
{
    return
        itemID == ITEM_ID_FOSSIL_ROCK ||
        itemID == ITEM_ID_FOSSIL_BOULDER ||
        itemID == ITEM_ID_DEEP_FOSSIL_ROCK ||
        itemID == ITEM_ID_FOSSILROID ||
        itemID == ITEM_ID_ALIEN_FOSSIL_ROCK ||
        itemID == ITEM_ID_IGNEOUS_FOSSIL_ROCK;
}

uint16 RollRadioactiveDrop()
{
    static const uint16 kDrops[] = {
        ITEM_ID_FOSSIL,
        ITEM_ID_FOSSIL,
        ITEM_ID_FOSSIL_ROCK,
        ITEM_ID_DEEP_FOSSIL_ROCK,
        ITEM_ID_ALIEN_FOSSIL_ROCK,
        ITEM_ID_IGNEOUS_FOSSIL_ROCK,
        ITEM_ID_POLISHED_FOSSIL
    };

    return kDrops[rand() % (sizeof(kDrops) / sizeof(kDrops[0]))];
}

}

void GeigerComponent::RequestChargerDialog(GamePlayer* pPlayer, TileInfo* pTile)
{
    if(!pPlayer || !pTile) {
        return;
    }

    const uint64 charge = pPlayer->GetCustomStatValue(kGeigerChargeStat);
    DialogBuilder db;
    db.SetDefaultColor('o')
        ->AddLabelWithIcon("`wGeiger Charger``", ITEM_ID_GEIGER_CHARGER, true)
        ->AddLabel("Current Geiger charge: `w" + ToString((uint32)std::min<uint64>(100, charge)) + "%``")
        ->AddLabel("Use this machine to restore your Geiger Counter.")
        ->EmbedData("tilex", pTile->GetPos().x)
        ->EmbedData("tiley", pTile->GetPos().y)
        ->EmbedData("TileItemID", pTile->GetDisplayedItem())
        ->AddButton("Charge", "`2Charge Counter``")
        ->EndDialog("GeigerChargerEdit", "", "Close")
        ->AddQuickExit();

    pPlayer->SendOnDialogRequest(db.Get());
}

bool GeigerComponent::TryChargeCounter(GamePlayer* pPlayer, World* pWorld, TileInfo* pTile)
{
    if(!pPlayer || !pWorld || !pTile || pTile->GetDisplayedItem() != ITEM_ID_GEIGER_CHARGER) {
        return false;
    }

    if(!HasWorkingCounter(pPlayer) && HasDeadCounter(pPlayer)) {
        pPlayer->ModifyInventoryItem(ITEM_ID_DEAD_GEIGER_COUNTER, -1);
        pPlayer->ModifyInventoryItem(ITEM_ID_GEIGER_COUNTER, 1);
    }

    if(!HasWorkingCounter(pPlayer)) {
        pPlayer->SendOnTalkBubble("`4You need a Geiger Counter first!", true);
        return false;
    }

    pPlayer->SetCustomStatValue(kGeigerChargeStat, 100);
    pPlayer->SendOnTalkBubble("`2Your Geiger Counter is fully charged!", true);
    pWorld->PlaySFXForEveryone("audio/powerup.wav", 0);
    return true;
}

void GeigerComponent::UpdatePlayerScan(GamePlayer* pPlayer, World* pWorld)
{
    if(!pPlayer || !pWorld || !pPlayer->HasState(PLAYER_STATE_IN_GAME) || !HasWorkingCounter(pPlayer)) {
        return;
    }

    const uint64 nowMS = Time::GetSystemTime();
    const uint64 nextScanMS = pPlayer->GetCustomStatValue(kGeigerNextScanStat);
    if(nextScanMS > nowMS) {
        return;
    }

    pPlayer->SetCustomStatValue(kGeigerNextScanStat, nowMS + 1250ull);

    uint64 charge = pPlayer->GetCustomStatValue(kGeigerChargeStat);
    if(charge == 0) {
        charge = 100;
    }

    if(charge > 0) {
        --charge;
    }

    pPlayer->SetCustomStatValue(kGeigerChargeStat, charge);
    if(charge == 0) {
        if(pPlayer->GetInventory().GetCountOfItem(ITEM_ID_GEIGER_COUNTER) > 0) {
            pPlayer->ModifyInventoryItem(ITEM_ID_GEIGER_COUNTER, -1);
            pPlayer->ModifyInventoryItem(ITEM_ID_DEAD_GEIGER_COUNTER, 1);
            pPlayer->SendOnConsoleMessage("`4Your Geiger Counter has run out of charge.");
        }
        return;
    }

    const Vector2Float playerPos = pPlayer->GetWorldPos();
    RectFloat searchRect(playerPos.x - 96.0f, playerPos.y - 96.0f, playerPos.x + 96.0f, playerPos.y + 96.0f);
    auto objects = pWorld->GetObjectManager()->GetObjectsInRect(searchRect);

    for(WorldObject* pObject : objects) {
        if(!pObject || !IsRadioactiveDrop(pObject->itemID)) {
            continue;
        }

        pWorld->RemoveObject(pObject->objectID);
        pPlayer->GetInventory().AddItem(pObject->itemID, 1, pPlayer);

        uint64 geigerXP = pPlayer->GetCustomStatValue(kGeigerXPStat) + 1;
        uint64 geigerLevel = pPlayer->GetCustomStatValue(kGeigerLevelStat);
        if(geigerLevel == 0) {
            geigerLevel = 1;
        }

        const uint64 needXP = 5 + (geigerLevel * 3);
        if(geigerXP >= needXP) {
            geigerXP = 0;
            ++geigerLevel;
            pPlayer->SetCustomStatValue(kGeigerLevelStat, geigerLevel);
            pPlayer->SendOnConsoleMessage("`2Your Geiger level increased to `w" + ToString((uint32)geigerLevel) + "``!");
        }

        pPlayer->SetCustomStatValue(kGeigerXPStat, geigerXP);
        pPlayer->PlaySFX("pickup.wav", 0);
        pPlayer->SendOnConsoleMessage("`2Your Geiger Counter detected something radioactive nearby!");
        return;
    }
}

void GeigerComponent::TrySpawnRadioactiveDrop(GamePlayer* pPlayer, World* pWorld, TileInfo* pTile, const ItemInfo* pBrokenItem)
{
    if(!pPlayer || !pWorld || !pTile || !pBrokenItem) {
        return;
    }

    if(!IsRadioactiveBreakSource(pBrokenItem->id)) {
        return;
    }

    if((rand() % 500) != 0) {
        return;
    }

    WorldObject radioactive;
    radioactive.itemID = RollRadioactiveDrop();
    radioactive.count = 1;
    radioactive.flags = OBJECT_FLAG_NO_STACK;
    pWorld->DropObject(pTile, radioactive, true);

    pPlayer->SendOnTalkBubble("`2Your Geiger Counter crackles nearby...", true);
}
