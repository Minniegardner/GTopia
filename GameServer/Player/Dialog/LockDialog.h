#pragma once
#include "Packet/PacketUtils.h"

class GamePlayer;
class TileInfo;

class LockDialog {
public:
    static void Request(GamePlayer* pPlayer, TileInfo* pTile);
};