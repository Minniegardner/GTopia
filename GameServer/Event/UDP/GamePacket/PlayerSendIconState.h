#pragma once

#include "../../../Player/GamePlayer.h"

class PlayerSendIconState {
public:
    static void Execute(GamePlayer* pPlayer, World* pWorld, GameUpdatePacket* pPacket);
};