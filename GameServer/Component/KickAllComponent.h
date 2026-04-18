#pragma once

#include "Precompiled.h"

class GamePlayer;
class World;

class KickAllComponent {
public:
    static bool Execute(GamePlayer* pInvoker, World* pWorld, uint32& outAffected, string& outMessage);
};
