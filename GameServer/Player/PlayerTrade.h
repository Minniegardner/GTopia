#pragma once

#include "Precompiled.h"

class GamePlayer;

class PlayerTrade {
public:
    PlayerTrade(GamePlayer* pPlayer);
    ~PlayerTrade();

public:
    GamePlayer* m_pTradingPlayer;
    GamePlayer* m_pPlayer;
};