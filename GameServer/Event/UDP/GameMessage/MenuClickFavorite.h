#pragma once

#include "../../../Player/GamePlayer.h"

class MenuClickFavorite {
public:
    static void Execute(GamePlayer* pPlayer, ParsedTextPacket<8>& packet);
};