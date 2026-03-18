#pragma once
#include "Packet/PacketUtils.h"

class GamePlayer;
class TileInfo;

class PlayerDialog {
public:
    static void Handle(GamePlayer* pPlayer, TileInfo* pTile);
};