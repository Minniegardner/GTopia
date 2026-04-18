#pragma once

#include "../Precompiled.h"
#include "../Math/Vector2.h"

class GamePlayer;
class World;
class TileInfo;

namespace GhostAlgorithm {
bool PlaceGhostJar(GamePlayer* pPlayer, World* pWorld, TileInfo* pTile);
bool SpawnGhostAt(World* pWorld, const Vector2Float& worldPos);
bool SpawnGhostNearPlayer(GamePlayer* pPlayer, World* pWorld);
void PullGhostsTowardPlayer(World* pWorld, const Vector2Float& playerPos, const Vector2Float& beamTilePos);
void ClearWorldGhosts(World* pWorld);
void DestroyWorldState(uint32 worldID);
bool HasWorldGhosts(World* pWorld);
void UpdateWorldGhosts(World* pWorld);
}
