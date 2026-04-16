#pragma once

#include "Packet/PacketUtils.h"
#include "../../../Player/GamePlayer.h"

class Input {
public:
    static void Execute(GamePlayer* pPlayer, ParsedTextPacket<8>& packet);
};