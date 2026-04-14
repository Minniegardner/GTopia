#pragma once

#include "../../../Player/GamePlayer.h"

class MenuClickSecure {
public:
    static void Execute(GamePlayer* pPlayer, ParsedTextPacket<8>& packet);
};