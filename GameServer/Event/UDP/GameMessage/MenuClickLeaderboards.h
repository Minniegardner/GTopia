#pragma once

#include "../../../Player/GamePlayer.h"

class MenuClickLeaderboards {
public:
    static void Execute(GamePlayer* pPlayer, ParsedTextPacket<8>& packet);
};