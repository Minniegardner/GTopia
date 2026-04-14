#pragma once

#include "Packet/PacketUtils.h"
#include "../../../Player/GamePlayer.h"

class MenuClickSocial {
public:
    static void Execute(GamePlayer* pPlayer, ParsedTextPacket<8>& packet);
};