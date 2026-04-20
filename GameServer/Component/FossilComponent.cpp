#include "FossilComponent.h"

#include "../Player/GamePlayer.h"
#include "../World/World.h"
#include "Item/ItemInfo.h"
#include "World/TileInfo.h"
#include "Utils/DialogBuilder.h"

#include <array>

namespace {

bool TryMapRockToFossil(uint16 rockItemID, uint16& outFossilItemID)
{
    switch(rockItemID) {
        case ITEM_ID_ROCK: outFossilItemID = ITEM_ID_FOSSIL_ROCK; return true;
        case ITEM_ID_BOULDER: outFossilItemID = ITEM_ID_FOSSIL_BOULDER; return true;
        case ITEM_ID_IGNEOUS_ROCK: outFossilItemID = ITEM_ID_IGNEOUS_FOSSIL_ROCK; return true;
        case ITEM_ID_ASTEROID: outFossilItemID = ITEM_ID_ALIEN_FOSSIL_ROCK; return true;
        case ITEM_ID_MARS_ROCK: outFossilItemID = ITEM_ID_FOSSILROID; return true;
        case ITEM_ID_DEEP_ROCK: outFossilItemID = ITEM_ID_DEEP_FOSSIL_ROCK; return true;
        default: return false;
    }
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

void FossilComponent::TrySpawnWorldFossil(World* pWorld)
{
    if(!pWorld || pWorld->GetPlayerCount() == 0) {
        return;
    }

    if((rand() % 2) != 0) {
        return;
    }

    const Vector2Int size = pWorld->GetTileManager()->GetSize();
    if(size.x <= 0 || size.y <= 0) {
        return;
    }

    std::vector<TileInfo*> rockTiles;
    rockTiles.reserve(128);

    for(int32 y = 0; y < size.y; ++y) {
        for(int32 x = 0; x < size.x; ++x) {
            TileInfo* pTile = pWorld->GetTileManager()->GetTile(x, y);
            if(!pTile) {
                continue;
            }

            uint16 fossilItemID = ITEM_ID_BLANK;
            if(TryMapRockToFossil(pTile->GetDisplayedItem(), fossilItemID)) {
                rockTiles.push_back(pTile);
            }
        }
    }

    if(rockTiles.empty()) {
        return;
    }

    TileInfo* pSelected = rockTiles[rand() % rockTiles.size()];
    if(!pSelected) {
        return;
    }

    uint16 fossilItemID = ITEM_ID_BLANK;
    if(!TryMapRockToFossil(pSelected->GetDisplayedItem(), fossilItemID)) {
        return;
    }

    pSelected->SetFG(fossilItemID, pWorld->GetTileManager());
    pWorld->SendTileUpdate(pSelected);
}

void FossilComponent::OnFossilRockPunched(GamePlayer* pPlayer, World* pWorld, TileInfo* pTile, const ItemInfo* pTileItem)
{
    (void)pPlayer;
    (void)pWorld;
    (void)pTile;
    (void)pTileItem;
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
