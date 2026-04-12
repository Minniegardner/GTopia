#pragma once

#include "Precompiled.h"

class GamePlayer;

class DropDialog {
public:
    static void Request(GamePlayer* pPlayer, uint16 itemID);
    static void Handle(GamePlayer* pPlayer, uint16 itemID, int16 itemCount);
};