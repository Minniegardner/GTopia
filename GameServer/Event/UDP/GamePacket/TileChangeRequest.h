#pragma once

#include "../../../Player/GamePlayer.h"
#include "../../../World/World.h"
#include "Packet/NetPacket.h"

class TileChangeRequest {
public:
    static void OnPunchedLock(GamePlayer* pPlayer, TileInfo* pTile);
    static void HandleConsumable(GamePlayer* pPlayer, World* pWorld, GameUpdatePacket* pPacket);
    static void Execute(GamePlayer* pPlayer, World* pWorld, GameUpdatePacket* pPacket);
};