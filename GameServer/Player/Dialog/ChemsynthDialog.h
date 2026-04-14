#pragma once

#include "../GamePlayer.h"

class TileInfo;
template<uint8 N> struct ParsedTextPacket;

namespace ChemsynthDialog {
void Request(GamePlayer* pPlayer, TileInfo* pTile);
void Handle(GamePlayer* pPlayer, ParsedTextPacket<8>& packet);
}