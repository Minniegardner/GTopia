#include "FossilComponent.h"

#include "../Player/GamePlayer.h"
#include "../World/World.h"
#include "World/TileInfo.h"
#include "Utils/DialogBuilder.h"

namespace {

constexpr const char* kFossilCounterStat = "FOSSIL_RANDOM_COUNTER";

int32 RollFossilCounter()
{
    return 4 + (rand() % 3);
}

}

bool FossilComponent::IsFossilRockItem(uint16 itemID)
{
    switch(itemID) {
        case ITEM_ID_FOSSIL_ROCK:
        case ITEM_ID_FOSSIL_BOULDER:
        case ITEM_ID_DEEP_FOSSIL_ROCK:
        case ITEM_ID_FOSSILROID:
        case ITEM_ID_ALIEN_FOSSIL_ROCK:
        case ITEM_ID_IGNEOUS_FOSSIL_ROCK:
            return true;
        default:
            return false;
    }
}

void FossilComponent::OnFossilRockPunched(GamePlayer* pPlayer, World* pWorld, TileInfo* pTile, const ItemInfo* pTileItem)
{
    if(!pPlayer || !pWorld || !pTile || !pTileItem || !IsFossilRockItem(pTileItem->id)) {
        return;
    }

    int32 randomCounter = (int32)pPlayer->GetCustomStatValue(kFossilCounterStat);
    if(randomCounter <= 0) {
        randomCounter = RollFossilCounter();
    }

    --randomCounter;
    pPlayer->SetCustomStatValue(kFossilCounterStat, randomCounter > 0 ? (uint64)randomCounter : 0);
    pPlayer->SendOnTalkBubble("`wI smashed a Fossil!!", true);

    if(randomCounter > 0) {
        return;
    }

    WorldObject obj;
    obj.itemID = ITEM_ID_FOSSIL;
    obj.count = 1;
    obj.flags = OBJECT_FLAG_NO_STACK;
    pWorld->DropObject(pTile, obj, true);

    pPlayer->SetCustomStatValue(kFossilCounterStat, (uint64)RollFossilCounter());
    pPlayer->SendOnTalkBubble("`2I unearthed a Fossil!!", true);
}

void FossilComponent::RequestPrepDialog(GamePlayer* pPlayer, TileInfo* pTile)
{
    if(!pPlayer || !pTile) {
        return;
    }

    DialogBuilder db;
    db.SetDefaultColor('o')
        ->AddLabelWithIcon("`wFossil Prep Station``", ITEM_ID_FOSSIL_PREP_STATION, true)
        ->AddLabel("Brush an untouched fossil dropped in this tile to polish it.")
        ->AddLabel("You currently have `w" + ToString(pPlayer->GetInventory().GetCountOfItem(ITEM_ID_FOSSIL_BRUSH)) + "`` Fossil Brush(es).")
        ->EmbedData("tilex", pTile->GetPos().x)
        ->EmbedData("tiley", pTile->GetPos().y)
        ->EmbedData("TileItemID", pTile->GetDisplayedItem())
        ->AddButton("Prep", "`2Prep Fossil``")
        ->EndDialog("FossilPrepEdit", "", "Close")
        ->AddQuickExit();

    pPlayer->SendOnDialogRequest(db.Get());
}

bool FossilComponent::TryHandlePrepAction(GamePlayer* pPlayer, World* pWorld, TileInfo* pTile)
{
    if(!pPlayer || !pWorld || !pTile) {
        return false;
    }

    if(pTile->GetDisplayedItem() != ITEM_ID_FOSSIL_PREP_STATION) {
        pPlayer->SendOnTalkBubble("`4That prep station is gone.", true);
        return false;
    }

    if(pPlayer->GetInventory().GetCountOfItem(ITEM_ID_FOSSIL_BRUSH) == 0) {
        pPlayer->SendOnTalkBubble("`4You need a Fossil Brush first!", true);
        return false;
    }

    const Vector2Int tilePos = pTile->GetPos();
    RectFloat searchRect((float)tilePos.x * 32.0f, (float)tilePos.y * 32.0f, (float)(tilePos.x + 1) * 32.0f, (float)(tilePos.y + 1) * 32.0f);
    auto objects = pWorld->GetObjectManager()->GetObjectsInRectByItemID(searchRect, ITEM_ID_FOSSIL);

    WorldObject* pFossil = nullptr;
    for(WorldObject* pObject : objects) {
        if(pObject && pObject->itemID == ITEM_ID_FOSSIL && pObject->count == 1) {
            pFossil = pObject;
            break;
        }
    }

    if(!pFossil) {
        pPlayer->SendOnTalkBubble("`wYou can only brush Fossils that have never been picked up!", true);
        return false;
    }

    WorldObject polished;
    polished.itemID = ITEM_ID_POLISHED_FOSSIL;
    polished.count = 1;
    polished.pos = pFossil->pos;

    pWorld->RemoveObject(pFossil->objectID);
    pWorld->DropObject(polished);

    pPlayer->ModifyInventoryItem(ITEM_ID_FOSSIL_BRUSH, -1);
    pPlayer->SendOnTalkBubble("`wYou polished the `2Fossil`w, using up 1 `2Fossil Brush`w.", true);

    const uint8 gemsToDrop = (uint8)(rand() % 13);
    if(gemsToDrop > 0) {
        pPlayer->AddGems(gemsToDrop);
        pPlayer->SendOnSetBux();
    }

    pWorld->SendParticleEffectToAll((tilePos.x * 32.0f) + 16.0f, (tilePos.y * 32.0f) + 16.0f, 44, 1.0f, 0);
    return true;
}
