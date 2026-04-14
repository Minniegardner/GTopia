#pragma once

#include "../../../Player/GamePlayer.h"

class PlayerCollect {
public:
    static void Execute(GamePlayer* pPlayer, World* pWorld, GameUpdatePacket* pPacket);
};