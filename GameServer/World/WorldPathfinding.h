#pragma once

#include "Precompiled.h"

class World;
class GamePlayer;

namespace WorldPathfinding {

bool HasPath(World* pWorld, GamePlayer* pPlayer, const Vector2Float& currentPos, const Vector2Float& futurePos);

}
