#pragma once

#include "../../../Player/GamePlayer.h"
#include "../../../World/World.h"

class PlayerCollect {
public:
    static void Execute(GamePlayer* pPlayer, World* pWorld, GameUpdatePacket* pPacket);
};