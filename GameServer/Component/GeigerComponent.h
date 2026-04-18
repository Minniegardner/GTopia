#pragma once

#include "Precompiled.h"

class GamePlayer;
class World;
class TileInfo;
class ItemInfo;

class GeigerComponent {
public:
    static void RequestChargerDialog(GamePlayer* pPlayer, TileInfo* pTile);
    static bool TryChargeCounter(GamePlayer* pPlayer, World* pWorld, TileInfo* pTile);
    static void UpdatePlayerScan(GamePlayer* pPlayer, World* pWorld);
    static void TrySpawnRadioactiveDrop(GamePlayer* pPlayer, World* pWorld, TileInfo* pTile, const ItemInfo* pBrokenItem);
};
