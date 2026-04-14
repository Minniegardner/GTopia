#pragma once

#include "../../../Player/GamePlayer.h"

class BlockActivate {
public:
    static void Execute(GamePlayer* pPlayer, World* pWorld, GameUpdatePacket* pPacket);
};