#pragma once

#include "Precompiled.h"
#include "Packet/PacketUtils.h"

class GamePlayer;
class TileInfo;

class MachineDialog {
public:
    static void Request(GamePlayer* pPlayer, TileInfo* pTile);
    static void Handle(GamePlayer* pPlayer, ParsedTextPacket<8>& packet);
};
