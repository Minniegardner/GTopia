#pragma once

#include "../../../Player/GamePlayer.h"

class PlayerPunched {
public:
    static void Execute(GamePlayer* pPlayer, World* pWorld, GameUpdatePacket* pPacket);
};