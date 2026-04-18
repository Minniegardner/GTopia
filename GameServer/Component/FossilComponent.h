#pragma once

#include "Precompiled.h"
#include "Packet/PacketUtils.h"

class GamePlayer;
class World;
class TileInfo;
class ItemInfo;

class FossilComponent {
public:
    static bool IsFossilRockItem(uint16 itemID);
    static void OnFossilRockPunched(GamePlayer* pPlayer, World* pWorld, TileInfo* pTile, const ItemInfo* pTileItem);
    static void RequestPrepDialog(GamePlayer* pPlayer, TileInfo* pTile);
    static bool TryHandlePrepAction(GamePlayer* pPlayer, World* pWorld, TileInfo* pTile);
};
