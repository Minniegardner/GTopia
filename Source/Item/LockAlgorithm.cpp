#include "LockAlgorithm.h"
#include "../World/WorldTileManager.h"
#include "../Item/ItemInfoManager.h"
#include <algorithm>

namespace {

bool IsLockNeighbour(WorldTileManager* pWorld, TileInfo* pCurrentTile, TileInfo* pLockTile, bool ignoreEmptyAir, bool airOnly)
{
    if(!pWorld || !pCurrentTile || !pLockTile || pCurrentTile == pLockTile) {
        return false;
    }

    const ItemInfo* pItem = GetItemInfoManager()->GetItemByID(pCurrentTile->GetDisplayedItem());
    if(!pItem) {
        return false;
    }

    if(airOnly && pItem->id != ITEM_ID_BLANK) {
        return false;
    }

    if(pCurrentTile->HasFlag(TILE_FLAG_HAS_PARENT) || pCurrentTile->GetParent() != 0) {
        return false;
    }

    if(pItem->id == ITEM_ID_MAIN_DOOR || pItem->type == ITEM_TYPE_BEDROCK || pItem->type == ITEM_TYPE_LOCK) {
        return false;
    }

    if(ignoreEmptyAir && pItem->id == ITEM_ID_BLANK) {
        return false;
    }

    const Vector2Int lockPos = pLockTile->GetPos();
    const Vector2Int currentPos = pCurrentTile->GetPos();
    const uint32 parentIndex = lockPos.x + pWorld->GetSize().x * lockPos.y;

    const int32 neighbors[4][2] = {
        {1, 0},
        {0, 1},
        {-1, 0},
        {0, -1}
    };

    for(int8 i = 0; i < 4; ++i) {
        TileInfo* pNeighbor = pWorld->GetTile(currentPos.x + neighbors[i][0], currentPos.y + neighbors[i][1]);
        if(!pNeighbor) {
            continue;
        }

        if(pNeighbor->GetParent() == parentIndex || pNeighbor == pLockTile ||
            (currentPos.x + neighbors[i][0] == lockPos.x && currentPos.y + neighbors[i][1] == lockPos.y))
        {
            return true;
        }
    }

    return false;
}

}

bool IsWorldLock(uint16 itemID)
{
    switch(itemID) {
        case ITEM_ID_WORLD_LOCK:
        case ITEM_ID_DIAMOND_LOCK:
        case ITEM_ID_HARMONIC_LOCK:
        case ITEM_ID_ROBOTIC_LOCK:
        case ITEM_ID_BLUE_GEM_LOCK:
            return true;

        default:
            return false;
    }
}

bool IsAreaLock(uint16 itemID)
{
    switch(itemID) {
        case ITEM_ID_SMALL_LOCK:
        case ITEM_ID_BIG_LOCK:
        case ITEM_ID_HUGE_LOCK:
        case ITEM_ID_BUILDERS_LOCK:
            return true;

        default:
            return false;
    }
}

uint16 GetMaxTilesToLock(uint16 itemID)
{
    switch(itemID) {
        case ITEM_ID_SMALL_LOCK:
            return 10;

        case ITEM_ID_BIG_LOCK:
            return 48;

        case ITEM_ID_HUGE_LOCK:
        case ITEM_ID_BUILDERS_LOCK:
            return 200;

        default:
            return 0;
    }
}

namespace LockAlgorithm {

bool ApplyLockTiles(WorldTileManager* pWorld, TileInfo* pLockTile, int32 tileSizeToLock, bool ignoreEmpty, std::vector<TileInfo*>& outTiles, bool airOnly)
{
    outTiles.clear();

    if(!pWorld || !pLockTile || tileSizeToLock > (pWorld->GetSize().x * pWorld->GetSize().y) || tileSizeToLock == 0) {
        return false;
    }

    if(ignoreEmpty && airOnly) {
        airOnly = false;
    }

    pWorld->RemoveTileParentsLockedBy(pLockTile);

    const Vector2Int lockPos = pLockTile->GetPos();
    std::vector<TileInfo*> lockedTiles;
    const uint32 maxRadius = Max(pWorld->GetSize().x, pWorld->GetSize().y);
    uint32 radius = 1;
    uint32 lockedCount = 0;

    while(lockedCount < (uint32)tileSizeToLock && radius <= maxRadius) {
        bool foundBlock = false;

        while(true) {
            int32 minTileDist = 99999;
            int32 selectedTileX = -1;
            int32 selectedTileY = -1;

            for(int32 tileX = lockPos.x - (int32)radius; tileX <= lockPos.x + (int32)radius; ++tileX) {
                for(int32 tileY = lockPos.y - (int32)radius; tileY <= lockPos.y + (int32)radius; ++tileY) {
                    TileInfo* pCurrentTile = pWorld->GetTile(tileX, tileY);
                    if(!pCurrentTile || pCurrentTile == pLockTile) {
                        continue;
                    }

                    if(std::find(lockedTiles.begin(), lockedTiles.end(), pCurrentTile) != lockedTiles.end()) {
                        continue;
                    }

                    if(!IsLockNeighbour(pWorld, pCurrentTile, pLockTile, ignoreEmpty, airOnly)) {
                        continue;
                    }

                    const int32 tileDist = Abs(tileY - lockPos.y) + Abs(tileX - lockPos.x);
                    if(tileDist >= minTileDist) {
                        continue;
                    }

                    minTileDist = tileDist;
                    selectedTileX = tileX;
                    selectedTileY = tileY;
                }
            }

            if(selectedTileX == -1 && selectedTileY == -1) {
                break;
            }

            foundBlock = true;

            TileInfo* pSelectedTile = pWorld->GetTile(selectedTileX, selectedTileY);
            if(pSelectedTile && pSelectedTile->GetParent() == 0) {
                pSelectedTile->SetParent(lockPos.x + pWorld->GetSize().x * lockPos.y);
                pSelectedTile->SetFlag(TILE_FLAG_HAS_PARENT);
                lockedTiles.push_back(pSelectedTile);
            }

            ++lockedCount;

            if(lockedCount >= (uint32)tileSizeToLock) {
                break;
            }
        }

        if(!foundBlock) {
            pWorld->RemoveTileParentsLockedBy(pLockTile);
            return false;
        }

        ++radius;
    }

    if(lockedCount < (uint32)tileSizeToLock) {
        pWorld->RemoveTileParentsLockedBy(pLockTile);
        return false;
    }

    outTiles = std::move(lockedTiles);
    return true;
}

}