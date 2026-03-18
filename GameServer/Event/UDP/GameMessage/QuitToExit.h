#pragma once

#include "../../../Player/GamePlayer.h"

class QuitToExit {
public:
    static void Execute(GamePlayer* pPlayer, ParsedTextPacket<8>& packet);
};