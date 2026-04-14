#pragma once

#include "../../../Player/GamePlayer.h"
#include "../../../World/World.h"

class BlockActivate {
public:
    static void Execute(GamePlayer* pPlayer, World* pWorld, GameUpdatePacket* pPacket);
};