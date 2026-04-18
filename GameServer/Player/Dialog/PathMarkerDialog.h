#pragma once

#include "Precompiled.h"
#include "World/TileInfo.h"

class GamePlayer;

class PathMarkerDialog {
public:
    static void Request(GamePlayer* pPlayer, TileInfo* pTile);
    static void Handle(GamePlayer* pPlayer, const string& text, int32 tileX, int32 tileY);
};