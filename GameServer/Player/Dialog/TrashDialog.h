#pragma once

#include "Precompiled.h"

class GamePlayer;

class TrashDialog {
public:
    static void Request(GamePlayer* pPlayer, uint16 itemID);
    static void Handle(GamePlayer* pPlayer, uint16 itemID, int16 itemCount);
    static void HandleUntradeable(GamePlayer* pPlayer, uint16 itemID, int16 itemCount);
};