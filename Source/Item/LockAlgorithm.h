#pragma once

#include "ItemUtils.h"
#include <vector>

class WorldTileManager;
class TileInfo;

bool IsWorldLock(uint16 itemID);
bool IsAreaLock(uint16 itemID);
uint16 GetMaxTilesToLock(uint16 itemID);

namespace LockAlgorithm {
	bool ApplyLockTiles(WorldTileManager* pWorld, TileInfo* pLockTile, int32 tileSizeToLock, bool ignoreEmpty, std::vector<TileInfo*>& outTiles, bool airOnly = false);
}