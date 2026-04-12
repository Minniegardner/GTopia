#pragma once

#include "../../../Player/GamePlayer.h"

class StoreNavigate {
public:
    static void Execute(GamePlayer* pPlayer, ParsedTextPacket<8>& packet);
};
