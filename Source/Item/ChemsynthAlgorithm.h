#pragma once

#include "ItemUtils.h"
#include "World/TileExtra.h"
#include <utility>

class GamePlayer;
class TileInfo;
class World;

namespace ChemsynthAlgorithm {
ChemsynthColor GetNextColor(ChemsynthColor color);
ChemsynthColor GetRandomColor();
uint16 ColorToChemicalID(ChemsynthColor color);

void SetTankActive(TileInfo* pTile, bool enable);
bool IsTankActive(TileInfo* pTile);

void CancelChemsynth(World* pWorld, TileInfo* pProcessorTile);
void MoveActiveTank(World* pWorld, TileInfo* pProcessorTile);
uint32 GetPercentFinished(World* pWorld, TileInfo* pProcessorTile);
bool IsChemsynthReady(World* pWorld, TileInfo* pProcessorTile);
std::pair<int32, int32> GetTankRange(World* pWorld, TileInfo* pTankTile);
TileInfo* FindProcessorTile(World* pWorld, TileInfo* pTankTile);
void CheckProgressAndReward(GamePlayer* pPlayer, World* pWorld, TileInfo* pTankTile);
void UseTool(GamePlayer* pPlayer, World* pWorld, TileInfo* pTankTile, uint16 itemID);
void StartChemsynth(GamePlayer* pPlayer, World* pWorld, TileInfo* pProcessorTile);
void UpdateWorldChemsynth(World* pWorld);
}